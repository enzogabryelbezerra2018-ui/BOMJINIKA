// kernel.c — BOMJINIKA M1 (x86_64, UEFI loader -> console serial + VGA + printk)
#include <efi.h>
#include <efilib.h>
#include "console.h"

EFI_STATUS efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    // Inicializa nossa console (serial + VGA)
    console_init();

    printk("\n========================================\n");
    printk("  BOMJINIKA Kernel (x86_64) - M1\n");
    printk("  Consoles: SERIAL (COM1) + VGA fallback\n");
    printk("  Pronúncia: bomginica\n");
    printk("========================================\n\n");

    printk("Console inicializada. Teste de printf:\n");
    printk("inteiro: %d, hex: 0x%x, string: %s, char: %c\n", 12345, 0xdeadbeef, "olá BOMJINIKA", 'B');

    // loop principal — hlt para poupar CPU
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }

    return EFI_SUCCESS;
}
