#ifndef JOSH_FIRMWARE_UPDATE_H
#define JOSH_FIRMWARE_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#define JOSH_FW_UPDATE_MAGIC UINT32_C(0x5557464a)
#define JOSH_FW_UPDATE_VERSION 1u
#define JOSH_FW_BOARD_ID_MAX 31u
#define JOSH_FW_SLOT_NONE UINT32_MAX
#define JOSH_FW_MAX_ATTEMPTS 3u

typedef enum {
    JOSH_FW_UPDATE_OK = 0,
    JOSH_FW_UPDATE_INVALID_ARGUMENT,
    JOSH_FW_UPDATE_BAD_MANIFEST,
    JOSH_FW_UPDATE_WRONG_BOARD,
    JOSH_FW_UPDATE_ROLLBACK_BLOCKED,
    JOSH_FW_UPDATE_IMAGE_TOO_LARGE,
    JOSH_FW_UPDATE_IMAGE_CORRUPT,
    JOSH_FW_UPDATE_ERASE_FAILED,
    JOSH_FW_UPDATE_WRITE_FAILED,
    JOSH_FW_UPDATE_VERIFY_FAILED
} josh_fw_update_status_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    char board_id[JOSH_FW_BOARD_ID_MAX + 1u];
    uint64_t generation;
    uint32_t image_size;
    uint32_t image_crc32;
} josh_fw_update_manifest_t;

typedef struct {
    uint32_t active_slot;
    uint32_t previous_good_slot;
    uint32_t pending_slot;
    uint32_t pending_attempts;
    uint64_t active_generation;
    uint64_t pending_generation;
} josh_fw_update_state_t;

typedef struct {
    void *context;
    uint32_t slot_count;
    uint32_t slot_size;
    int (*erase_slot)(void *context, uint32_t slot);
    int (*write_slot)(void *context, uint32_t slot, uint32_t offset,
                      const void *data, uint32_t length);
    int (*read_slot)(void *context, uint32_t slot, uint32_t offset,
                     void *data, uint32_t length);
} josh_fw_flash_backend_t;

uint32_t josh_fw_crc32(const void *data, size_t length);
void josh_fw_update_state_default(josh_fw_update_state_t *state);
josh_fw_update_status_t josh_fw_stage_update(
    const josh_fw_update_manifest_t *manifest,
    const void *image,
    const char *expected_board_id,
    const josh_fw_flash_backend_t *backend,
    josh_fw_update_state_t *state,
    int allow_rollback);
uint32_t josh_fw_select_boot_slot(
    josh_fw_update_state_t *state,
    uint32_t max_attempts);
int josh_fw_mark_boot_good(josh_fw_update_state_t *state, uint32_t slot);
const char *josh_fw_update_status_string(josh_fw_update_status_t status);

#endif
