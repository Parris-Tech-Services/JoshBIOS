#ifndef JOSHBOOT_HEALTH_H
#define JOSHBOOT_HEALTH_H
#include <stddef.h>
#include <stdint.h>

#define JOSH_BOOT_HEALTH_MAGIC UINT32_C(0x48424a53)
#define JOSH_BOOT_HEALTH_VERSION 1u
#define JOSH_BOOT_HEALTH_MAX_ATTEMPTS 3u

typedef enum {
    JOSH_BOOT_SLOT_CURRENT=0,
    JOSH_BOOT_SLOT_PREVIOUS=1,
    JOSH_BOOT_SLOT_RECOVERY=2
} josh_boot_slot_t;

typedef enum {
    JOSH_BOOT_FAILURE_NONE=0,
    JOSH_BOOT_FAILURE_CONFIG=1,
    JOSH_BOOT_FAILURE_KERNEL_FILE=2,
    JOSH_BOOT_FAILURE_ELF=3,
    JOSH_BOOT_FAILURE_HANDOFF=4,
    JOSH_BOOT_FAILURE_KERNEL=5,
    JOSH_BOOT_FAILURE_USERSPACE=6,
    JOSH_BOOT_FAILURE_DESKTOP=7
} josh_boot_failure_stage_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint64_t generation;
    uint32_t selected_slot;
    uint32_t last_good_slot;
    uint32_t pending_good;
    uint32_t attempt_count;
    uint32_t last_failure_stage;
    uint32_t reserved0;
    uint64_t update_generation;
    uint32_t reserved1;
    uint32_t checksum;
} josh_boot_health_record_t;

void josh_boot_health_default(josh_boot_health_record_t *record);
int josh_boot_health_valid(const josh_boot_health_record_t *record);
void josh_boot_health_seal(josh_boot_health_record_t *record);
int josh_boot_health_choose(const josh_boot_health_record_t *a,
                            const josh_boot_health_record_t *b,
                            josh_boot_health_record_t *out);
int josh_boot_health_begin_update(josh_boot_health_record_t *record,
                                  josh_boot_slot_t candidate);
josh_boot_slot_t josh_boot_health_prepare_attempt(
    josh_boot_health_record_t *record,
    uint32_t max_attempts);
int josh_boot_health_mark_good(josh_boot_health_record_t *record,
                               josh_boot_slot_t slot);
void josh_boot_health_note_failure(josh_boot_health_record_t *record,
                                   josh_boot_failure_stage_t stage);
#endif
