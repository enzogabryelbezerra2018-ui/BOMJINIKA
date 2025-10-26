// apic.h - BOMJINIKA APIC interface (x86_64)
#pragma once
#include <stdint.h>
#include <stddef.h>

#define IA32_APIC_BASE_MSR 0x1B
#define APIC_DEFAULT_PHYS_BASE 0xFEE00000UL
#define IOAPIC_DEFAULT_PHYS_BASE 0xFEC00000UL

/* Inicializa o local APIC (ativa, seta SVR) */
void lapic_init(void);

/* Le/Escreve registro do LAPIC (32-bit) */
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);

/* Envia IPI para CPU destino (apic id), vector: 8..255 (vector number) */
void apic_send_ipi(uint8_t dest_apic_id, uint8_t vector);

/* Endereço base lido do MSR (físico). */
uintptr_t lapic_get_base_phys(void);

/* IO-APIC helpers (regsel/Window MMIO) */
uint32_t ioapic_read(uintptr_t ioapic_base, uint8_t reg);
void ioapic_write(uintptr_t ioapic_base, uint8_t reg, uint32_t val);

/* EOI - end of interrupt */
static inline void apic_send_eoi(void);
