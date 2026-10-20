/* ------------------------------------------------------------------
 * Type definitions
 * ------------------------------------------------------------------
 * These sizes assume a 32-bit RISC-V target (RV32), where:
 *   char = 8 bits, int = 32 bits, pointers = 32 bits.
 */

/* An unsigned 8-bit integer (one byte, range 0..255). Used to access
 * memory byte by byte. */
typedef unsigned char uint8_t;

/* An unsigned 32-bit integer */
typedef unsigned int uint32_t;

/* The type used for sizes and byte counts (what sizeof yields, and
 * what memset() takes as its length). */
typedef uint32_t size_t;


/* ------------------------------------------------------------------
 * Symbols provided by the linker script (kernel.ld)
 * ------------------------------------------------------------------
 * "extern" means "this is defined elsewhere; the linker will resolve
 * it". These three names are NOT variables stored in memory. They are
 * labels the linker script created (e.g. "__bss = .;"), so the
 * *address* of each symbol is the value we care about.
 *
 *   __bss        address where the .bss section starts
 *   __bss_end    address just past the end of .bss
 *   __stack_top  address just past the 128 KiB stack region
 *
 * Declaring them as arrays of char ("char x[]") is a common trick:
 * in an expression, an array name automatically becomes a pointer to
 * its first element, so writing "__bss" already gives the address,
 * with no "&" needed. Using "char" also means pointer arithmetic on
 * them moves one byte at a time - because each element of the array
 * is a char.
 *
 * Do not read or write through them as if they held data. Only use
 * their addresses.
 */
extern uint8_t __bss[], __bss_end[], __stack_top[];


/*
 * === NOTE ===
 * Linker symbols (like __bss) have no storage. They are just names
 * for an address, the same as a label in assembly. The C declaration
 * decides whether the compiler uses that address directly or reads
 * memory at it.
 *
 *   extern char __bss[];  (array, what we use)
 *       Writing "__bss" gives the ADDRESS. No memory is read.
 *       Same as "la a0, __bss" in assembly. This is what we want.
 *
 *   extern char __bss;    (single char)
 *       Writing "__bss" READS THE BYTE stored at that address.
 *       You must write "&__bss" to get the address, which is easy
 *       to forget. Works, but is error-prone.
 *
 *   extern char *__bss;   (pointer, WRONG)
 *       Writing "__bss" READS 4 BYTES stored at that address and
 *       treats them as a pointer. The linker symbol never held a
 *       pointer, so you get garbage (for example 0 after memset).
 *
 * Rule of thumb: for linker symbols, declare them as arrays and use
 * the name as the address.
 *
 * === __bss[] vs *__bss
 * extern char  __bss[];   // "an array of chars starts at 0x80201000"
 * extern char *__bss;     // "a variable at 0x80201000 holds a pointer"
 */


/* ------------------------------------------------------------------
 * memset: fill a block of memory with one byte value
 * ------------------------------------------------------------------
 * Parameters:
 *   buf  pointer to the start of the memory to fill.
 *   c    the byte value to write.
 *   n    how many bytes to write.
 *
 * Returns: the original pointer "buf", matching the standard C
 * memset.
 */
void *memset(uint8_t *buf, uint8_t c, size_t n) {

    uint8_t *p = buf;

    while (n--) {
        *p++ = c;
    }

    /* Hand back the original pointer. We return "buf" and not "p",
     * because p has been advanced to the end of the block. */
    return buf;

}


/* ------------------------------------------------------------------
 * kernel_main: the first C function that runs
 * ------------------------------------------------------------------
 * Reached from boot() below, once the stack is set up.
 *
 * return: see the final loop.
 */
void kernel_main(void) {

    /* Zero the whole .bss section. */
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    /* We loop forever because there is nothing to return to. boot()
     * entered this function with a plain jump ("j"), not a call, so
     * no return address was saved. Returning would send the CPU to
     * a garbage address. */
    for (;;);

}


/* ------------------------------------------------------------------
 * boot: the kernel's entry point
 * ------------------------------------------------------------------
 * This is the symbol named by ENTRY(boot) in the linker script. The
 * firmware (OpenSBI) jumps to it at 0x80200000.
 *
 * Attributes:
 *
 *   section(".text.boot")
 *       Put this function in the section named .text.boot instead
 *       of the default .text. The linker script has
 *       KEEP(*(.text.boot)) first in .text, so this code lands at
 *       the very start of the image, exactly at the entry address.
 *
 *   naked
 *       Tell the compiler not to generate a function prologue or
 *       epilogue (the code that normally allocates the stack frame
 *       and saves registers). That matters here because the stack
 *       pointer is not valid yet. Any compiler-generated stack
 *       access would write to a random address. With "naked", the
 *       function body is only our inline assembly.
 */
__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    /* Inline assembly. General form:
     *
     *   __asm__ __volatile__ (
     *       "assembly template"
     *       : output operands
     *       : input operands
     *       : clobbered registers/memory
     *   );
     *
     * "__asm__" embeds assembly in C. "__volatile__" tells the
     * compiler this statement has effects it can't see, so it must
     * not delete, duplicate or reorder it.
     */
    __asm__ __volatile__(
        /* "mv sp, <reg>" copies a register into sp (the stack pointer).
         * It is a pseudo-instruction for "addi sp, <reg>, 0".
         * From now on, function calls and local variables have a valid
         * stack. */
        "mv sp, %[stack_top]\n" // Set the stack pointer

        /* "j <label>" is an unconditional jump: a pseudo-instruction
         * for "jal x0, <label>".
         * Control moves to kernel_main and does not come back. */
        "j kernel_main\n"       // Jump to the kernel main function

        /* Output operands: none. */
        :

        /* Input operands, in the form
         *   [symbolic name] "constraint" (C expression)
         * - [stack_top]: the name used as %[stack_top] in the template.
         * - "r": constraint meaning "put the value in any general
         *   purpose register of the compiler's choice".
         * - (__stack_top): the C expression to pass in. Here it is the
         *   address of the top of the reserved stack region, as
         *   defined by the linker script. */
        : [stack_top] "r" (__stack_top) // Pass the stack top address as %[stack_top]
    );

}