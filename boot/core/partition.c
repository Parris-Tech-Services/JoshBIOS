#include "partition.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MBR_SIGNATURE_OFFSET 510u
#define MBR_PARTITION_OFFSET 446u
#define MBR_PARTITION_SIZE   16u
#define MBR_PARTITION_COUNT  4u
#define GPT_HEADER_LBA       1u
#define GPT_HEADER_MIN_SIZE  92u
#define GPT_SIGNATURE        "EFI PART"

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const uint8_t *p) {
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

static int bytes_all_zero(const uint8_t *p, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (p[i] != 0) return 0;
    }
    return 1;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static int range_valid(const josh_block_device_t *device, uint64_t first, uint64_t count) {
    if (count == 0 || first >= device->sector_count) return 0;
    return count <= device->sector_count - first;
}

static josh_partition_status_t parse_gpt(const josh_block_device_t *device,
                                         josh_partition_t *partition) {
    uint8_t header[JOSH_BLOCK_SECTOR_SIZE];
    if (josh_block_read(device, GPT_HEADER_LBA, 1, header) != 0) {
        return JOSH_PARTITION_IO_ERROR;
    }
    if (memcmp(header, GPT_SIGNATURE, 8) != 0) return JOSH_PARTITION_CORRUPT;

    uint32_t header_size = read_le32(header + 12);
    uint32_t expected_header_crc = read_le32(header + 16);
    uint64_t current_lba = read_le64(header + 24);
    uint64_t first_usable = read_le64(header + 40);
    uint64_t last_usable = read_le64(header + 48);
    uint64_t entries_lba = read_le64(header + 72);
    uint32_t entry_count = read_le32(header + 80);
    uint32_t entry_size = read_le32(header + 84);
    uint32_t expected_entries_crc = read_le32(header + 88);

    if (header_size < GPT_HEADER_MIN_SIZE || header_size > JOSH_BLOCK_SECTOR_SIZE) {
        return JOSH_PARTITION_CORRUPT;
    }
    if (current_lba != GPT_HEADER_LBA || first_usable > last_usable || last_usable >= device->sector_count) {
        return JOSH_PARTITION_CORRUPT;
    }
    if (entry_count == 0 || entry_count > 4096u || entry_size < 128u || entry_size > JOSH_BLOCK_SECTOR_SIZE
        || (JOSH_BLOCK_SECTOR_SIZE % entry_size) != 0u) {
        return JOSH_PARTITION_UNSUPPORTED;
    }

    uint8_t header_copy[JOSH_BLOCK_SECTOR_SIZE];
    memcpy(header_copy, header, header_size);
    memset(header_copy + 16, 0, 4);
    if (crc32_update(0, header_copy, header_size) != expected_header_crc) {
        return JOSH_PARTITION_CORRUPT;
    }

    uint64_t entry_bytes = (uint64_t)entry_count * entry_size;
    uint64_t entry_sectors = (entry_bytes + JOSH_BLOCK_SECTOR_SIZE - 1u) / JOSH_BLOCK_SECTOR_SIZE;
    if (!range_valid(device, entries_lba, entry_sectors)) return JOSH_PARTITION_CORRUPT;

    uint32_t entries_crc = 0;
    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    uint64_t bytes_remaining = entry_bytes;
    for (uint64_t i = 0; i < entry_sectors; ++i) {
        if (josh_block_read(device, entries_lba + i, 1, sector) != 0) return JOSH_PARTITION_IO_ERROR;
        size_t chunk = bytes_remaining > JOSH_BLOCK_SECTOR_SIZE ? JOSH_BLOCK_SECTOR_SIZE : (size_t)bytes_remaining;
        entries_crc = crc32_update(entries_crc, sector, chunk);
        bytes_remaining -= chunk;
    }
    if (entries_crc != expected_entries_crc) return JOSH_PARTITION_CORRUPT;

    uint32_t entries_per_sector = JOSH_BLOCK_SECTOR_SIZE / entry_size;
    for (uint32_t index = 0; index < entry_count; ++index) {
        uint64_t sector_lba = entries_lba + (index / entries_per_sector);
        uint32_t offset = (index % entries_per_sector) * entry_size;
        if (josh_block_read(device, sector_lba, 1, sector) != 0) return JOSH_PARTITION_IO_ERROR;
        const uint8_t *entry = sector + offset;
        if (bytes_all_zero(entry, 16)) continue;

        uint64_t first = read_le64(entry + 32);
        uint64_t last = read_le64(entry + 40);
        if (first > last || first < first_usable || last > last_usable) return JOSH_PARTITION_CORRUPT;

        memset(partition, 0, sizeof(*partition));
        partition->scheme = JOSH_PARTITION_SCHEME_GPT;
        partition->first_lba = first;
        partition->sector_count = last - first + 1u;
        memcpy(partition->gpt_type_guid, entry, 16);
        memcpy(partition->gpt_unique_guid, entry + 16, 16);
        return JOSH_PARTITION_OK;
    }
    return JOSH_PARTITION_NO_USABLE_PARTITION;
}

josh_partition_status_t josh_partition_find_boot(const josh_block_device_t *device,
                                                  josh_partition_t *partition) {
    if (!device || !partition || !device->read || device->sector_count < 2) {
        return JOSH_PARTITION_NO_TABLE;
    }

    uint8_t mbr[JOSH_BLOCK_SECTOR_SIZE];
    if (josh_block_read(device, 0, 1, mbr) != 0) return JOSH_PARTITION_IO_ERROR;
    if (mbr[MBR_SIGNATURE_OFFSET] != 0x55 || mbr[MBR_SIGNATURE_OFFSET + 1] != 0xAA) {
        return JOSH_PARTITION_NO_TABLE;
    }

    const uint8_t *fallback = NULL;
    for (unsigned i = 0; i < MBR_PARTITION_COUNT; ++i) {
        const uint8_t *entry = mbr + MBR_PARTITION_OFFSET + i * MBR_PARTITION_SIZE;
        uint8_t status = entry[0];
        uint8_t type = entry[4];
        uint32_t first = read_le32(entry + 8);
        uint32_t count = read_le32(entry + 12);

        if (status != 0x00 && status != 0x80) return JOSH_PARTITION_CORRUPT;
        if (type == 0 || count == 0) continue;
        if (type == 0xEE) return parse_gpt(device, partition);
        if (!range_valid(device, first, count)) return JOSH_PARTITION_CORRUPT;
        if (!fallback) fallback = entry;
        if (status == 0x80) {
            fallback = entry;
            break;
        }
    }

    if (!fallback) return JOSH_PARTITION_NO_USABLE_PARTITION;
    memset(partition, 0, sizeof(*partition));
    partition->scheme = JOSH_PARTITION_SCHEME_MBR;
    partition->mbr_type = fallback[4];
    partition->first_lba = read_le32(fallback + 8);
    partition->sector_count = read_le32(fallback + 12);
    return JOSH_PARTITION_OK;
}

const char *josh_partition_status_string(josh_partition_status_t status) {
    switch (status) {
        case JOSH_PARTITION_OK: return "ok";
        case JOSH_PARTITION_NO_TABLE: return "no partition table";
        case JOSH_PARTITION_NO_USABLE_PARTITION: return "no usable partition";
        case JOSH_PARTITION_IO_ERROR: return "I/O error";
        case JOSH_PARTITION_CORRUPT: return "corrupt partition table";
        case JOSH_PARTITION_UNSUPPORTED: return "unsupported partition table";
        default: return "unknown partition error";
    }
}
