#include "../boot/core/health.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    josh_boot_health_record_t record;
    josh_boot_health_default(&record);
    assert(josh_boot_health_valid(&record));
    assert(record.selected_slot == JOSH_BOOT_SLOT_CURRENT);

    josh_boot_health_record_t corrupt = record;
    corrupt.attempt_count = 9;
    assert(!josh_boot_health_valid(&corrupt));

    assert(josh_boot_health_mark_good(&record, JOSH_BOOT_SLOT_PREVIOUS));
    assert(josh_boot_health_begin_update(&record, JOSH_BOOT_SLOT_CURRENT));
    assert(josh_boot_health_prepare_attempt(&record, 3) == JOSH_BOOT_SLOT_CURRENT);
    assert(josh_boot_health_prepare_attempt(&record, 3) == JOSH_BOOT_SLOT_CURRENT);
    assert(josh_boot_health_prepare_attempt(&record, 3) == JOSH_BOOT_SLOT_CURRENT);
    assert(record.attempt_count == 3);
    assert(josh_boot_health_prepare_attempt(&record, 3) == JOSH_BOOT_SLOT_PREVIOUS);
    assert(record.pending_good == 0);
    assert(record.last_failure_stage == JOSH_BOOT_FAILURE_KERNEL);

    josh_boot_health_record_t a, b, chosen;
    josh_boot_health_default(&a);
    josh_boot_health_default(&b);
    b.generation = 5;
    josh_boot_health_seal(&b);
    assert(josh_boot_health_choose(&a, &b, &chosen));
    assert(chosen.generation == 5);

    b.checksum ^= 1u;
    assert(josh_boot_health_choose(&a, &b, &chosen));
    assert(chosen.generation == 0);

    josh_boot_health_default(&record);
    record.last_good_slot = JOSH_BOOT_SLOT_CURRENT;
    record.selected_slot = JOSH_BOOT_SLOT_CURRENT;
    record.pending_good = 1;
    record.attempt_count = 3;
    josh_boot_health_seal(&record);
    assert(josh_boot_health_prepare_attempt(&record, 3) == JOSH_BOOT_SLOT_RECOVERY);

    puts("JoshBootloader boot-health tests passed");
    return 0;
}
