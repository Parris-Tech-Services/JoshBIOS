#include "health.h"

static int slot_valid(uint32_t slot) {
    return slot <= JOSH_BOOT_SLOT_RECOVERY;
}

static uint32_t crc32_bytes(const unsigned char *data, size_t length) {
    uint32_t crc = UINT32_C(0xffffffff);
    while (length--) {
        crc ^= *data++;
        for (unsigned int i = 0; i < 8u; ++i) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static uint32_t checksum_for(const josh_boot_health_record_t *record) {
    return crc32_bytes(
        (const unsigned char *)record,
        offsetof(josh_boot_health_record_t, checksum)
    );
}

void josh_boot_health_seal(josh_boot_health_record_t *record) {
    if (record) record->checksum = checksum_for(record);
}

void josh_boot_health_default(josh_boot_health_record_t *record) {
    if (!record) return;
    unsigned char *bytes = (unsigned char *)record;
    for (size_t i = 0; i < sizeof(*record); ++i) bytes[i] = 0;
    record->magic = JOSH_BOOT_HEALTH_MAGIC;
    record->version = JOSH_BOOT_HEALTH_VERSION;
    record->size = sizeof(*record);
    record->selected_slot = JOSH_BOOT_SLOT_CURRENT;
    record->last_good_slot = JOSH_BOOT_SLOT_CURRENT;
    josh_boot_health_seal(record);
}

int josh_boot_health_valid(const josh_boot_health_record_t *record) {
    if (!record ||
        record->magic != JOSH_BOOT_HEALTH_MAGIC ||
        record->version != JOSH_BOOT_HEALTH_VERSION ||
        record->size != sizeof(*record) ||
        !slot_valid(record->selected_slot) ||
        !slot_valid(record->last_good_slot) ||
        record->pending_good > 1u ||
        record->last_failure_stage > JOSH_BOOT_FAILURE_DESKTOP) {
        return 0;
    }
    return record->checksum == checksum_for(record);
}

int josh_boot_health_choose(
    const josh_boot_health_record_t *a,
    const josh_boot_health_record_t *b,
    josh_boot_health_record_t *out
) {
    if (!out) return 0;
    int a_valid = josh_boot_health_valid(a);
    int b_valid = josh_boot_health_valid(b);
    if (!a_valid && !b_valid) {
        josh_boot_health_default(out);
        return 0;
    }
    const josh_boot_health_record_t *chosen =
        a_valid && (!b_valid || a->generation >= b->generation) ? a : b;
    *out = *chosen;
    return 1;
}

int josh_boot_health_begin_update(
    josh_boot_health_record_t *record,
    josh_boot_slot_t candidate
) {
    if (!record || !josh_boot_health_valid(record) ||
        !slot_valid(candidate) || candidate == JOSH_BOOT_SLOT_RECOVERY) {
        return 0;
    }
    record->generation++;
    record->update_generation++;
    record->selected_slot = candidate;
    record->pending_good = 1u;
    record->attempt_count = 0;
    record->last_failure_stage = JOSH_BOOT_FAILURE_NONE;
    josh_boot_health_seal(record);
    return 1;
}

josh_boot_slot_t josh_boot_health_prepare_attempt(
    josh_boot_health_record_t *record,
    uint32_t max_attempts
) {
    if (!record || !josh_boot_health_valid(record)) {
        return JOSH_BOOT_SLOT_RECOVERY;
    }
    if (max_attempts == 0u) max_attempts = JOSH_BOOT_HEALTH_MAX_ATTEMPTS;
    if (record->pending_good) {
        record->generation++;
        if (record->attempt_count >= max_attempts) {
            record->selected_slot =
                record->last_good_slot != record->selected_slot
                    ? record->last_good_slot
                    : JOSH_BOOT_SLOT_RECOVERY;
            record->pending_good = 0u;
            record->attempt_count = 0;
            record->last_failure_stage = JOSH_BOOT_FAILURE_KERNEL;
        } else {
            record->attempt_count++;
        }
        josh_boot_health_seal(record);
    }
    return (josh_boot_slot_t)record->selected_slot;
}

int josh_boot_health_mark_good(
    josh_boot_health_record_t *record,
    josh_boot_slot_t slot
) {
    if (!record || !josh_boot_health_valid(record) || !slot_valid(slot)) {
        return 0;
    }
    record->generation++;
    record->selected_slot = slot;
    record->last_good_slot = slot;
    record->pending_good = 0u;
    record->attempt_count = 0;
    record->last_failure_stage = JOSH_BOOT_FAILURE_NONE;
    josh_boot_health_seal(record);
    return 1;
}

void josh_boot_health_note_failure(
    josh_boot_health_record_t *record,
    josh_boot_failure_stage_t stage
) {
    if (!record || !josh_boot_health_valid(record) ||
        stage > JOSH_BOOT_FAILURE_DESKTOP) {
        return;
    }
    record->generation++;
    record->last_failure_stage = (uint32_t)stage;
    josh_boot_health_seal(record);
}
