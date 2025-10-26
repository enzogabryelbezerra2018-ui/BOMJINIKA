// apic.c - BOMJINIKA APIC implementation (x86_64)
// Assumptions:
//  - Kernel is running in a stage where CPU supports rdmsr/wrmsr.
//  - If paging is enabled, APIC phys base must be identity-mapped or this
//    code must be called after mapping phys->virt accordingly.
//  - This file is freestanding C with minimal inline asm.

#include "apic.h"
#include <stdint.h>

/* --- util: rdmsr / wrmsr --- */
static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

/* --- I/O port access (utilitário, caso precise usar PIC legacy) --- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* --- LAPIC MMIO access helpers --- */
/* Offsets are bytes from APIC base. All registers are 32-bit. */
#define LAPIC_REG_ID         0x020
#define LAPIC_REG_VERSION    0x030
#define LAPIC_REG_TPR        0x080
#define LAPIC_REG_EOI        0x0B0
#define LAPIC_REG_SVR        0x0F0
#define LAPIC_REG_ICR_LOW    0x300
#define LAPIC_REG_ICR_HIGH   0x310
#define LAPIC_REG_LVT_TIMER  0x320
#define LAPIC_REG_LVT_ERROR  0x370

/* volatile pointer to APIC base (assumes identity mapped or already mapped) */
static volatile uint32_t *lapic = (volatile uint32_t *)APIC_DEFAULT_PHYS_BASE;
static uintptr_t lapic_phys_base_cached = APIC_DEFAULT_PHYS_BASE;

/* If paging remaps APIC physical base to a virtual address, set lapic to that virt address.
   For now we default to identity mapping at APIC_DEFAULT_PHYS_BASE. */
void lapic_set_base_virt(void *virt_addr, uintptr_t phys_base)
{
    lapic = (volatile uint32_t *)virt_addr;
    lapic_phys_base_cached = phys_base;
}

uintptr_t lapic_get_base_phys(void)
{
    // Tenta ler MSR para obter base real (físico).
    uint64_t msr = rdmsr(IA32_APIC_BASE_MSR);
    uintptr_t base = (uintptr_t)(msr & 0xFFFFF000ULL);
    if (base != 0) return base;
    return lapic_phys_base_cached;
}

void lapic_write(uint32_t reg, uint32_t value)
{
    volatile uint32_t *addr = (volatile uint32_t *)((uintptr_t)lapic + reg);
    *addr = value;
    // Ler um registro qualquer para serializar (ordering)
    (void)lapic_read(LAPIC_REG_ID);
}

uint32_t lapic_read(uint32_t reg)
{
    volatile uint32_t *addr = (volatile uint32_t *)((uintptr_t)lapic + reg);
    return *addr;
}

/* --- EOI --- */
static inline void apic_send_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

/* --- Ativa/Configura o Local APIC ---
   Passos básicos:
   - Ler MSR IA32_APIC_BASE para obter base física e checar se APIC está habilitado.
   - Se não habilitado, setar bit enable no MSR.
   - Configurar SVR (Spurious Interrupt Vector Register) para habilitar o APIC e setar vetor de spurious (ex: 0xFF).
*/
void lapic_init(void)
{
    uint64_t apic_msr = rdmsr(IA32_APIC_BASE_MSR);
    uintptr_t phys_base = (uintptr_t)(apic_msr & 0xFFFFF000ULL);

    if (phys_base == 0) {
        // fallback ao default
        phys_base = APIC_DEFAULT_PHYS_BASE;
    }

    // se bit 11 (APIC global enable) não está setado, setamos
    if (!(apic_msr & (1ULL << 11))) {
        apic_msr |= (1ULL << 11); // enable APIC
        wrmsr(IA32_APIC_BASE_MSR, apic_msr);
    }

    lapic_phys_base_cached = phys_base;

    // Se o kernel usa paginação com kernel virtual diferent do físico,
    // chame lapic_set_base_virt(virt_addr, phys_base) antes de usar.

    // Ajusta ponteiro lapic só se o endereço físico for igual ao mapeamento esperado
    // (usuário pode chamar lapic_set_base_virt se necessário).
    lapic = (volatile uint32_t *)(uintptr_t)phys_base;

    // Setar SVR: bit 8 = APIC enabled, bits 0-7 = spurious vector
    // Escolhemos 0xFF (255) como vetor spurious simples — adapte conforme sua IDT.
    uint32_t svr = 0xFF | (1 << 8);
    lapic_write(LAPIC_REG_SVR, svr);

    // opcional: clear error status (escrita em LVT error)
    lapic_write(LAPIC_REG_LVT_ERROR, 0x10000); // write 1 to clear error (way many APICs)
}

/* --- IPI (Inter-Processor Interrupt) ---
   Escreve o ICR high (dest APIC ID) e ICR low (delivery mode/vector).
   Delivery mode 0 = Fixed.
*/
void apic_send_ipi(uint8_t dest_apic_id, uint8_t vector)
{
    // preparar high dword: dest APIC ID << 24
    uint32_t icr_high = ((uint32_t)dest_apic_id) << 24;
    uint32_t icr_low = 0;

    // vector
    icr_low = (uint32_t)vector;

    // delivery mode = fixed (0), dest shorthand = none (bits default 0)
    // bit 12 = trigger mode (0 edge), bit 11 = level (0)
    // ensure previous send finished: check delivery status (bit 12 of ICR low)
    // Delivery status (bit 12) = 0 means ready. Actually bit 12 is set while sending.

    // Wait for previous send complete
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1 << 12)) { __asm__ __volatile__("pause"); }

    lapic_write(LAPIC_REG_ICR_HIGH, icr_high);
    lapic_write(LAPIC_REG_ICR_LOW, icr_low);

    // wait again for completion
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1 << 12)) { __asm__ __volatile__("pause"); }
}

/* --- IO-APIC helpers ---
   IO-APIC register layout (regsel at base + 0x00, data at base + 0x10).
   Redirection entries start at index 0x10 (two 32-bit words).
*/
uint32_t ioapic_read(uintptr_t ioapic_base, uint8_t reg)
{
    volatile uint32_t *ioapic = (volatile uint32_t *)ioapic_base;
    volatile uint32_t *regsel = (volatile uint32_t *)((uintptr_t)ioapic + 0x00);
    volatile uint32_t *win = (volatile uint32_t *)((uintptr_t)ioapic + 0x10);
    *regsel = reg;
    // Serializing read
    uint32_t v = *win;
    return v;
}

void ioapic_write(uintptr_t ioapic_base, uint8_t reg, uint32_t val)
{
    volatile uint32_t *ioapic = (volatile uint32_t *)ioapic_base;
    volatile uint32_t *regsel = (volatile uint32_t *)((uintptr_t)ioapic + 0x00);
    volatile uint32_t *win = (volatile uint32_t *)((uintptr_t)ioapic + 0x10);
    *regsel = reg;
    *win = val;
}

/* Exemplo: programa uma entrada de redirecionamento (irq -> vector) no IO-APIC
   redir_idx: IRQ number
   vector: vetor IDT
   dest_apic_id: target APIC id (ou 0 para CPU0)
*/
void ioapic_set_redirection(uintptr_t ioapic_base, uint8_t redir_idx, uint64_t entry)
{
    // cada redirection entry ocupa 2 registros (low, high)
    uint8_t reg_low = 0x10 + redir_idx * 2;
    uint8_t reg_high = reg_low + 1;
    ioapic_write(ioapic_base, reg_high, (uint32_t)(entry >> 32));
    ioapic_write(ioapic_base, reg_low, (uint32_t)(entry & 0xFFFFFFFF));
}

/* --- Nota ---
   Este arquivo fornece funções básicas. Antes de usá-las em um kernel com
   paginação, certifique-se de mapear o endereço físico do LAPIC (ex: 0xFEE00000)
   para um endereço virtual acessível e chame lapic_set_base_virt() com o
   ponteiro virtual retornado.
*/
