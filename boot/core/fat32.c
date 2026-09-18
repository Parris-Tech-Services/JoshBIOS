#include "fat32.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FAT32_MIN_CLUSTERS 65525u
#define FAT32_EOC_MIN      0x0FFFFFF8u
#define FAT32_BAD_CLUSTER  0x0FFFFFF7u
#define FAT32_CLUSTER_MASK 0x0FFFFFFFu
#define FAT_ATTR_DIRECTORY 0x10u
#define FAT_ATTR_VOLUME    0x08u
#define FAT_ATTR_LFN       0x0Fu

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static int power_of_two_u8(uint8_t value) {
    return value != 0 && (value & (uint8_t)(value - 1u)) == 0;
}

static int cluster_valid(const josh_fat32_t *fs, uint32_t cluster) {
    return cluster >= 2u && cluster < fs->cluster_count + 2u;
}

static josh_fat32_status_t cluster_lba(const josh_fat32_t *fs, uint32_t cluster, uint64_t *lba) {
    if (!cluster_valid(fs, cluster)) return JOSH_FAT32_CORRUPT;
    uint64_t relative = (uint64_t)fs->data_lba + (uint64_t)(cluster - 2u) * fs->sectors_per_cluster;
    if (relative >= fs->partition_sectors || fs->sectors_per_cluster > fs->partition_sectors - relative) {
        return JOSH_FAT32_CORRUPT;
    }
    *lba = fs->partition_lba + relative;
    return JOSH_FAT32_OK;
}

static josh_fat32_status_t next_cluster(const josh_fat32_t *fs, uint32_t cluster, uint32_t *next) {
    if (!cluster_valid(fs, cluster)) return JOSH_FAT32_CORRUPT;
    uint64_t fat_offset = (uint64_t)cluster * 4u;
    uint64_t sector_index = fat_offset / JOSH_BLOCK_SECTOR_SIZE;
    uint32_t byte_offset = (uint32_t)(fat_offset % JOSH_BLOCK_SECTOR_SIZE);
    if (sector_index >= fs->sectors_per_fat || byte_offset > JOSH_BLOCK_SECTOR_SIZE - 4u) {
        return JOSH_FAT32_CORRUPT;
    }

    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    uint64_t lba = fs->partition_lba + fs->fat_lba + sector_index;
    if (josh_block_read(fs->device, lba, 1, sector) != 0) return JOSH_FAT32_IO_ERROR;
    uint32_t value = read_le32(sector + byte_offset) & FAT32_CLUSTER_MASK;
    if (value == FAT32_BAD_CLUSTER || value == 0 || value == 1) return JOSH_FAT32_CORRUPT;
    *next = value;
    return JOSH_FAT32_OK;
}

static int ascii_upper(char c, uint8_t *out) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_' || c == '-' || c == '$' || c == '~' || c == '!' || c == '#'
        || c == '%' || c == '&' || c == '(' || c == ')' || c == '@' || c == '^'
        || c == '`' || c == '{' || c == '}') {
        *out = (uint8_t)c;
        return 1;
    }
    return 0;
}

static josh_fat32_status_t short_name_from_segment(const char *segment, size_t length, uint8_t out[11]) {
    if (!segment || length == 0 || length > 12) return JOSH_FAT32_BAD_PATH;
    memset(out, ' ', 11);

    size_t dot = length;
    for (size_t i = 0; i < length; ++i) {
        if (segment[i] == '.') {
            if (dot != length) return JOSH_FAT32_BAD_PATH;
            dot = i;
        }
    }
    size_t base_len = dot;
    size_t ext_len = dot == length ? 0 : length - dot - 1u;
    if (base_len == 0 || base_len > 8 || ext_len > 3) return JOSH_FAT32_BAD_PATH;

    for (size_t i = 0; i < base_len; ++i) {
        if (!ascii_upper(segment[i], &out[i])) return JOSH_FAT32_BAD_PATH;
    }
    for (size_t i = 0; i < ext_len; ++i) {
        if (!ascii_upper(segment[dot + 1u + i], &out[8u + i])) return JOSH_FAT32_BAD_PATH;
    }
    return JOSH_FAT32_OK;
}

static josh_fat32_status_t find_in_directory(const josh_fat32_t *fs,
                                              uint32_t directory_cluster,
                                              const uint8_t short_name[11],
                                              josh_fat32_file_t *file) {
    uint32_t cluster = directory_cluster;
    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];

    for (uint32_t visited = 0; visited <= fs->cluster_count; ++visited) {
        uint64_t base_lba = 0;
        josh_fat32_status_t status = cluster_lba(fs, cluster, &base_lba);
        if (status != JOSH_FAT32_OK) return status;

        for (uint32_t sector_index = 0; sector_index < fs->sectors_per_cluster; ++sector_index) {
            if (josh_block_read(fs->device, base_lba + sector_index, 1, sector) != 0) {
                return JOSH_FAT32_IO_ERROR;
            }
            for (uint32_t offset = 0; offset < JOSH_BLOCK_SECTOR_SIZE; offset += 32u) {
                const uint8_t *entry = sector + offset;
                if (entry[0] == 0x00) return JOSH_FAT32_NOT_FOUND;
                if (entry[0] == 0xE5) continue;
                uint8_t attributes = entry[11];
                if (attributes == FAT_ATTR_LFN || (attributes & FAT_ATTR_VOLUME) != 0) continue;
                if (memcmp(entry, short_name, 11) != 0) continue;

                uint32_t high = read_le16(entry + 20);
                uint32_t low = read_le16(entry + 26);
                file->first_cluster = (high << 16) | low;
                file->size = read_le32(entry + 28);
                file->attributes = attributes;
                if (file->first_cluster < 2u && file->size != 0) return JOSH_FAT32_CORRUPT;
                return JOSH_FAT32_OK;
            }
        }

        uint32_t next = 0;
        status = next_cluster(fs, cluster, &next);
        if (status != JOSH_FAT32_OK) return status;
        if (next >= FAT32_EOC_MIN) return JOSH_FAT32_NOT_FOUND;
        cluster = next;
    }
    return JOSH_FAT32_CORRUPT;
}

josh_fat32_status_t josh_fat32_mount(const josh_block_device_t *device,
                                      const josh_partition_t *partition,
                                      josh_fat32_t *fs) {
    if (!device || !partition || !fs || partition->sector_count == 0) return JOSH_FAT32_NOT_FAT32;
    if (partition->first_lba >= device->sector_count
        || partition->sector_count > device->sector_count - partition->first_lba) {
        return JOSH_FAT32_CORRUPT;
    }

    uint8_t boot[JOSH_BLOCK_SECTOR_SIZE];
    if (josh_block_read(device, partition->first_lba, 1, boot) != 0) return JOSH_FAT32_IO_ERROR;
    if (boot[510] != 0x55 || boot[511] != 0xAA) return JOSH_FAT32_NOT_FAT32;

    uint16_t bytes_per_sector = read_le16(boot + 11);
    uint8_t sectors_per_cluster = boot[13];
    uint16_t reserved = read_le16(boot + 14);
    uint8_t fat_count = boot[16];
    uint16_t root_entries = read_le16(boot + 17);
    uint16_t total16 = read_le16(boot + 19);
    uint16_t fat16 = read_le16(boot + 22);
    uint32_t total32 = read_le32(boot + 32);
    uint32_t fat32 = read_le32(boot + 36);
    uint32_t root_cluster = read_le32(boot + 44);

    if (bytes_per_sector != JOSH_BLOCK_SECTOR_SIZE) return JOSH_FAT32_UNSUPPORTED;
    if (!power_of_two_u8(sectors_per_cluster) || sectors_per_cluster > 128u) return JOSH_FAT32_CORRUPT;
    if (reserved == 0 || fat_count == 0 || fat_count > 2 || root_entries != 0 || fat16 != 0 || fat32 == 0) {
        return JOSH_FAT32_NOT_FAT32;
    }

    uint32_t total = total16 != 0 ? total16 : total32;
    if (total == 0 || total > partition->sector_count) return JOSH_FAT32_CORRUPT;
    uint64_t fat_area = (uint64_t)fat_count * fat32;
    if ((uint64_t)reserved + fat_area >= total) return JOSH_FAT32_CORRUPT;
    uint32_t data_sectors = (uint32_t)(total - reserved - fat_area);
    uint32_t clusters = data_sectors / sectors_per_cluster;
    if (clusters < FAT32_MIN_CLUSTERS) return JOSH_FAT32_NOT_FAT32;
    if (root_cluster < 2u || root_cluster >= clusters + 2u) return JOSH_FAT32_CORRUPT;

    memset(fs, 0, sizeof(*fs));
    fs->device = device;
    fs->partition_lba = partition->first_lba;
    fs->partition_sectors = total;
    fs->fat_lba = reserved;
    fs->data_lba = (uint32_t)(reserved + fat_area);
    fs->sectors_per_fat = fat32;
    fs->cluster_count = clusters;
    fs->root_cluster = root_cluster;
    fs->sectors_per_cluster = sectors_per_cluster;
    fs->fat_count = fat_count;
    return JOSH_FAT32_OK;
}

josh_fat32_status_t josh_fat32_open_path(const josh_fat32_t *fs,
                                          const char *path,
                                          josh_fat32_file_t *file) {
    if (!fs || !path || !file) return JOSH_FAT32_BAD_PATH;
    while (*path == '/') ++path;
    if (*path == '\0') {
        file->first_cluster = fs->root_cluster;
        file->size = 0;
        file->attributes = FAT_ATTR_DIRECTORY;
        return JOSH_FAT32_OK;
    }

    uint32_t directory = fs->root_cluster;
    for (;;) {
        const char *segment = path;
        while (*path != '\0' && *path != '/') ++path;
        size_t length = (size_t)(path - segment);
        uint8_t short_name[11];
        josh_fat32_status_t status = short_name_from_segment(segment, length, short_name);
        if (status != JOSH_FAT32_OK) return status;

        josh_fat32_file_t found;
        status = find_in_directory(fs, directory, short_name, &found);
        if (status != JOSH_FAT32_OK) return status;

        while (*path == '/') ++path;
        if (*path == '\0') {
            *file = found;
            return JOSH_FAT32_OK;
        }
        if ((found.attributes & FAT_ATTR_DIRECTORY) == 0) return JOSH_FAT32_NOT_DIRECTORY;
        if (!cluster_valid(fs, found.first_cluster)) return JOSH_FAT32_CORRUPT;
        directory = found.first_cluster;
    }
}

josh_fat32_status_t josh_fat32_read_file(const josh_fat32_t *fs,
                                          const josh_fat32_file_t *file,
                                          uint32_t offset,
                                          void *buffer,
                                          uint32_t length,
                                          uint32_t *bytes_read) {
    if (bytes_read) *bytes_read = 0;
    if (!fs || !file || (!buffer && length != 0)) return JOSH_FAT32_BAD_PATH;
    if ((file->attributes & FAT_ATTR_DIRECTORY) != 0) return JOSH_FAT32_NOT_DIRECTORY;
    if (offset > file->size) return JOSH_FAT32_BAD_PATH;

    uint32_t available = file->size - offset;
    if (length > available) length = available;
    if (length == 0) return JOSH_FAT32_OK;
    if (!cluster_valid(fs, file->first_cluster)) return JOSH_FAT32_CORRUPT;

    uint32_t cluster_bytes = (uint32_t)fs->sectors_per_cluster * JOSH_BLOCK_SECTOR_SIZE;
    uint32_t skip_clusters = offset / cluster_bytes;
    uint32_t offset_in_cluster = offset % cluster_bytes;
    uint32_t cluster = file->first_cluster;

    for (uint32_t skipped = 0; skipped < skip_clusters; ++skipped) {
        uint32_t next = 0;
        josh_fat32_status_t status = next_cluster(fs, cluster, &next);
        if (status != JOSH_FAT32_OK || next >= FAT32_EOC_MIN) return JOSH_FAT32_CORRUPT;
        cluster = next;
    }

    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    uint8_t *out = (uint8_t *)buffer;
    uint32_t remaining = length;
    uint32_t visited = skip_clusters;

    while (remaining > 0) {
        if (visited++ > fs->cluster_count) return JOSH_FAT32_CORRUPT;
        uint64_t base_lba = 0;
        josh_fat32_status_t status = cluster_lba(fs, cluster, &base_lba);
        if (status != JOSH_FAT32_OK) return status;

        uint32_t first_sector = offset_in_cluster / JOSH_BLOCK_SECTOR_SIZE;
        uint32_t byte_offset = offset_in_cluster % JOSH_BLOCK_SECTOR_SIZE;
        for (uint32_t s = first_sector; s < fs->sectors_per_cluster && remaining > 0; ++s) {
            if (josh_block_read(fs->device, base_lba + s, 1, sector) != 0) return JOSH_FAT32_IO_ERROR;
            uint32_t take = JOSH_BLOCK_SECTOR_SIZE - byte_offset;
            if (take > remaining) take = remaining;
            memcpy(out, sector + byte_offset, take);
            out += take;
            remaining -= take;
            byte_offset = 0;
        }
        offset_in_cluster = 0;
        if (remaining == 0) break;

        uint32_t next = 0;
        status = next_cluster(fs, cluster, &next);
        if (status != JOSH_FAT32_OK) return status;
        if (next >= FAT32_EOC_MIN) return JOSH_FAT32_CORRUPT;
        cluster = next;
    }

    if (bytes_read) *bytes_read = length;
    return JOSH_FAT32_OK;
}

const char *josh_fat32_status_string(josh_fat32_status_t status) {
    switch (status) {
        case JOSH_FAT32_OK: return "ok";
        case JOSH_FAT32_IO_ERROR: return "I/O error";
        case JOSH_FAT32_NOT_FAT32: return "not FAT32";
        case JOSH_FAT32_CORRUPT: return "corrupt FAT32";
        case JOSH_FAT32_UNSUPPORTED: return "unsupported FAT32 feature";
        case JOSH_FAT32_NOT_FOUND: return "path not found";
        case JOSH_FAT32_NOT_DIRECTORY: return "not a directory";
        case JOSH_FAT32_BAD_PATH: return "invalid path";
        case JOSH_FAT32_BUFFER_TOO_SMALL: return "buffer too small";
        default: return "unknown FAT32 error";
    }
}
