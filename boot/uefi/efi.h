#ifndef JOSHBOOT_UEFI_EFI_H
#define JOSHBOOT_UEFI_EFI_H
#include <stdint.h>
typedef void VOID; typedef void *EFI_HANDLE; typedef uint64_t EFI_STATUS;
typedef uint64_t EFI_PHYSICAL_ADDRESS; typedef uint64_t EFI_VIRTUAL_ADDRESS;
typedef uint64_t UINTN; typedef uint16_t CHAR16;
typedef struct { uint32_t Data1; uint16_t Data2,Data3; uint8_t Data4[8]; } EFI_GUID;
typedef struct { uint64_t Signature; uint32_t Revision,HeaderSize,CRC32,Reserved; } EFI_TABLE_HEADER;
#define EFI_ERROR_BIT UINT64_C(0x8000000000000000)
#define EFIERR(c) (EFI_ERROR_BIT|(EFI_STATUS)(c))
#define EFI_SUCCESS ((EFI_STATUS)0)
#define EFI_LOAD_ERROR EFIERR(1)
#define EFI_INVALID_PARAMETER EFIERR(2)
#define EFI_UNSUPPORTED EFIERR(3)
#define EFI_BAD_BUFFER_SIZE EFIERR(4)
#define EFI_BUFFER_TOO_SMALL EFIERR(5)
#define EFI_DEVICE_ERROR EFIERR(7)
#define EFI_OUT_OF_RESOURCES EFIERR(9)
#define EFI_NOT_FOUND EFIERR(14)
#define EFI_ERROR(s) (((s)&EFI_ERROR_BIT)!=0)
#define EFI_FILE_MODE_READ UINT64_C(1)
#define EFI_PAGE_SIZE 4096u
typedef enum { AllocateAnyPages,AllocateMaxAddress,AllocateAddress,MaxAllocateType } EFI_ALLOCATE_TYPE;
typedef enum { EfiReservedMemoryType,EfiLoaderCode,EfiLoaderData,EfiBootServicesCode,EfiBootServicesData,EfiRuntimeServicesCode,EfiRuntimeServicesData,EfiConventionalMemory,EfiUnusableMemory,EfiACPIReclaimMemory,EfiACPIMemoryNVS,EfiMemoryMappedIO,EfiMemoryMappedIOPortSpace,EfiPalCode,EfiPersistentMemory,EfiUnacceptedMemoryType,EfiMaxMemoryType } EFI_MEMORY_TYPE;
typedef struct { uint32_t Type,Pad; EFI_PHYSICAL_ADDRESS PhysicalStart; EFI_VIRTUAL_ADDRESS VirtualStart; uint64_t NumberOfPages,Attribute; } EFI_MEMORY_DESCRIPTOR;
typedef EFI_STATUS(*EFI_ALLOCATE_PAGES)(EFI_ALLOCATE_TYPE,EFI_MEMORY_TYPE,UINTN,EFI_PHYSICAL_ADDRESS*);
typedef EFI_STATUS(*EFI_FREE_PAGES)(EFI_PHYSICAL_ADDRESS,UINTN);
typedef EFI_STATUS(*EFI_GET_MEMORY_MAP)(UINTN*,EFI_MEMORY_DESCRIPTOR*,UINTN*,UINTN*,uint32_t*);
typedef EFI_STATUS(*EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE,UINTN,VOID**);
typedef EFI_STATUS(*EFI_FREE_POOL)(VOID*);
typedef EFI_STATUS(*EFI_HANDLE_PROTOCOL)(EFI_HANDLE,EFI_GUID*,VOID**);
typedef EFI_STATUS(*EFI_LOCATE_PROTOCOL)(EFI_GUID*,VOID*,VOID**);
typedef EFI_STATUS(*EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE,UINTN);
typedef struct { EFI_TABLE_HEADER Hdr; VOID *RaiseTPL,*RestoreTPL; EFI_ALLOCATE_PAGES AllocatePages; EFI_FREE_PAGES FreePages; EFI_GET_MEMORY_MAP GetMemoryMap; EFI_ALLOCATE_POOL AllocatePool; EFI_FREE_POOL FreePool; VOID *CreateEvent,*SetTimer,*WaitForEvent,*SignalEvent,*CloseEvent,*CheckEvent,*InstallProtocolInterface,*ReinstallProtocolInterface,*UninstallProtocolInterface; EFI_HANDLE_PROTOCOL HandleProtocol; VOID *Reserved,*RegisterProtocolNotify,*LocateHandle,*LocateDevicePath,*InstallConfigurationTable,*LoadImage,*StartImage,*Exit,*UnloadImage; EFI_EXIT_BOOT_SERVICES ExitBootServices; VOID *GetNextMonotonicCount,*Stall,*SetWatchdogTimer,*ConnectController,*DisconnectController,*OpenProtocol,*CloseProtocol,*OpenProtocolInformation,*ProtocolsPerHandle,*LocateHandleBuffer; EFI_LOCATE_PROTOCOL LocateProtocol; VOID *InstallMultipleProtocolInterfaces,*UninstallMultipleProtocolInterfaces,*CalculateCrc32,*CopyMem,*SetMem,*CreateEventEx; } EFI_BOOT_SERVICES;
typedef struct { EFI_GUID VendorGuid; VOID *VendorTable; } EFI_CONFIGURATION_TABLE;
typedef struct EFI_SYSTEM_TABLE { EFI_TABLE_HEADER Hdr; CHAR16 *FirmwareVendor; uint32_t FirmwareRevision; EFI_HANDLE ConsoleInHandle; VOID *ConIn; EFI_HANDLE ConsoleOutHandle; VOID *ConOut; EFI_HANDLE StandardErrorHandle; VOID *StdErr,*RuntimeServices; EFI_BOOT_SERVICES *BootServices; UINTN NumberOfTableEntries; EFI_CONFIGURATION_TABLE *ConfigurationTable; } EFI_SYSTEM_TABLE;
typedef struct { uint32_t Revision; EFI_HANDLE ParentHandle; EFI_SYSTEM_TABLE *SystemTable; EFI_HANDLE DeviceHandle; VOID *FilePath,*Reserved; uint32_t LoadOptionsSize; VOID *LoadOptions,*ImageBase; uint64_t ImageSize; EFI_MEMORY_TYPE ImageCodeType,ImageDataType; EFI_STATUS(*Unload)(EFI_HANDLE); } EFI_LOADED_IMAGE_PROTOCOL;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef EFI_STATUS(*EFI_FILE_OPEN)(EFI_FILE_PROTOCOL*,EFI_FILE_PROTOCOL**,CHAR16*,uint64_t,uint64_t);
typedef EFI_STATUS(*EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL*);
typedef EFI_STATUS(*EFI_FILE_READ)(EFI_FILE_PROTOCOL*,UINTN*,VOID*);
typedef EFI_STATUS(*EFI_FILE_GET_INFO)(EFI_FILE_PROTOCOL*,EFI_GUID*,UINTN*,VOID*);
struct EFI_FILE_PROTOCOL { uint64_t Revision; EFI_FILE_OPEN Open; EFI_FILE_CLOSE Close; VOID *Delete; EFI_FILE_READ Read; VOID *Write,*GetPosition,*SetPosition; EFI_FILE_GET_INFO GetInfo; VOID *SetInfo,*Flush,*OpenEx,*ReadEx,*WriteEx,*FlushEx; };
typedef struct { uint64_t Revision; EFI_STATUS(*OpenVolume)(VOID*,EFI_FILE_PROTOCOL**); } EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct { uint32_t Version,HorizontalResolution,VerticalResolution,PixelFormat; struct { uint32_t RedMask,GreenMask,BlueMask,ReservedMask; } PixelInformation; uint32_t PixelsPerScanLine; } EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
typedef struct { uint32_t MaxMode,Mode; EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info; UINTN SizeOfInfo; EFI_PHYSICAL_ADDRESS FrameBufferBase; UINTN FrameBufferSize; } EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL { EFI_STATUS(*QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL*,uint32_t,UINTN*,EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**); EFI_STATUS(*SetMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL*,uint32_t); VOID *Blt; EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode; } EFI_GRAPHICS_OUTPUT_PROTOCOL;
#define PixelRedGreenBlueReserved8BitPerColor 0u
#define PixelBlueGreenRedReserved8BitPerColor 1u
#define PixelBitMask 2u
#define PixelBltOnly 3u
extern EFI_GUID gEfiLoadedImageProtocolGuid,gEfiSimpleFileSystemProtocolGuid,gEfiFileInfoGuid,gEfiGraphicsOutputProtocolGuid,gEfiAcpi20TableGuid,gEfiAcpi10TableGuid,gEfiSmbios3TableGuid,gEfiSmbiosTableGuid;
#endif
