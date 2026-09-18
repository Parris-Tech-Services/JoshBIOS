#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../boot/core/block.h"
#include "../boot/core/fat32.h"
#include "../boot/core/partition.h"

typedef struct { uint8_t *bytes; uint64_t sectors; } memory_disk_t;

static int memory_read(void *context, uint64_t lba, uint32_t count, void *buffer) {
    memory_disk_t *disk = (memory_disk_t *)context;
    if (!disk || !buffer || lba >= disk->sectors || count > disk->sectors - lba) return -1;
    memcpy(buffer, disk->bytes + lba * JOSH_BLOCK_SECTOR_SIZE,
           (size_t)count * JOSH_BLOCK_SECTOR_SIZE);
    return 0;
}

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void put64(uint8_t *p, uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    crc = ~crc;
    for (size_t i=0;i<length;++i) {
        crc ^= data[i];
        for (unsigned bit=0;bit<8;++bit) {
            uint32_t mask=0u-(crc&1u);
            crc=(crc>>1)^(0xEDB88320u&mask);
        }
    }
    return ~crc;
}

static void set_fat_entry(memory_disk_t *disk, uint64_t fat_lba, uint32_t cluster, uint32_t value) {
    uint64_t byte_offset=(uint64_t)cluster*4u;
    uint64_t lba=fat_lba+byte_offset/JOSH_BLOCK_SECTOR_SIZE;
    uint32_t off=(uint32_t)(byte_offset%JOSH_BLOCK_SECTOR_SIZE);
    put32(disk->bytes+lba*JOSH_BLOCK_SECTOR_SIZE+off,value);
}

static void make_dir_entry(uint8_t *entry, const char name[11], uint8_t attr,
                           uint32_t cluster, uint32_t size) {
    memset(entry,0,32); memcpy(entry,name,11); entry[11]=attr;
    put16(entry+20,(uint16_t)(cluster>>16)); put16(entry+26,(uint16_t)cluster); put32(entry+28,size);
}

static memory_disk_t build_mbr_fat32_disk(void) {
    const uint64_t disk_sectors=72000;
    const uint32_t part_lba=2048, part_sectors=68000, fat_sectors=600;
    const uint16_t reserved=32;
    const uint8_t fats=2;
    const uint32_t data_rel=reserved+fats*fat_sectors;
    memory_disk_t disk={0};
    disk.sectors=disk_sectors;
    disk.bytes=calloc((size_t)disk_sectors,JOSH_BLOCK_SECTOR_SIZE);
    assert(disk.bytes);

    uint8_t *mbr=disk.bytes, *entry=mbr+446;
    entry[0]=0x80; entry[4]=0x0C; put32(entry+8,part_lba); put32(entry+12,part_sectors);
    mbr[510]=0x55; mbr[511]=0xAA;

    uint8_t *bpb=disk.bytes+(uint64_t)part_lba*JOSH_BLOCK_SECTOR_SIZE;
    bpb[0]=0xEB; bpb[1]=0x58; bpb[2]=0x90; memcpy(bpb+3,"JOSHBOOT",8);
    put16(bpb+11,JOSH_BLOCK_SECTOR_SIZE); bpb[13]=1; put16(bpb+14,reserved); bpb[16]=fats;
    put16(bpb+17,0); put16(bpb+19,0); bpb[21]=0xF8; put16(bpb+22,0);
    put32(bpb+32,part_sectors); put32(bpb+36,fat_sectors); put32(bpb+44,2);
    bpb[510]=0x55; bpb[511]=0xAA;

    uint64_t fat0=(uint64_t)part_lba+reserved;
    set_fat_entry(&disk,fat0,0,0x0FFFFFF8u); set_fat_entry(&disk,fat0,1,0xFFFFFFFFu);
    set_fat_entry(&disk,fat0,2,0x0FFFFFFFu); set_fat_entry(&disk,fat0,3,0x0FFFFFFFu);
    set_fat_entry(&disk,fat0,4,0x0FFFFFFFu); set_fat_entry(&disk,fat0,5,6u);
    set_fat_entry(&disk,fat0,6,0x0FFFFFFFu); set_fat_entry(&disk,fat0,7,0x0FFFFFFFu);

    uint64_t data_lba=(uint64_t)part_lba+data_rel;
    uint8_t *root=disk.bytes+data_lba*JOSH_BLOCK_SECTOR_SIZE;
    make_dir_entry(root,"BOOT       ",0x10,3,0); root[32]=0;
    uint8_t *boot_dir=disk.bytes+(data_lba+1)*JOSH_BLOCK_SECTOR_SIZE;
    make_dir_entry(boot_dir,"JOSH       ",0x10,4,0); boot_dir[32]=0;
    uint8_t *josh_dir=disk.bytes+(data_lba+2)*JOSH_BLOCK_SECTOR_SIZE;
    make_dir_entry(josh_dir,"KERNEL  ELF",0x20,5,900);
    make_dir_entry(josh_dir+32,"BROKEN  BIN",0x20,7,700); josh_dir[64]=0;

    uint8_t *file0=disk.bytes+(data_lba+3)*JOSH_BLOCK_SECTOR_SIZE;
    uint8_t *file1=disk.bytes+(data_lba+4)*JOSH_BLOCK_SECTOR_SIZE;
    for (unsigned i=0;i<512;++i) file0[i]=(uint8_t)(i&0xFFu);
    for (unsigned i=0;i<388;++i) file1[i]=(uint8_t)((i+17u)&0xFFu);
    return disk;
}

static void test_mbr_and_fat32(void) {
    memory_disk_t disk=build_mbr_fat32_disk();
    josh_block_device_t device={&disk,disk.sectors,memory_read};
    josh_partition_t partition;
    assert(josh_partition_find_boot(&device,&partition)==JOSH_PARTITION_OK);
    assert(partition.scheme==JOSH_PARTITION_SCHEME_MBR && partition.first_lba==2048 && partition.sector_count==68000);

    josh_fat32_t fs;
    assert(josh_fat32_mount(&device,&partition,&fs)==JOSH_FAT32_OK);
    josh_fat32_file_t kernel;
    assert(josh_fat32_open_path(&fs,"/boot/josh/kernel.elf",&kernel)==JOSH_FAT32_OK);
    assert(kernel.first_cluster==5 && kernel.size==900);

    uint8_t contents[900]; uint32_t bytes_read=0;
    assert(josh_fat32_read_file(&fs,&kernel,0,contents,sizeof(contents),&bytes_read)==JOSH_FAT32_OK);
    assert(bytes_read==900);
    for (unsigned i=0;i<512;++i) assert(contents[i]==(uint8_t)(i&0xFFu));
    for (unsigned i=0;i<388;++i) assert(contents[512+i]==(uint8_t)((i+17u)&0xFFu));

    uint8_t slice[40];
    assert(josh_fat32_read_file(&fs,&kernel,500,slice,sizeof(slice),&bytes_read)==JOSH_FAT32_OK);
    assert(bytes_read==40);
    for (unsigned i=0;i<12;++i) assert(slice[i]==(uint8_t)((500u+i)&0xFFu));
    for (unsigned i=12;i<40;++i) assert(slice[i]==(uint8_t)(((i-12u)+17u)&0xFFu));

    josh_fat32_file_t missing;
    assert(josh_fat32_open_path(&fs,"/boot/josh/nope.elf",&missing)==JOSH_FAT32_NOT_FOUND);
    assert(josh_fat32_open_path(&fs,"/boot/josh/kernel-too-long.elf",&missing)==JOSH_FAT32_BAD_PATH);

    josh_fat32_file_t broken;
    assert(josh_fat32_open_path(&fs,"/BOOT/JOSH/BROKEN.BIN",&broken)==JOSH_FAT32_OK);
    uint8_t broken_data[700];
    assert(josh_fat32_read_file(&fs,&broken,0,broken_data,sizeof(broken_data),&bytes_read)==JOSH_FAT32_CORRUPT);
    free(disk.bytes);
}

static memory_disk_t build_gpt_disk(void) {
    memory_disk_t disk={0}; disk.sectors=4096;
    disk.bytes=calloc((size_t)disk.sectors,JOSH_BLOCK_SECTOR_SIZE); assert(disk.bytes);
    uint8_t *mbr=disk.bytes, *mbr_entry=mbr+446;
    mbr_entry[4]=0xEE; put32(mbr_entry+8,1); put32(mbr_entry+12,4095); mbr[510]=0x55; mbr[511]=0xAA;

    uint8_t *entries=disk.bytes+2u*JOSH_BLOCK_SECTOR_SIZE;
    const uint8_t basic_data_guid[16]={0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};
    memcpy(entries,basic_data_guid,16);
    for (unsigned i=0;i<16;++i) entries[16+i]=(uint8_t)(i+1u);
    put64(entries+32,100); put64(entries+40,200);
    uint32_t entries_crc=crc32_update(0,entries,4u*128u);

    uint8_t *header=disk.bytes+JOSH_BLOCK_SECTOR_SIZE;
    memcpy(header,"EFI PART",8); put32(header+8,0x00010000u); put32(header+12,92); put32(header+16,0);
    put64(header+24,1); put64(header+32,4095); put64(header+40,34); put64(header+48,4000);
    for (unsigned i=0;i<16;++i) header[56+i]=(uint8_t)(0xA0u+i);
    put64(header+72,2); put32(header+80,4); put32(header+84,128); put32(header+88,entries_crc);
    put32(header+16,crc32_update(0,header,92));
    return disk;
}

static void test_gpt(void) {
    memory_disk_t disk=build_gpt_disk();
    josh_block_device_t device={&disk,disk.sectors,memory_read}; josh_partition_t partition;
    assert(josh_partition_find_boot(&device,&partition)==JOSH_PARTITION_OK);
    assert(partition.scheme==JOSH_PARTITION_SCHEME_GPT && partition.first_lba==100 && partition.sector_count==101);
    disk.bytes[JOSH_BLOCK_SECTOR_SIZE+40]^=1u;
    assert(josh_partition_find_boot(&device,&partition)==JOSH_PARTITION_CORRUPT);
    free(disk.bytes);
}

static void test_invalid_mbr(void) {
    memory_disk_t disk={0}; disk.sectors=16;
    disk.bytes=calloc((size_t)disk.sectors,JOSH_BLOCK_SECTOR_SIZE); assert(disk.bytes);
    josh_block_device_t device={&disk,disk.sectors,memory_read}; josh_partition_t partition;
    assert(josh_partition_find_boot(&device,&partition)==JOSH_PARTITION_NO_TABLE);
    free(disk.bytes);
}

int main(void) {
    test_mbr_and_fat32(); test_gpt(); test_invalid_mbr();
    puts("boot storage tests passed");
    return 0;
}
