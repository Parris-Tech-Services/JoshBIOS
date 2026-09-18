#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../boot/core/elf64.h"

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void put64(uint8_t *p, uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }

static void phdr(uint8_t *p, uint32_t flags, uint64_t off, uint64_t va,
                 uint64_t filesz, uint64_t memsz, uint64_t align) {
    put32(p,1); put32(p+4,flags); put64(p+8,off); put64(p+16,va); put64(p+24,va);
    put64(p+32,filesz); put64(p+40,memsz); put64(p+48,align);
}

static size_t make_valid(uint8_t *image, size_t cap) {
    assert(cap >= 0x2400); memset(image,0,cap);
    image[0]=0x7f; image[1]='E'; image[2]='L'; image[3]='F'; image[4]=2; image[5]=1; image[6]=1;
    put16(image+16,2); put16(image+18,62); put32(image+20,1);
    put64(image+24,0xffffffff80001010ull); put64(image+32,64);
    put16(image+52,64); put16(image+54,56); put16(image+56,2);
    phdr(image+64,5,0x1000,0xffffffff80001000ull,0x100,0x100,0x1000);
    phdr(image+120,6,0x2000,0xffffffff80002000ull,0x80,0x180,0x1000);
    for (unsigned i=0;i<0x100;++i) image[0x1000+i]=(uint8_t)(0xA0u+(i&0x1Fu));
    for (unsigned i=0;i<0x80;++i) image[0x2000+i]=(uint8_t)(0x30u+(i&0x0Fu));
    return 0x2080;
}

typedef struct { uint8_t memory[0x10000]; uint64_t base; } phys_t;
static int write_phys(void *ctx,uint64_t addr,const void *data,size_t n) {
    phys_t *p=(phys_t*)ctx; if(addr<p->base || n>sizeof(p->memory)-(addr-p->base)) return -1;
    memcpy(p->memory+(addr-p->base),data,n); return 0;
}
static int zero_phys(void *ctx,uint64_t addr,size_t n) {
    phys_t *p=(phys_t*)ctx; if(addr<p->base || n>sizeof(p->memory)-(addr-p->base)) return -1;
    memset(p->memory+(addr-p->base),0,n); return 0;
}

int main(void) {
    uint8_t image[0x2400]; size_t size=make_valid(image,sizeof(image));
    josh_elf64_image_t elf;
    assert(josh_elf64_parse(image,size,&elf)==JOSH_ELF64_OK);
    assert(elf.segment_count==2 && elf.entry==0xffffffff80001010ull);

    josh_elf64_load_plan_t plan;
    assert(josh_elf64_plan(&elf,0x200000,0x210000,&plan)==JOSH_ELF64_OK);
    assert(plan.segment_count==2 && plan.segments[0].physical_page==0x200000);
    assert(plan.segments[1].physical_page==0x201000);

    phys_t phys={.base=0x200000}; memset(phys.memory,0xCC,sizeof(phys.memory));
    assert(josh_elf64_load(image,size,&plan,write_phys,zero_phys,&phys)==JOSH_ELF64_OK);
    assert(memcmp(phys.memory,image+0x1000,0x100)==0);
    assert(memcmp(phys.memory+0x1000,image+0x2000,0x80)==0);
    for (unsigned i=0x1080;i<0x1180;++i) assert(phys.memory[i]==0);

    uint8_t bad[sizeof(image)]; memcpy(bad,image,sizeof(image)); bad[0]='X';
    assert(josh_elf64_parse(bad,size,&elf)==JOSH_ELF64_BAD_MAGIC);
    memcpy(bad,image,sizeof(image)); put16(bad+18,3);
    assert(josh_elf64_parse(bad,size,&elf)==JOSH_ELF64_UNSUPPORTED_MACHINE);
    memcpy(bad,image,sizeof(image)); put64(bad+120+16,0xffffffff80001000ull);
    assert(josh_elf64_parse(bad,size,&elf)==JOSH_ELF64_OVERLAP);
    memcpy(bad,image,sizeof(image)); put64(bad+24,0xffffffff80003000ull);
    assert(josh_elf64_parse(bad,size,&elf)==JOSH_ELF64_ENTRY_NOT_EXECUTABLE);
    memcpy(bad,image,sizeof(image)); put64(bad+120+40,0x100000);
    assert(josh_elf64_parse(bad,size,&elf)==JOSH_ELF64_OK);
    assert(josh_elf64_plan(&elf,0x200000,0x210000,&plan)==JOSH_ELF64_OUT_OF_MEMORY);

    puts("ELF64 tests passed");
    return 0;
}
