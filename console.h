// console.h - interface do console BOMJINIKA (serial + vga)
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void console_init(void);
void console_putc(char c);
void console_write(const char *s);
int printk(const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap); // utilitario
