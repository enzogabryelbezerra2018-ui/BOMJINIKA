# Makefile — BOMJINIKA M1 (adaptar caminhos se necessário)
EFIINC = /usr/include/efi
EFIINC_ARCH = /usr/include/efi/x86_64
CFLAGS = -I$(EFIINC) -I$(EFIINC_ARCH) -fshort-wchar -fPIC -DEFI_FUNCTION_WRAPPER -Wall -O2 -ffreestanding
LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/gnu-efi/elf_x86_64_efi.lds -shared -Bsymbolic
LIBS = -lefi -lgnuefi

SRCS = kernel.c console.c
OBJS = $(SRCS:.c=.o)

.SUFFIXES: .c .o .so .efi

all: BOMJINIKA.efi

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

BOMJINIKA.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) $(LIBS) -o BOMJINIKA.so

BOMJINIKA.efi: BOMJINIKA.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rela -S -O binary BOMJINIKA.so BOMJINIKA.efi

clean:
	rm -f *.o *.so *.efi
