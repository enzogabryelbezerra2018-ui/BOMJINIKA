// banner.c - BOMJINIKA kernel ASCII banner

const char *bomjinika_banner[] = {
"  ____   ___  __  __      _ _ _ _       ",
" | __ ) / _ \\|  \\/  | ___(_) (_) |_ ___ ",
" |  _ \\| | | | |\\/| |/ _ \\ | | | __/ _ \\",
" | |_) | |_| | |  | |  __/ | | | ||  __/",
" |____/ \\___/|_|  |_|\\___|_|_|_|\\__\\___|",
" ---------------------------------------",
"          B  O  M  J  I  N  I  K  A     ",
" ---------------------------------------",
""
};

// Função para imprimir o banner (usando printk do kernel)
void print_bomjinika_banner(void) {
    for (size_t i = 0; i < sizeof(bomjinika_banner) / sizeof(bomjinika_banner[0]); i++) {
        printk("%s\n", bomjinika_banner[i]);
    }
}
