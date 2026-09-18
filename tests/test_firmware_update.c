#include "../firmware/update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLOT_SIZE 4096u

typedef struct {
    unsigned char slots[2][SLOT_SIZE];
    int fail_write;
    int corrupt_after_write;
    unsigned int erase_calls;
    unsigned int write_calls;
} mock_flash_t;

static int erase_slot(void *context, uint32_t slot) {
    mock_flash_t *flash = context;
    if (!flash || slot >= 2u) return -1;
    memset(flash->slots[slot], 0xff, SLOT_SIZE);
    flash->erase_calls++;
    return 0;
}

static int write_slot(void *context, uint32_t slot, uint32_t offset,
                      const void *data, uint32_t length) {
    mock_flash_t *flash = context;
    if (!flash || slot >= 2u || offset > SLOT_SIZE ||
        length > SLOT_SIZE - offset) return -1;
    flash->write_calls++;
    if (flash->fail_write) return -1;
    memcpy(flash->slots[slot] + offset, data, length);
    if (flash->corrupt_after_write && length != 0u) {
        flash->slots[slot][offset] ^= 0x01u;
    }
    return 0;
}

static int read_slot(void *context, uint32_t slot, uint32_t offset,
                     void *data, uint32_t length) {
    mock_flash_t *flash = context;
    if (!flash || !data || slot >= 2u || offset > SLOT_SIZE ||
        length > SLOT_SIZE - offset) return -1;
    memcpy(data, flash->slots[slot] + offset, length);
    return 0;
}

static josh_fw_update_manifest_t manifest_for(
    const char *board,
    const unsigned char *image,
    uint32_t size,
    uint64_t generation
) {
    josh_fw_update_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));
    manifest.magic = JOSH_FW_UPDATE_MAGIC;
    manifest.version = JOSH_FW_UPDATE_VERSION;
    manifest.header_size = sizeof(manifest);
    strncpy(manifest.board_id, board, JOSH_FW_BOARD_ID_MAX);
    manifest.generation = generation;
    manifest.image_size = size;
    manifest.image_crc32 = josh_fw_crc32(image, size);
    return manifest;
}

int main(void) {
    static const unsigned char image[] =
        "JoshFirmware inactive-slot integration fixture";

    mock_flash_t flash;
    memset(&flash, 0xa5, sizeof(flash));
    flash.fail_write = 0;
    flash.corrupt_after_write = 0;
    flash.erase_calls = flash.write_calls = 0;

    josh_fw_flash_backend_t backend = {
        .context = &flash,
        .slot_count = 2,
        .slot_size = SLOT_SIZE,
        .erase_slot = erase_slot,
        .write_slot = write_slot,
        .read_slot = read_slot
    };

    josh_fw_update_state_t state;
    josh_fw_update_state_default(&state);
    state.active_generation = 7u;

    josh_fw_update_manifest_t manifest =
        manifest_for("qemu-x86_64", image, sizeof(image), 8u);

    unsigned char original_active[SLOT_SIZE];
    memcpy(original_active, flash.slots[0], SLOT_SIZE);

    assert(josh_fw_stage_update(
        &manifest, image, "wrong-board", &backend, &state, 0) ==
        JOSH_FW_UPDATE_WRONG_BOARD);
    assert(flash.erase_calls == 0u);

    josh_fw_update_manifest_t old = manifest;
    old.generation = 7u;
    assert(josh_fw_stage_update(
        &old, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_ROLLBACK_BLOCKED);
    assert(flash.erase_calls == 0u);

    josh_fw_update_manifest_t corrupt = manifest;
    corrupt.image_crc32 ^= 1u;
    assert(josh_fw_stage_update(
        &corrupt, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_IMAGE_CORRUPT);
    assert(flash.erase_calls == 0u);

    flash.fail_write = 1;
    assert(josh_fw_stage_update(
        &manifest, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_WRITE_FAILED);
    assert(state.pending_slot == JOSH_FW_SLOT_NONE);
    assert(memcmp(flash.slots[0], original_active, SLOT_SIZE) == 0);
    flash.fail_write = 0;

    flash.corrupt_after_write = 1;
    assert(josh_fw_stage_update(
        &manifest, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_VERIFY_FAILED);
    assert(state.pending_slot == JOSH_FW_SLOT_NONE);
    flash.corrupt_after_write = 0;

    assert(josh_fw_stage_update(
        &manifest, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_OK);
    assert(state.active_slot == 0u);
    assert(state.pending_slot == 1u);
    assert(memcmp(flash.slots[0], original_active, SLOT_SIZE) == 0);
    assert(memcmp(flash.slots[1], image, sizeof(image)) == 0);

    assert(josh_fw_select_boot_slot(&state, 3u) == 1u);
    assert(josh_fw_select_boot_slot(&state, 3u) == 1u);
    assert(josh_fw_select_boot_slot(&state, 3u) == 1u);
    assert(josh_fw_select_boot_slot(&state, 3u) == 0u);
    assert(state.pending_slot == JOSH_FW_SLOT_NONE);
    assert(state.active_slot == 0u);

    assert(josh_fw_stage_update(
        &manifest, image, "qemu-x86_64", &backend, &state, 0) ==
        JOSH_FW_UPDATE_OK);
    assert(josh_fw_select_boot_slot(&state, 3u) == 1u);
    assert(josh_fw_mark_boot_good(&state, 1u));
    assert(state.active_slot == 1u);
    assert(state.previous_good_slot == 0u);
    assert(state.active_generation == 8u);
    assert(state.pending_slot == JOSH_FW_SLOT_NONE);

    puts("JoshFirmware update-engine tests passed");
    return 0;
}
