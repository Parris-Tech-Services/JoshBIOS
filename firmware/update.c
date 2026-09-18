#include "update.h"

#define VERIFY_CHUNK 256u

static int text_equal(const char *a, const char *b, size_t limit) {
    if (!a || !b) return 0;
    for (size_t i = 0; i < limit; ++i) {
        if (a[i] != b[i]) return 0;
        if (a[i] == '\0') return 1;
    }
    return 0;
}

uint32_t josh_fw_crc32(const void *data, size_t length) {
    if (!data && length != 0u) return 0u;
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xffffffff);
    while (length--) {
        crc ^= *bytes++;
        for (unsigned int bit = 0; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

void josh_fw_update_state_default(josh_fw_update_state_t *state) {
    if (!state) return;
    state->active_slot = 0u;
    state->previous_good_slot = 0u;
    state->pending_slot = JOSH_FW_SLOT_NONE;
    state->pending_attempts = 0u;
    state->active_generation = 0u;
    state->pending_generation = 0u;
}

static int manifest_valid(const josh_fw_update_manifest_t *manifest) {
    if (!manifest ||
        manifest->magic != JOSH_FW_UPDATE_MAGIC ||
        manifest->version != JOSH_FW_UPDATE_VERSION ||
        manifest->header_size != sizeof(*manifest) ||
        manifest->image_size == 0u) {
        return 0;
    }
    int terminated = 0;
    for (size_t i = 0; i <= JOSH_FW_BOARD_ID_MAX; ++i) {
        unsigned char c = (unsigned char)manifest->board_id[i];
        if (c == '\0') {
            terminated = i != 0u;
            break;
        }
        if (c < 0x21u || c > 0x7eu) return 0;
    }
    return terminated;
}

static int state_valid(
    const josh_fw_update_state_t *state,
    const josh_fw_flash_backend_t *backend
) {
    if (!state || !backend || backend->slot_count < 2u) return 0;
    if (state->active_slot >= backend->slot_count ||
        state->previous_good_slot >= backend->slot_count) return 0;
    if (state->pending_slot != JOSH_FW_SLOT_NONE &&
        state->pending_slot >= backend->slot_count) return 0;
    return 1;
}

josh_fw_update_status_t josh_fw_stage_update(
    const josh_fw_update_manifest_t *manifest,
    const void *image,
    const char *expected_board_id,
    const josh_fw_flash_backend_t *backend,
    josh_fw_update_state_t *state,
    int allow_rollback
) {
    if (!image || !expected_board_id || !backend || !state ||
        !backend->erase_slot || !backend->write_slot || !backend->read_slot) {
        return JOSH_FW_UPDATE_INVALID_ARGUMENT;
    }
    if (!manifest_valid(manifest) || !state_valid(state, backend)) {
        return JOSH_FW_UPDATE_BAD_MANIFEST;
    }
    if (!text_equal(manifest->board_id, expected_board_id,
                    JOSH_FW_BOARD_ID_MAX + 1u)) {
        return JOSH_FW_UPDATE_WRONG_BOARD;
    }
    if (!allow_rollback && manifest->generation <= state->active_generation) {
        return JOSH_FW_UPDATE_ROLLBACK_BLOCKED;
    }
    if (manifest->image_size > backend->slot_size) {
        return JOSH_FW_UPDATE_IMAGE_TOO_LARGE;
    }
    if (josh_fw_crc32(image, manifest->image_size) != manifest->image_crc32) {
        return JOSH_FW_UPDATE_IMAGE_CORRUPT;
    }

    uint32_t target = (state->active_slot + 1u) % backend->slot_count;
    if (target == state->active_slot) return JOSH_FW_UPDATE_BAD_MANIFEST;

    if (backend->erase_slot(backend->context, target) != 0) {
        return JOSH_FW_UPDATE_ERASE_FAILED;
    }
    if (backend->write_slot(
            backend->context, target, 0u, image, manifest->image_size) != 0) {
        return JOSH_FW_UPDATE_WRITE_FAILED;
    }

    uint8_t verify[VERIFY_CHUNK];
    const uint8_t *source = (const uint8_t *)image;
    uint32_t offset = 0u;
    while (offset < manifest->image_size) {
        uint32_t remaining = manifest->image_size - offset;
        uint32_t length = remaining < VERIFY_CHUNK ? remaining : VERIFY_CHUNK;
        if (backend->read_slot(
                backend->context, target, offset, verify, length) != 0) {
            return JOSH_FW_UPDATE_VERIFY_FAILED;
        }
        for (uint32_t i = 0; i < length; ++i) {
            if (verify[i] != source[offset + i]) {
                return JOSH_FW_UPDATE_VERIFY_FAILED;
            }
        }
        offset += length;
    }

    state->previous_good_slot = state->active_slot;
    state->pending_slot = target;
    state->pending_generation = manifest->generation;
    state->pending_attempts = 0u;
    return JOSH_FW_UPDATE_OK;
}

uint32_t josh_fw_select_boot_slot(
    josh_fw_update_state_t *state,
    uint32_t max_attempts
) {
    if (!state) return JOSH_FW_SLOT_NONE;
    if (max_attempts == 0u) max_attempts = JOSH_FW_MAX_ATTEMPTS;
    if (state->pending_slot == JOSH_FW_SLOT_NONE) return state->active_slot;

    if (state->pending_attempts >= max_attempts) {
        state->pending_slot = JOSH_FW_SLOT_NONE;
        state->pending_generation = 0u;
        state->pending_attempts = 0u;
        return state->active_slot;
    }

    state->pending_attempts++;
    return state->pending_slot;
}

int josh_fw_mark_boot_good(josh_fw_update_state_t *state, uint32_t slot) {
    if (!state || state->pending_slot == JOSH_FW_SLOT_NONE ||
        slot != state->pending_slot) {
        return 0;
    }
    state->previous_good_slot = state->active_slot;
    state->active_slot = state->pending_slot;
    state->active_generation = state->pending_generation;
    state->pending_slot = JOSH_FW_SLOT_NONE;
    state->pending_generation = 0u;
    state->pending_attempts = 0u;
    return 1;
}

const char *josh_fw_update_status_string(josh_fw_update_status_t status) {
    switch (status) {
        case JOSH_FW_UPDATE_OK: return "ok";
        case JOSH_FW_UPDATE_INVALID_ARGUMENT: return "invalid argument";
        case JOSH_FW_UPDATE_BAD_MANIFEST: return "bad manifest";
        case JOSH_FW_UPDATE_WRONG_BOARD: return "wrong board";
        case JOSH_FW_UPDATE_ROLLBACK_BLOCKED: return "rollback blocked";
        case JOSH_FW_UPDATE_IMAGE_TOO_LARGE: return "image too large";
        case JOSH_FW_UPDATE_IMAGE_CORRUPT: return "image integrity mismatch";
        case JOSH_FW_UPDATE_ERASE_FAILED: return "erase failed";
        case JOSH_FW_UPDATE_WRITE_FAILED: return "write failed";
        case JOSH_FW_UPDATE_VERIFY_FAILED: return "read-back verify failed";
        default: return "unknown update error";
    }
}
