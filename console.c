// console.c - implementação do console BOMJINIKA
#include "console.h"

/* 
 * Implementação mínima:
 * - Serial (COM1) via I/O ports (0x3F8).
 * - VGA text mode (0xB8000) como fallback.
 * - printk com vsnprintf simples (suporta: %s %d %u %x %lx %p %c %%).
 */

#define COM1_PORT 0x3F8
#define VGA_TEXT_BUF ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static int serial_ok = 0;
static int vga_row = 0, vga_col = 0;
static uint8_t vga_attr = 0x07; // white on black

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init_port(void) {
    // configurar COM1: 115200, 8N1
    outb(COM1_PORT + 1, 0x00);    // disable all interrupts
    outb(COM1_PORT + 3, 0x80);    // enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x01);    // divisor low byte (115200)
    outb(COM1_PORT + 1, 0x00);    // divisor high byte
    outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7);    // enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set

    // taste: verificar se porta responde (ler line status)
    unsigned char lsr = inb(COM1_PORT + 5);
    (void)lsr; // não 100% confiável no ambiente UEFI, mas tentamos
    serial_ok = 1; // assumimos habilitado; QEMU direcionará
}

static int serial_can_write(void) {
    // bit 5 of LSR is Transmitter Holding Register Empty
    return inb(COM1_PORT + 5) & 0x20;
}

static void serial_putc(char c) {
    if (!serial_ok) return;
    // esperar espaço
    while (!serial_can_write()) { __asm__ __volatile__("pause"); }
    outb(COM1_PORT, (unsigned char)c);
}

static void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_ROWS) vga_row = 0; // simples rollover
        return;
    } else if (c == '\r') {
        vga_col = 0;
        return;
    } else if (c == '\t') {
        int spaces = 4 - (vga_col % 4);
        while (spaces--) {
            vga_putc(' ');
        }
        return;
    }

    uint16_t pos = vga_row * VGA_COLS + vga_col;
    VGA_TEXT_BUF[pos] = ((uint16_t)vga_attr << 8) | (uint8_t)c;
    vga_col++;
    if (vga_col >= VGA_COLS) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_ROWS) vga_row = 0;
    }
}

void console_init(void) {
    // tenta inicializar serial; se falhar, usaremos VGA na mesma
    serial_init_port();
    // limpa tela VGA
    for (int r = 0; r < VGA_ROWS; ++r) {
        for (int c = 0; c < VGA_COLS; ++c) {
            VGA_TEXT_BUF[r * VGA_COLS + c] = ((uint16_t)vga_attr << 8) | ' ';
        }
    }
    vga_row = 0; vga_col = 0;
}

void console_putc(char c) {
    if (serial_ok) serial_putc(c);
    vga_putc(c);
}

void console_write(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}

/* Minimal vsnprintf implementation - restrito, seguro e suficiente para debug.
   Suporta: %s, %c, %d, %u, %x, %lx, %p, %%.
*/
static void reverse_str(char *s, int len) {
    for (int i = 0, j = len-1; i < j; ++i, --j) {
        char t = s[i]; s[i] = s[j]; s[j] = t;
    }
}

static int utoa_base(unsigned long val, char *buf, int base, int lowercase) {
    const char *digits_up = "0123456789ABCDEF";
    const char *digits_lo = "0123456789abcdef";
    const char *digits = lowercase ? digits_lo : digits_up;
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return i;
    }
    while (val) {
        int d = val % base;
        buf[i++] = digits[d];
        val /= base;
    }
    buf[i] = '\0';
    reverse_str(buf, i);
    return i;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;
    const char *p = fmt;
    char numbuf[32];
    while (*p && pos + 1 < size) {
        if (*p != '%') {
            buf[pos++] = *p++;
            continue;
        }
        p++; // skip '%'
        if (*p == '\0') break;
        if (*p == '%') {
            buf[pos++] = '%';
            p++; continue;
        }
        if (*p == 'c') {
            char c = (char)va_arg(ap, int);
            buf[pos++] = c;
            p++; continue;
        }
        if (*p == 's') {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && pos + 1 < size) buf[pos++] = *s++;
            p++; continue;
        }
        int longflag = 0;
        if (*p == 'l') { longflag = 1; p++; }
        char spec = *p++;
        if (spec == 'd' || spec == 'i') {
            long val = longflag ? va_arg(ap, long) : va_arg(ap, int);
            unsigned long u;
            int neg = 0;
            if (val < 0) { neg = 1; u = (unsigned long)(-val); } else u = (unsigned long)val;
            utoa_base(u, numbuf, 10, 0);
            int i = 0; if (neg && pos + 1 < size) buf[pos++] = '-';
            while (numbuf[i] && pos + 1 < size) buf[pos++] = numbuf[i++];
            continue;
        } else if (spec == 'u') {
            unsigned long val = longflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            utoa_base(val, numbuf, 10, 0);
            int i = 0;
            while (numbuf[i] && pos + 1 < size) buf[pos++] = numbuf[i++];
            continue;
        } else if (spec == 'x' || spec == 'X') {
            unsigned long val = longflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            utoa_base(val, numbuf, 16, (spec == 'x'));
            int i = 0;
            while (numbuf[i] && pos + 1 < size) buf[pos++] = numbuf[i++];
            continue;
        } else if (spec == 'p') {
            void *ptr = va_arg(ap, void*);
            unsigned long val = (unsigned long)ptr;
            // print 0x + hex
            if (pos + 3 < size) { buf[pos++] = '0'; buf[pos++] = 'x'; }
            utoa_base(val, numbuf, 16, 1);
            int i = 0;
            while (numbuf[i] && pos + 1 < size) buf[pos++] = numbuf[i++];
            continue;
        } else {
            // unsupported specifier: print it literally
            buf[pos++] = '%';
            if (longflag && pos + 1 < size) buf[pos++] = 'l';
            if (pos + 1 < size) buf[pos++] = spec;
        }
    }
    buf[pos] = '\0';
    return (int)pos;
}

int printk(const char *fmt, ...) {
    char outbuf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(outbuf, sizeof(outbuf), fmt, ap);
    va_end(ap);
    console_write(outbuf);
    return n;
}
