// kernel.c — BOMJINIKA Kernel (x86_64 / UEFI / C puro)
#include <efi.h>
#include <efilib.h>

/*
 * Entrada UEFI do kernel BOMJINIKA
 * Autor: Enzo / Gabriel
 * Descrição:
 *   Este é o primeiro estágio do kernel BOMJINIKA — totalmente em C.
 *   O firmware UEFI carrega este binário e chama efi_main().
 *   Aqui apenas imprimimos informações e mantemos o kernel ativo.
 */

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"\n========================================\n");
    Print(L"      🔷  BOMJINIKA Kernel (x86_64) 🔷\n");
    Print(L"========================================\n");
    Print(L"  Autor: Enzo / Gabriel\n");
    Print(L"  Linguagem: C puro\n");
    Print(L"  Plataforma: UEFI / x86_64\n");
    Print(L"  Status: M0 (boot inicial OK)\n");
    Print(L"----------------------------------------\n");
    Print(L"  Iniciando o kernel BOMJINIKA...\n\n");

    // Loop infinito — CPU em modo de espera
    for (;;) {
        __asm__ __volatile__("hlt");
    }

    return EFI_SUCCESS;
}
