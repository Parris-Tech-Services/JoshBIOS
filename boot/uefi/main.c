#include "efi.h"
#include "../elf64.h"
#include "../protocol.h"
#include "../core/config.h"

#define COM1_BASE 0x3f8u
#define KERNEL_VIRT_FLOOR UINT64_C(0xffffffff80000000)
#define MAX_KERNEL_BYTES (64u * 1024u * 1024u)
#define PAGE_SIZE 4096u
#define HUGE_PAGE_SIZE UINT64_C(0x200000)
#define PAGE_PRESENT UINT64_C(0x001)
#define PAGE_RW UINT64_C(0x002)
#define PAGE_PS UINT64_C(0x080)
#define PAGE_TABLE_POOL_PAGES 48u
#define RAW_MAP_EXTRA_DESCRIPTORS 16u
#define IDENTITY_GIB 4u

EFI_GUID gEfiLoadedImageProtocolGuid={0x5b1b31a1u,0x9562u,0x11d2u,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
EFI_GUID gEfiSimpleFileSystemProtocolGuid={0x964e5b22u,0x6459u,0x11d2u,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
EFI_GUID gEfiFileInfoGuid={0x09576e92u,0x6d3fu,0x11d2u,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
EFI_GUID gEfiGraphicsOutputProtocolGuid={0x9042a9deu,0x23dcu,0x4a38u,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
EFI_GUID gEfiAcpi20TableGuid={0x8868e871u,0xe4f1u,0x11d3u,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
EFI_GUID gEfiAcpi10TableGuid={0xeb9d2d30u,0x2d88u,0x11d3u,{0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}};
EFI_GUID gEfiSmbios3TableGuid={0xf2fd1544u,0x9794u,0x4a2cu,{0x99,0x2e,0xe5,0xbb,0xcf,0x20,0xe3,0x94}};
EFI_GUID gEfiSmbiosTableGuid={0xeb9d2d31u,0x2d88u,0x11d3u,{0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}};

typedef struct {
    JoshBootInfo info;
    JoshMemoryMapEntry memory_map[JOSH_BOOT_MAX_MEMORY_ENTRIES];
    char command_line[JOSH_BOOT_CONFIG_CMDLINE_MAX+1u];
    char loader_name[32];
} uefi_metadata_t;

typedef struct { uint64_t *cursor,*end; } page_table_pool_t;

extern void josh_uefi_handoff(uint64_t,uint64_t,uint64_t,const JoshBootInfo*);

static inline void out8(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t in8(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static void serial_init(void){out8(COM1_BASE+1,0);out8(COM1_BASE+3,0x80);out8(COM1_BASE,1);out8(COM1_BASE+1,0);out8(COM1_BASE+3,3);out8(COM1_BASE+2,0xc7);out8(COM1_BASE+4,0x0b);}
static void serial_write(const char*s){while(s&&*s){while((in8(COM1_BASE+5)&0x20u)==0){}out8(COM1_BASE,(uint8_t)*s++);}}
static void memzero(void*d,uint64_t n){uint8_t*p=d;while(n--)*p++=0;}
static void memcopy(void*d,const void*s,uint64_t n){uint8_t*x=d;const uint8_t*y=s;while(n--)*x++=*y++;}
static uint32_t text_length(const char*s,uint32_t lim){uint32_t n=0;while(s&&n<lim&&s[n])++n;return n;}
static int guid_equal(const EFI_GUID*a,const EFI_GUID*b){if(a->Data1!=b->Data1||a->Data2!=b->Data2||a->Data3!=b->Data3)return 0;for(unsigned i=0;i<8;i++)if(a->Data4[i]!=b->Data4[i])return 0;return 1;}
static uint64_t pages_for(uint64_t n){return(n+PAGE_SIZE-1u)/PAGE_SIZE;}
static EFI_STATUS alloc_low(EFI_BOOT_SERVICES*bs,UINTN pages,EFI_PHYSICAL_ADDRESS*a){if(!bs||!a||!pages)return EFI_INVALID_PARAMETER;*a=UINT64_C(0xffffffff);return bs->AllocatePages(AllocateMaxAddress,EfiLoaderData,pages,a);}

static EFI_STATUS open_root(EFI_HANDLE image,EFI_SYSTEM_TABLE*st,EFI_FILE_PROTOCOL**root,EFI_LOADED_IMAGE_PROTOCOL**loaded_out){
    EFI_LOADED_IMAGE_PROTOCOL*loaded=0;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*fs=0;
    EFI_STATUS s=st->BootServices->HandleProtocol(image,&gEfiLoadedImageProtocolGuid,(VOID**)&loaded);
    if(EFI_ERROR(s)||!loaded)return s;
    s=st->BootServices->HandleProtocol(loaded->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(VOID**)&fs);
    if(EFI_ERROR(s)||!fs||!fs->OpenVolume)return EFI_NOT_FOUND;
    s=fs->OpenVolume(fs,root);if(!EFI_ERROR(s)&&loaded_out)*loaded_out=loaded;return s;
}

static int ascii_path_to_uefi(const char*a,CHAR16*w,UINTN cap){
    if(!a||!w||cap<2||a[0]!='/')return 0;UINTN o=0;
    for(UINTN i=0;a[i];i++){if(o+1>=cap)return 0;unsigned char c=(unsigned char)a[i];if(c<0x20u||c>0x7eu)return 0;w[o++]=(CHAR16)(c=='/'?'\\':c);}w[o]=0;return 1;
}

static EFI_STATUS read_file(EFI_BOOT_SERVICES*bs,EFI_FILE_PROTOCOL*root,CHAR16*path,VOID**buf,UINTN*size){
    EFI_FILE_PROTOCOL*f=0;EFI_STATUS s=root->Open(root,&f,path,EFI_FILE_MODE_READ,0);if(EFI_ERROR(s)||!f)return s;
    UINTN is=0;s=f->GetInfo(f,&gEfiFileInfoGuid,&is,0);if(s!=EFI_BUFFER_TOO_SMALL||is<24u){f->Close(f);return EFI_DEVICE_ERROR;}
    VOID*info=0;s=bs->AllocatePool(EfiLoaderData,is,&info);if(EFI_ERROR(s)){f->Close(f);return s;}
    s=f->GetInfo(f,&gEfiFileInfoGuid,&is,info);if(EFI_ERROR(s)){bs->FreePool(info);f->Close(f);return s;}
    uint64_t fs=*(uint64_t*)((uint8_t*)info+8u);bs->FreePool(info);if(!fs){f->Close(f);return EFI_BAD_BUFFER_SIZE;}
    s=bs->AllocatePool(EfiLoaderData,(UINTN)fs,buf);if(EFI_ERROR(s)){f->Close(f);return s;}
    *size=(UINTN)fs;s=f->Read(f,size,*buf);f->Close(f);if(EFI_ERROR(s)||*size!=(UINTN)fs){bs->FreePool(*buf);return EFI_DEVICE_ERROR;}return EFI_SUCCESS;
}

static EFI_STATUS load_files(EFI_HANDLE image,EFI_SYSTEM_TABLE*st,josh_boot_config_t*cfg,VOID**kernel,UINTN*kernel_size,EFI_LOADED_IMAGE_PROTOCOL**loaded){
    EFI_FILE_PROTOCOL*root=0;EFI_STATUS s=open_root(image,st,&root,loaded);if(EFI_ERROR(s))return s;
    CHAR16 cfg_path[]={'\\','E','F','I','\\','J','O','S','H','\\','B','O','O','T','.','C','F','G',0};
    VOID*cfg_buf=0;UINTN cfg_size=0;s=read_file(st->BootServices,root,cfg_path,&cfg_buf,&cfg_size);
    if(EFI_ERROR(s)){root->Close(root);return s;}
    if(cfg_size>JOSH_BOOT_CONFIG_MAX_BYTES||josh_boot_config_parse(cfg_buf,(size_t)cfg_size,cfg)!=JOSH_CONFIG_OK){st->BootServices->FreePool(cfg_buf);root->Close(root);return EFI_LOAD_ERROR;}
    st->BootServices->FreePool(cfg_buf);serial_write("JOSHUEFI_CONFIG_OK\r\n");
    CHAR16 path[JOSH_BOOT_CONFIG_PATH_MAX+1u];if(!ascii_path_to_uefi(cfg->kernel_path,path,sizeof(path)/sizeof(path[0]))){root->Close(root);return EFI_LOAD_ERROR;}
    s=read_file(st->BootServices,root,path,kernel,kernel_size);root->Close(root);if(!EFI_ERROR(s))serial_write("JOSHUEFI_KERNEL_FILE_LOADED\r\n");return s;
}

static EFI_STATUS load_elf(EFI_BOOT_SERVICES*bs,const VOID*image,UINTN bytes,JoshElf64Summary*sum,EFI_PHYSICAL_ADDRESS*phys){
    if(josh_elf64_validate(image,bytes,sum)!=JOSH_ELF64_OK)return EFI_LOAD_ERROR;
    if(sum->virtual_min<KERNEL_VIRT_FLOOR||sum->virtual_max<=sum->virtual_min)return EFI_UNSUPPORTED;
    uint64_t span=sum->virtual_max-sum->virtual_min;if(!span||span>MAX_KERNEL_BYTES)return EFI_BAD_BUFFER_SIZE;
    EFI_STATUS s=alloc_low(bs,(UINTN)pages_for(span),phys);if(EFI_ERROR(s))return s;memzero((VOID*)(uintptr_t)*phys,pages_for(span)*PAGE_SIZE);
    for(uint16_t i=0;i<sum->load_segment_count;i++){JoshElf64LoadSegment seg;if(josh_elf64_load_segment(image,bytes,i,&seg)!=JOSH_ELF64_OK||seg.virtual_address<sum->virtual_min)return EFI_LOAD_ERROR;uint64_t off=seg.virtual_address-sum->virtual_min;if(off>span||seg.memory_size>span-off)return EFI_LOAD_ERROR;memcopy((VOID*)(uintptr_t)(*phys+off),(const uint8_t*)image+seg.file_offset,seg.file_size);}
    serial_write("JOSHUEFI_ELF64_LOADED\r\n");return EFI_SUCCESS;
}

static unsigned mask_shift(uint32_t m){unsigned s=0;if(!m)return 0;while(!(m&1u)){m>>=1;s++;}return s;}
static unsigned mask_size(uint32_t m){unsigned s=0;while(m){s+=m&1u;m>>=1;}return s;}
static EFI_STATUS fill_fb(EFI_BOOT_SERVICES*bs,JoshFramebufferInfo*fb){
    EFI_GRAPHICS_OUTPUT_PROTOCOL*g=0;EFI_STATUS s=bs->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,0,(VOID**)&g);
    if(EFI_ERROR(s)||!g||!g->Mode||!g->Mode->Info)return EFI_NOT_FOUND;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*i=g->Mode->Info;
    if(i->PixelFormat==PixelBltOnly||!g->Mode->FrameBufferBase)return EFI_UNSUPPORTED;memzero(fb,sizeof(*fb));
    fb->address=g->Mode->FrameBufferBase;fb->width=i->HorizontalResolution;fb->height=i->VerticalResolution;fb->pitch=i->PixelsPerScanLine*4u;fb->bpp=32u;
    if(i->PixelFormat==PixelRedGreenBlueReserved8BitPerColor){fb->red_mask_size=8;fb->red_mask_shift=0;fb->green_mask_size=8;fb->green_mask_shift=8;fb->blue_mask_size=8;fb->blue_mask_shift=16;}
    else if(i->PixelFormat==PixelBlueGreenRedReserved8BitPerColor){fb->red_mask_size=8;fb->red_mask_shift=16;fb->green_mask_size=8;fb->green_mask_shift=8;fb->blue_mask_size=8;fb->blue_mask_shift=0;}
    else{fb->red_mask_size=mask_size(i->PixelInformation.RedMask);fb->red_mask_shift=mask_shift(i->PixelInformation.RedMask);fb->green_mask_size=mask_size(i->PixelInformation.GreenMask);fb->green_mask_shift=mask_shift(i->PixelInformation.GreenMask);fb->blue_mask_size=mask_size(i->PixelInformation.BlueMask);fb->blue_mask_shift=mask_shift(i->PixelInformation.BlueMask);}
    serial_write("JOSHUEFI_GOP_OK\r\n");return EFI_SUCCESS;
}

static uint64_t find_table(EFI_SYSTEM_TABLE*st,const EFI_GUID*a,const EFI_GUID*b){uint64_t fb=0;for(UINTN i=0;i<st->NumberOfTableEntries;i++){EFI_CONFIGURATION_TABLE*e=&st->ConfigurationTable[i];if(guid_equal(&e->VendorGuid,a))return(uint64_t)(uintptr_t)e->VendorTable;if(b&&guid_equal(&e->VendorGuid,b))fb=(uint64_t)(uintptr_t)e->VendorTable;}return fb;}
static uint32_t map_type(uint32_t t){if(t==EfiConventionalMemory)return JOSH_MEMORY_USABLE;if(t==EfiACPIReclaimMemory)return JOSH_MEMORY_ACPI_RECLAIMABLE;if(t==EfiACPIMemoryNVS)return JOSH_MEMORY_ACPI_NVS;if(t==EfiUnusableMemory)return JOSH_MEMORY_BAD;return JOSH_MEMORY_RESERVED;}

static EFI_STATUS capture_map(EFI_BOOT_SERVICES*bs,EFI_MEMORY_DESCRIPTOR*raw,UINTN cap,uefi_metadata_t*m,UINTN*key){
    UINTN size=cap,ds=0;uint32_t dv=0;EFI_STATUS s=bs->GetMemoryMap(&size,raw,key,&ds,&dv);(void)dv;if(EFI_ERROR(s)||ds<sizeof(EFI_MEMORY_DESCRIPTOR))return s;
    uint32_t n=0;for(UINTN off=0;off+ds<=size;off+=ds){EFI_MEMORY_DESCRIPTOR*d=(EFI_MEMORY_DESCRIPTOR*)((uint8_t*)raw+off);if(!d->NumberOfPages)continue;if(n>=JOSH_BOOT_MAX_MEMORY_ENTRIES)return EFI_OUT_OF_RESOURCES;JoshMemoryMapEntry*o=&m->memory_map[n++];o->base=d->PhysicalStart;o->length=d->NumberOfPages*PAGE_SIZE;o->type=map_type(d->Type);o->flags=0;}m->info.memory_map_entries=n;serial_write("JOSHUEFI_MEMORY_MAP_OK\r\n");return EFI_SUCCESS;
}

static uint64_t*pt_alloc(page_table_pool_t*p){if(!p||p->cursor>=p->end)return 0;uint64_t*r=p->cursor;p->cursor+=PAGE_SIZE/sizeof(uint64_t);memzero(r,PAGE_SIZE);return r;}
static uint64_t*table(uint64_t e){return(uint64_t*)(uintptr_t)(e&UINT64_C(0x000ffffffffff000));}
static int map4k(page_table_pool_t*p,uint64_t*pml4,uint64_t v,uint64_t ph){
    unsigned i4=(v>>39)&511u,i3=(v>>30)&511u,i2=(v>>21)&511u,i1=(v>>12)&511u;uint64_t*pdpt,*pd,*pt;
    if(!(pml4[i4]&PAGE_PRESENT)){pdpt=pt_alloc(p);if(!pdpt)return 0;pml4[i4]=(uint64_t)(uintptr_t)pdpt|PAGE_PRESENT|PAGE_RW;}else pdpt=table(pml4[i4]);
    if(!(pdpt[i3]&PAGE_PRESENT)){pd=pt_alloc(p);if(!pd)return 0;pdpt[i3]=(uint64_t)(uintptr_t)pd|PAGE_PRESENT|PAGE_RW;}else{if(pdpt[i3]&PAGE_PS)return 0;pd=table(pdpt[i3]);}
    if(!(pd[i2]&PAGE_PRESENT)){pt=pt_alloc(p);if(!pt)return 0;pd[i2]=(uint64_t)(uintptr_t)pt|PAGE_PRESENT|PAGE_RW;}else{if(pd[i2]&PAGE_PS)return 0;pt=table(pd[i2]);}
    pt[i1]=(ph&UINT64_C(0x000ffffffffff000))|PAGE_PRESENT|PAGE_RW;return 1;
}

static EFI_STATUS build_tables(EFI_BOOT_SERVICES*bs,uint64_t kp,const JoshElf64Summary*s,uint64_t*root,uint64_t*stack){
    EFI_PHYSICAL_ADDRESS pa;EFI_STATUS st=alloc_low(bs,PAGE_TABLE_POOL_PAGES,&pa);if(EFI_ERROR(st))return st;memzero((VOID*)(uintptr_t)pa,PAGE_TABLE_POOL_PAGES*PAGE_SIZE);
    page_table_pool_t pool={(uint64_t*)(uintptr_t)pa,(uint64_t*)(uintptr_t)(pa+PAGE_TABLE_POOL_PAGES*PAGE_SIZE)};
    uint64_t*pml4=pt_alloc(&pool),*pdpt=pt_alloc(&pool);if(!pml4||!pdpt)return EFI_OUT_OF_RESOURCES;pml4[0]=(uint64_t)(uintptr_t)pdpt|PAGE_PRESENT|PAGE_RW;
    for(unsigned g=0;g<IDENTITY_GIB;g++){uint64_t*pd=pt_alloc(&pool);if(!pd)return EFI_OUT_OF_RESOURCES;pdpt[g]=(uint64_t)(uintptr_t)pd|PAGE_PRESENT|PAGE_RW;for(unsigned i=0;i<512;i++){uint64_t ph=((uint64_t)g*512u+i)*HUGE_PAGE_SIZE;pd[i]=ph|PAGE_PRESENT|PAGE_RW|PAGE_PS;}}
    uint64_t pages=pages_for(s->virtual_max-s->virtual_min);for(uint64_t i=0;i<pages;i++)if(!map4k(&pool,pml4,s->virtual_min+i*PAGE_SIZE,kp+i*PAGE_SIZE))return EFI_OUT_OF_RESOURCES;
    EFI_PHYSICAL_ADDRESS sp;st=alloc_low(bs,4u,&sp);if(EFI_ERROR(st))return st;memzero((VOID*)(uintptr_t)sp,4u*PAGE_SIZE);*root=(uint64_t)(uintptr_t)pml4;*stack=sp+4u*PAGE_SIZE-16u;return EFI_SUCCESS;
}

static void fill_meta(uefi_metadata_t*m,EFI_SYSTEM_TABLE*st,const JoshElf64Summary*s,uint64_t kp,const josh_boot_config_t*c){
    memzero(m,sizeof(*m));JoshBootInfo*i=&m->info;i->magic=JOSH_BOOT_INFO_MAGIC;i->abi_major=JOSH_BOOT_ABI_MAJOR;i->abi_minor=JOSH_BOOT_ABI_MINOR;i->total_size=sizeof(*i);i->firmware_type=JOSH_FIRMWARE_UEFI;
    i->flags=JOSH_BOOT_FLAG_MEMORY_MAP|JOSH_BOOT_FLAG_FRAMEBUFFER|JOSH_BOOT_FLAG_LOADER_NAME;i->memory_map_address=(uint64_t)(uintptr_t)m->memory_map;i->memory_map_entry_size=sizeof(JoshMemoryMapEntry);
    i->kernel_phys_start=kp;i->kernel_phys_end=kp+(s->virtual_max-s->virtual_min);i->kernel_virt_start=s->virtual_min;i->kernel_virt_end=s->virtual_max;i->kernel_entry=s->entry;
    i->rsdp_phys=find_table(st,&gEfiAcpi20TableGuid,&gEfiAcpi10TableGuid);if(i->rsdp_phys)i->flags|=JOSH_BOOT_FLAG_RSDP;i->smbios_phys=find_table(st,&gEfiSmbios3TableGuid,&gEfiSmbiosTableGuid);if(i->smbios_phys)i->flags|=JOSH_BOOT_FLAG_SMBIOS;
    const char n[]="JoshBootloader UEFI";memcopy(m->loader_name,n,sizeof(n));i->bootloader_name_address=(uint64_t)(uintptr_t)m->loader_name;i->bootloader_name_length=sizeof(n)-1u;
    if(c&&c->command_line[0]){uint32_t l=text_length(c->command_line,JOSH_BOOT_CONFIG_CMDLINE_MAX);memcopy(m->command_line,c->command_line,l);i->command_line_address=(uint64_t)(uintptr_t)m->command_line;i->command_line_length=l;i->flags|=JOSH_BOOT_FLAG_CMDLINE;}
}

EFI_STATUS efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE*st){
    serial_init();serial_write("JOSHUEFI_ENTRY_OK\r\n");if(!st||!st->BootServices)return EFI_INVALID_PARAMETER;EFI_BOOT_SERVICES*bs=st->BootServices;
    josh_boot_config_t cfg;VOID*ki=0;UINTN kis=0;EFI_LOADED_IMAGE_PROTOCOL*li=0;EFI_STATUS s=load_files(image,st,&cfg,&ki,&kis,&li);
    if(EFI_ERROR(s)){serial_write("JOSHUEFI_ERROR_FILESYSTEM\r\n");return s;}
    if(!li||(uint64_t)(uintptr_t)li->ImageBase+li->ImageSize>UINT64_C(0x100000000)){serial_write("JOSHUEFI_ERROR_IMAGE_ABOVE_4G\r\n");return EFI_UNSUPPORTED;}
    JoshElf64Summary sum;EFI_PHYSICAL_ADDRESS kp;s=load_elf(bs,ki,kis,&sum,&kp);bs->FreePool(ki);if(EFI_ERROR(s)){serial_write("JOSHUEFI_ERROR_ELF64\r\n");return s;}
    EFI_PHYSICAL_ADDRESS ma;s=alloc_low(bs,1u,&ma);if(EFI_ERROR(s))return s;uefi_metadata_t*m=(uefi_metadata_t*)(uintptr_t)ma;fill_meta(m,st,&sum,kp,&cfg);
    s=fill_fb(bs,&m->info.framebuffer);if(EFI_ERROR(s)){serial_write("JOSHUEFI_ERROR_GOP\r\n");return s;}
    uint64_t root,stack;s=build_tables(bs,kp,&sum,&root,&stack);if(EFI_ERROR(s)){serial_write("JOSHUEFI_ERROR_PAGING\r\n");return s;}
    UINTN need=0,key=0,ds=0;uint32_t dv=0;s=bs->GetMemoryMap(&need,0,&key,&ds,&dv);if(s!=EFI_BUFFER_TOO_SMALL||ds<sizeof(EFI_MEMORY_DESCRIPTOR))return EFI_DEVICE_ERROR;
    need+=ds*RAW_MAP_EXTRA_DESCRIPTORS;EFI_PHYSICAL_ADDRESS ra;s=alloc_low(bs,(UINTN)pages_for(need),&ra);if(EFI_ERROR(s))return s;UINTN cap=(UINTN)(pages_for(need)*PAGE_SIZE);EFI_MEMORY_DESCRIPTOR*raw=(EFI_MEMORY_DESCRIPTOR*)(uintptr_t)ra;
    for(unsigned a=0;a<3;a++){s=capture_map(bs,raw,cap,m,&key);if(EFI_ERROR(s))return s;s=bs->ExitBootServices(image,key);if(!EFI_ERROR(s))break;if(a==2){serial_write("JOSHUEFI_ERROR_EXIT_BOOT_SERVICES\r\n");return s;}}
    serial_write("JOSHUEFI_EXIT_BOOT_SERVICES_OK\r\n");serial_write("JOSHUEFI_HANDOFF_READY\r\n");josh_uefi_handoff(root,stack,sum.entry,&m->info);for(;;)__asm__ volatile("cli; hlt");
}
