typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 row = 0;
static u32 col = 0;

static void clear_screen(void) {
    for (u32 i = 0; i < 80u * 25u; ++i) {
        VGA[i] = (u16)(' ' | (0x0Fu << 8));
    }
}

static void putc(char c, u8 colour) {
    if (c == '\n') {
        row++;
        col = 0;
        return;
    }
    if (col >= 80) {
        row++;
        col = 0;
    }
    if (row >= 25) {
        row = 0;
    }
    VGA[row * 80 + col] = (u16)((u8)c | ((u16)colour << 8));
    col++;
}

static void print(const char *s, u8 colour) {
    while (*s) {
        putc(*s++, colour);
    }
}

void kmain(void) {
    clear_screen();
    print("JoshBIOS\n", 0x0B);
    print("========\n\n", 0x08);
    print("Kernel entered successfully.\n", 0x0F);
    print("Stage 1 -> Stage 2 -> protected mode -> kernel\n\n", 0x07);
    print("Welcome to the beginning of JoshBIOS.\n", 0x0A);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
