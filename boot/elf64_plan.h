#ifndef JOSHBOOT_ELF64_PLAN_H
#define JOSHBOOT_ELF64_PLAN_H

#include "elf64.h"

#define JOSH_ELF64_PAGE_SIZE 4096u

typedef enum {
    JOSH_ELF64_PLAN_OK = 0,
    JOSH_ELF64_PLAN_ERR_ARGUMENT,
    JOSH_ELF64_PLAN_ERR_ELF,
    JOSH_ELF64_PLAN_ERR_OVERFLOW,
    JOSH_ELF64_PLAN_ERR_PAGE_OVERLAP,
    JOSH_ELF64_PLAN_ERR_OUT_OF_MEMORY,
    JOSH_ELF64_PLAN_ERR_IO
} JoshElf64PlanStatus;

typedef struct {
    je_u64 virtual_page;
    je_u64 physical_page;
    je_u64 physical_address;
    je_u64 page_count;
    je_u64 file_offset;
    je_u64 file_size;
    je_u64 memory_size;
    je_u32 flags;
} JoshElf64Mapping;

typedef struct {
    je_u64 entry;
    je_u64 virtual_min;
    je_u64 virtual_max;
    je_u64 physical_start;
    je_u64 physical_end;
    je_u16 mapping_count;
    JoshElf64Mapping mappings[JOSH_ELF64_MAX_PHDRS];
} JoshElf64LoadPlan;

typedef int (*JoshElf64WriteFn)(void *context, je_u64 physical_address,
                                 const void *data, je_u64 length);
typedef int (*JoshElf64ZeroFn)(void *context, je_u64 physical_address,
                                je_u64 length);

JoshElf64PlanStatus josh_elf64_plan_segments(const JoshElf64Summary *summary,
                                              const JoshElf64LoadSegment *segments,
                                              je_u16 segment_count,
                                              je_u64 physical_base,
                                              je_u64 physical_limit,
                                              JoshElf64LoadPlan *plan);
JoshElf64PlanStatus josh_elf64_plan_image(const void *image,
                                           je_u64 image_size,
                                           je_u64 physical_base,
                                           je_u64 physical_limit,
                                           JoshElf64LoadPlan *plan);
JoshElf64PlanStatus josh_elf64_materialize(const void *image,
                                            je_u64 image_size,
                                            const JoshElf64LoadPlan *plan,
                                            JoshElf64WriteFn write_fn,
                                            JoshElf64ZeroFn zero_fn,
                                            void *context);
const char *josh_elf64_plan_status_string(JoshElf64PlanStatus status);

#endif
