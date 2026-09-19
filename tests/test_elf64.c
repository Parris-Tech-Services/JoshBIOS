#include "../boot/elf64.h"
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

static void make_valid(unsigned char image[1024]) {
    memset(image, 0, 1024);

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
    h->entry = 0xffffffff80001000ULL;
    h->phoff = sizeof(*h);
    h->ehsize = sizeof(*h);
    h->phentsize = sizeof(TestElf64ProgramHeader);
    h->phnum = 2;

    TestElf64ProgramHeader *p =
        (TestElf64ProgramHeader *)(image + h->phoff);

    p[0].type = 1;
    p[0].flags = 5;
    p[0].offset = 0x200;
    p[0].vaddr = 0xffffffff80001000ULL;
    p[0].filesz = 32;
    p[0].memsz = 64;
    p[0].align = 0x100;

    p[1].type = 1;
    p[1].flags = 6;
    p[1].offset = 0x300;
    p[1].vaddr = 0xffffffff80002000ULL;
    p[1].filesz = 16;
    p[1].memsz = 32;
    p[1].align = 0x100;
}

int main(void) {
    unsigned char image[1024];
    JoshElf64Summary summary;
    JoshElf64LoadSegment segment;

    make_valid(image);
    expect("valid ELF64",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_OK);
    expect("entry captured",
           summary.entry == 0xffffffff80001000ULL);
    expect("load count", summary.load_segment_count == 2);
    expect("segment lookup",
           josh_elf64_load_segment(image, sizeof(image), 1, &segment) ==
               JOSH_ELF64_OK);
    expect("segment vaddr",
           segment.virtual_address == 0xffffffff80002000ULL);

    make_valid(image);
    image[0] = 0;
    expect("bad magic",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_MAGIC);

    make_valid(image);
    ((TestElf64Header *)image)->type = 3;
    expect("ET_DYN relocation model",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_RELOCATION_MODEL);

    make_valid(image);
    ((TestElf64Header *)image)->machine = 3;
    expect("wrong machine",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_MACHINE);

    make_valid(image);
    TestElf64ProgramHeader *p =
        (TestElf64ProgramHeader *)(image + sizeof(TestElf64Header));
    p[0].filesz = 65;
    expect("filesz greater than memsz",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_SEGMENT);

    make_valid(image);
    p = (TestElf64ProgramHeader *)(image + sizeof(TestElf64Header));
    p[1].vaddr = p[0].vaddr + 32;
    p[1].align = 1;
    expect("overlapping segments",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_OVERLAP);

    make_valid(image);
    ((TestElf64Header *)image)->entry = 0xffffffff90000000ULL;
    expect("entry outside executable segment",
           josh_elf64_validate(image, sizeof(image), &summary) ==
               JOSH_ELF64_ERR_ENTRY);

    make_valid(image);
    expect("truncated program headers",
           josh_elf64_validate(image, sizeof(TestElf64Header) + 10, &summary) ==
               JOSH_ELF64_ERR_TRUNCATED);

    for (int status = JOSH_ELF64_OK; status <= JOSH_ELF64_ERR_ENTRY; ++status) {
        expect("ELF status string", josh_elf64_status_string((JoshElf64Status)status) != NULL);
    }
    expect("unknown ELF status string", josh_elf64_status_string((JoshElf64Status)999) != NULL);

    if (failures) return 1;
    puts("JoshBIOS ELF64 parser tests passed");
    return 0;
}
