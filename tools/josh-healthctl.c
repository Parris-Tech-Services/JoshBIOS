#include "../boot/core/health.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_record(FILE *file, uint64_t lba, josh_boot_health_record_t *record) {
    unsigned char sector[JOSH_BOOT_HEALTH_SECTOR_SIZE];
    if (fseek(file, (long)(lba * JOSH_BOOT_HEALTH_SECTOR_SIZE), SEEK_SET) != 0) return 0;
    if (fread(sector, 1, sizeof(sector), file) != sizeof(sector)) return 0;
    memcpy(record, sector, sizeof(*record));
    return 1;
}

static int write_record(FILE *file, uint64_t lba, const josh_boot_health_record_t *record) {
    unsigned char sector[JOSH_BOOT_HEALTH_SECTOR_SIZE] = {0};
    memcpy(sector, record, sizeof(*record));
    if (fseek(file, (long)(lba * JOSH_BOOT_HEALTH_SECTOR_SIZE), SEEK_SET) != 0) return 0;
    if (fwrite(sector, 1, sizeof(sector), file) != sizeof(sector)) return 0;
    return fflush(file) == 0;
}

static int load_state(FILE *file, josh_boot_health_record_t *record) {
    josh_boot_health_record_t a, b;
    int have_a = read_record(file, JOSH_BOOT_HEALTH_LBA_A, &a);
    int have_b = read_record(file, JOSH_BOOT_HEALTH_LBA_B, &b);
    if (!have_a) memset(&a, 0, sizeof(a));
    if (!have_b) memset(&b, 0, sizeof(b));
    return josh_boot_health_choose(&a, &b, record);
}

static int persist(FILE *file, const josh_boot_health_record_t *record) {
    uint64_t lba = (record->generation & 1u)
        ? JOSH_BOOT_HEALTH_LBA_B : JOSH_BOOT_HEALTH_LBA_A;
    return write_record(file, lba, record);
}

static int parse_slot(const char *name, josh_boot_slot_t *slot) {
    if (strcmp(name, "current") == 0) *slot = JOSH_BOOT_SLOT_CURRENT;
    else if (strcmp(name, "previous") == 0) *slot = JOSH_BOOT_SLOT_PREVIOUS;
    else if (strcmp(name, "recovery") == 0) *slot = JOSH_BOOT_SLOT_RECOVERY;
    else return 0;
    return 1;
}

static const char *slot_name(uint32_t slot) {
    switch (slot) {
        case JOSH_BOOT_SLOT_CURRENT: return "current";
        case JOSH_BOOT_SLOT_PREVIOUS: return "previous";
        case JOSH_BOOT_SLOT_RECOVERY: return "recovery";
        default: return "invalid";
    }
}

static void show(const josh_boot_health_record_t *record) {
    printf("generation=%llu\n", (unsigned long long)record->generation);
    printf("selected=%s\n", slot_name(record->selected_slot));
    printf("last_good=%s\n", slot_name(record->last_good_slot));
    printf("pending_good=%u\n", record->pending_good);
    printf("attempt_count=%u\n", record->attempt_count);
    printf("last_failure_stage=%u\n", record->last_failure_stage);
    printf("update_generation=%llu\n",
           (unsigned long long)record->update_generation);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "usage: %s <show|init|good|pending|attempt> <image> [slot]\n",
            argv[0]);
        return 2;
    }
    FILE *file = fopen(argv[2], "r+b");
    if (!file) { perror(argv[2]); return 1; }

    josh_boot_health_record_t record;
    int existed = load_state(file, &record);

    if (strcmp(argv[1], "init") == 0) {
        josh_boot_health_default(&record);
        if (!write_record(file, JOSH_BOOT_HEALTH_LBA_A, &record) ||
            !write_record(file, JOSH_BOOT_HEALTH_LBA_B, &record)) {
            fclose(file); return 1;
        }
    } else if (strcmp(argv[1], "show") == 0) {
        if (!existed) {
            fprintf(stderr, "no valid boot-health record\n");
            fclose(file); return 1;
        }
        show(&record);
        fclose(file);
        return 0;
    } else if (strcmp(argv[1], "good") == 0 ||
               strcmp(argv[1], "pending") == 0) {
        if (argc != 4) { fclose(file); return 2; }
        josh_boot_slot_t slot;
        if (!parse_slot(argv[3], &slot)) { fclose(file); return 2; }
        if (!existed) josh_boot_health_default(&record);
        int ok = strcmp(argv[1], "good") == 0
            ? josh_boot_health_mark_good(&record, slot)
            : josh_boot_health_begin_update(&record, slot);
        if (!ok || !persist(file, &record)) { fclose(file); return 1; }
    } else if (strcmp(argv[1], "attempt") == 0) {
        if (!existed) josh_boot_health_default(&record);
        (void)josh_boot_health_prepare_attempt(
            &record, JOSH_BOOT_HEALTH_MAX_ATTEMPTS);
        if (!persist(file, &record)) { fclose(file); return 1; }
    } else {
        fclose(file); return 2;
    }

    show(&record);
    fclose(file);
    return 0;
}
