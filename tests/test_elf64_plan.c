#include "../boot/elf64_plan.h"
#include <stdio.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    unsigned char ident[16];
    unsigned short type;
    unsigned short machine;
    unsigned int version;
    unsigned long long entry;
    unsigned long long phoff;
    unsigned long long shoff;
    unsigned int flags;
    unsigned short ehsize;
    unsigned short phentsize;
    unsigned short phnum;
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
} TestElf64Header;

typedef struct __attribute__((packed)) {
    unsigned int type;
    unsigned int flags;
    unsigned long long offset;
    unsigned long long vaddr;
    unsigned long long paddr;
    unsigned long long filesz;
    unsigned long long memsz;
    unsigned long long align;
} TestElf64ProgramHeader;

static int failures;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static void make_valid(unsigned char image[0x4000]) {
    memset(image, 0, 0x4000);
    TestElf64Header *h = (TestElf64Header *)image;
    h->ident[0] = 0x7f;
    h->ident[1] = 'E';
    h->ident[2] = 'L';
    h->ident[3] = 'F';
    h->ident[4] = 2;
    h->ident[5] = 1;
    h->ident[6] = 1;
    h->type = 2;
    h->machine = 62;
    h->version = 1;
    h->entry = 0xffffffff80001010ULL;
    h->phoff = sizeof(*h);
    h->ehsize = sizeof(*h);
    h->phentsize = sizeof(TestElf64ProgramHeader);
    h->phnum = 3;

    TestElf64ProgramHeader *p =
        (TestElf64ProgramHeader *)(image + h->phoff);

    p[0].type = 1;
    p[0].flags = 5;
    p[0].offset = 0x1000;
    p[0].vaddr = 0xffffffff80001000ULL;
    p[0].filesz = 0x100;
    p[0].memsz = 0x100;
    p[0].align = 0x1000;

    p[1].type = 1;
    p[1].flags = 4;
    p[1].offset = 0x2000;
    p[1].vaddr = 0xffffffff80002000ULL;
    p[1].filesz = 0x80;
    p[1].memsz = 0x180;
    p[1].align = 0x1000;

    p[2].type = 1;
    p[2].flags = 6;
    p[2].offset = 0x3000;
    p[2].vaddr = 0xffffffff80003000ULL;
    p[2].filesz = 0x20;
    p[2].memsz = 0x180;
    p[2].align = 0x1000;

    for (unsigned i = 0; i < 0x100; ++i) image[0x1000 + i] = (unsigned char)(i ^ 0x5a);
    for (unsigned i = 0; i < 0x80; ++i) image[0x2000 + i] = (unsigned char)(0xa0 + i);
    for (unsigned i = 0; i < 0x20; ++i) image[0x3000 + i] = (unsigned char)(0x30 + i);
}

typedef struct {
    unsigned char memory[0x6000];
    je_u64 base;
} TestMemory;

static int write_memory(void *context, je_u64 address,
                        const void *data, je_u64 length) {
    TestMemory *memory = (TestMemory *)context;
    if (address < memory->base || length > sizeof(memory->memory) ||
        address - memory->base > sizeof(memory->memory) - length) {
        return -1;
    }
    memcpy(memory->memory + (address - memory->base), data, (size_t)length);
    return 0;
}

static int zero_memory(void *context, je_u64 address, je_u64 length) {
    TestMemory *memory = (TestMemory *)context;
    if (address < memory->base || length > sizeof(memory->memory) ||
        address - memory->base > sizeof(memory->memory) - length) {
        return -1;
    }
    memset(memory->memory + (address - memory->base), 0, (size_t)length);
    return 0;
}

int main(void) {
    unsigned char image[0x4000];
    make_valid(image);

    JoshElf64LoadPlan plan;
    expect("plan valid high-half ELF",
           josh_elf64_plan_image(image, sizeof(image), 0x200000, 0x210000, &plan) ==
               JOSH_ELF64_PLAN_OK);
    expect("mapping count", plan.mapping_count == 3);
    expect("entry preserved", plan.entry == 0xffffffff80001010ULL);
    expect("text backing", plan.mappings[0].physical_page == 0x200000);
    expect("rodata backing", plan.mappings[1].physical_page == 0x201000);
    expect("data backing", plan.mappings[2].physical_page == 0x202000);

    TestMemory memory = {.base = 0x200000};
    memset(memory.memory, 0xcc, sizeof(memory.memory));
    expect("materialize",
           josh_elf64_materialize(image, sizeof(image), &plan,
                                  write_memory, zero_memory, &memory) ==
               JOSH_ELF64_PLAN_OK);
    expect("text copied", memcmp(memory.memory, image + 0x1000, 0x100) == 0);
    expect("text tail zero", memory.memory[0x100] == 0 && memory.memory[0xfff] == 0);
    expect("rodata copied", memcmp(memory.memory + 0x1000, image + 0x2000, 0x80) == 0);
    expect("rodata bss zero", memory.memory[0x1080] == 0 && memory.memory[0x117f] == 0);
    expect("data copied", memcmp(memory.memory + 0x2000, image + 0x3000, 0x20) == 0);
    expect("data bss zero", memory.memory[0x2020] == 0 && memory.memory[0x217f] == 0);

    expect("physical limit enforced",
           josh_elf64_plan_image(image, sizeof(image), 0x200000, 0x202000, &plan) ==
               JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY);

    TestElf64Header *h = (TestElf64Header *)image;
    TestElf64ProgramHeader *p =
        (TestElf64ProgramHeader *)(image + h->phoff);
    p[1].vaddr = 0xffffffff80001800ULL;
    p[1].offset = 0x1800;
    p[1].align = 0x100;
    expect("shared page rejected",
           josh_elf64_plan_image(image, sizeof(image), 0x200000, 0x210000, &plan) ==
               JOSH_ELF64_PLAN_ERR_PAGE_OVERLAP);

    if (failures) return 1;
    puts("JoshBIOS ELF64 load-plan tests passed");
    return 0;
}
