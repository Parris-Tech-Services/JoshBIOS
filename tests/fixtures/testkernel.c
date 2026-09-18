/* Minimal ELF64 fixture: has .text, .rodata, .data and a large .bss so the
 * loader's memsz-beyond-filesz zeroing path is exercised. */
__attribute__((section(".text.entry")))
void _start(void) { for (;;) __asm__ volatile("hlt"); }

const char josh_marker[] = "JOSH_ELF64_FIXTURE_MARKER";
volatile unsigned int josh_data = 0xDEADBEEFu;
volatile unsigned char josh_bss[8192];   /* memsz > filesz */
