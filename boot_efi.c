/* boot_efi.c - 64-bit UEFI Bootloader for Cravgos (x86_64)
 * Компилируется через: gcc -nostdlib -m64 -O2 "-Wl,--subsystem,10" -e efi_main boot_efi.c -o bootx64.efi
 */

typedef unsigned long long UINTN;
typedef long long INTN;
typedef unsigned int UINT32;
typedef void *EFI_HANDLE;
typedef UINTN EFI_STATUS;

#define EFI_SUCCESS 0

typedef struct {
    UINTN Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (__attribute__((ms_abi)) *OutputString)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const unsigned short *String);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    EFI_STATUS (__attribute__((ms_abi)) *SetAttribute)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Attribute);
    EFI_STATUS (__attribute__((ms_abi)) *ClearScreen)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct _EFI_INPUT_KEY {
    unsigned short ScanCode;
    unsigned short UnicodeChar;
} EFI_INPUT_KEY;

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (__attribute__((ms_abi)) *ReadKeyStroke)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);
    void *WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* EFI GUID */
typedef struct {
    unsigned int Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} EFI_GUID;

/* Graphics Output Protocol (GOP) */
typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelFormat;
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    UINTN FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void *QueryMode;
    void *SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    void *AllocatePages;
    void *FreePages;
    void *GetMemoryMap;
    void *AllocatePool;
    void *FreePool;
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    EFI_STATUS (__attribute__((ms_abi)) *LocateHandle)(int SearchType, const EFI_GUID *Protocol, void *SearchKey, UINTN *BufferSize, EFI_HANDLE *Buffer);
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_STATUS (__attribute__((ms_abi)) *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void *GetNextMonotonicCount;
    void *Stall;
    void *SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    EFI_STATUS (__attribute__((ms_abi)) *LocateProtocol)(const EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    unsigned short *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

static const EFI_GUID gop_guid = { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };

/* Печать шестнадцатеричного числа */
static void put_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, UINTN val, int digits) {
    unsigned short buf[17];
    const unsigned short hex[] = u"0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[digits] = 0;
    out->OutputString(out, buf);
}

/* Печать десятичного числа */
static void put_dec(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, UINTN val) {
    unsigned short buf[24];
    int i = 0;
    if (val == 0) { out->OutputString(out, u"0"); return; }
    while (val > 0) {
        buf[i++] = u'0' + (val % 10);
        val /= 10;
    }
    for (int j = 0; j < i / 2; j++) {
        unsigned short t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    buf[i] = 0;
    out->OutputString(out, buf);
}

EFI_STATUS __attribute__((ms_abi)) efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out = SystemTable->ConOut;
    if (!out) return EFI_SUCCESS;

    out->ClearScreen(out);
    out->SetAttribute(out, 0x0E); /* Желтый / Amber */

    out->OutputString(out, u"=================================================================\r\n");
    out->OutputString(out, u"       CRAVGOS 0.7.0 (x86_64) - UEFI 64-BIT BOOTLOADER           \r\n");
    out->OutputString(out, u"=================================================================\r\n\r\n");

    out->SetAttribute(out, 0x0F); /* Белый */
    out->OutputString(out, u"Firmware Vendor: ");
    if (SystemTable->FirmwareVendor) out->OutputString(out, SystemTable->FirmwareVendor);
    out->OutputString(out, u"\r\n");

    out->OutputString(out, u"Firmware Revision: ");
    put_hex(out, SystemTable->FirmwareRevision, 8);
    out->OutputString(out, u"\r\n\r\n");

    /* Поиск GOP (Graphics Output Protocol) */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS st = SystemTable->BootServices->LocateProtocol(&gop_guid, 0, (void **)&gop);
    if (st == EFI_SUCCESS && gop && gop->Mode && gop->Mode->Info) {
        out->SetAttribute(out, 0x0A); /* Зеленый */
        out->OutputString(out, u"[ OK ] UEFI Graphics Output Protocol (GOP) detected!\r\n");
        out->SetAttribute(out, 0x07);
        out->OutputString(out, u"  Resolution:    ");
        put_dec(out, gop->Mode->Info->HorizontalResolution);
        out->OutputString(out, u" x ");
        put_dec(out, gop->Mode->Info->VerticalResolution);
        out->OutputString(out, u"\r\n");

        out->OutputString(out, u"  Framebuffer:   0x");
        put_hex(out, gop->Mode->FrameBufferBase, 16);
        out->OutputString(out, u" (");
        put_dec(out, gop->Mode->FrameBufferSize / (1024 * 1024));
        out->OutputString(out, u" MiB)\r\n");

        out->OutputString(out, u"  Pixel Pitch:   ");
        put_dec(out, gop->Mode->Info->PixelsPerScanLine);
        out->OutputString(out, u" pixels/line\r\n\r\n");
    } else {
        out->SetAttribute(out, 0x0C);
        out->OutputString(out, u"[INFO] Standard UEFI Text Console Mode active\r\n\r\n");
    }

    out->SetAttribute(out, 0x0E);
    out->OutputString(out, u"Cravgos UEFI environment initialized successfully.\r\n");
    out->SetAttribute(out, 0x07);
    out->OutputString(out, u"Press any key to continue...\r\n");

    /* Ожидание клавиши */
    if (SystemTable->ConIn) {
        EFI_INPUT_KEY key;
        while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key) != EFI_SUCCESS) {
            __asm__ volatile("pause");
        }
    }

    return EFI_SUCCESS;
}
