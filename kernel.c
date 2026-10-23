/* Pulls in the typedefs (uint8_t, size_t) and the sbiret struct.
 * Quotes ("...") mean "look in our own project first", unlike
 * <...> which searches the system include paths. */
#include "kernel.h"



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
 * Declaring them as arrays of bytes ("uint8_t x[]") is a common
 * trick: in an expression, an array name automatically becomes a
 * pointer to its first element, so writing "__bss" already gives the
 * address, with no "&" needed. Using a one-byte element type also
 * means pointer arithmetic on them moves one byte at a time, because
 * each element of the array is one byte.
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
 * memory at it. (The examples below use "char"; "uint8_t" behaves
 * the same way, since it is also a one-byte type.)
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
 * sbi_call: ask the firmware (OpenSBI) to do something for us
 * ------------------------------------------------------------------
 * SBI (Supervisor Binary Interface) is the API the firmware offers to
 * the kernel. Our kernel runs in S-mode (supervisor), while OpenSBI
 * runs in M-mode (machine), the most privileged level. The kernel
 * asks for a service with the "ecall" instruction, which works like
 * a function call into the more privileged mode. When OpenSBI is
 * done, execution resumes at the instruction after the ecall.
 *
 * The calling convention is fixed by the SBI specification:
 *   a7        extension ID (EID): which group of functions to use.
 *   a6        function ID (FID): which function inside that group.
 *   a0 - a5   up to six arguments.
 *   ecall     transfers control to the firmware.
 *   a0        returned error code (0 = success).
 *   a1        returned value.
 * The firmware must preserve every other register, so a2 - a7 are
 * guaranteed to be unchanged after the call.
 *
 * Parameters (in the order of this C function, not the registers):
 *   arg0..arg5  the six arguments, passed in a0..a5.
 *   fid         function ID, passed in a6.
 *   eid         extension ID, passed in a7.
 *
 * Returns: an sbiret holding the error code (a0) and value (a1).
 *
 * "long" is used for everything because it is register-sized: 32 bits
 * on RV32 and 64 bits on RV64.
 */
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {

    /* "register long a0 __asm__("a0") = arg0;" asks the compiler to
     * keep this variable in the named register (a0). This GCC/Clang
     * extension is needed because ecall expects each argument in one
     * specific register. A plain "r" constraint alone would let the
     * compiler pick any register. */
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    /* The template is just "ecall": it has no operands of its own,
     * because the data already sits in the right registers.
     *
     * Outputs ("=r" means "written by the asm, in any register",
     * which here is forced to a0 and a1 by the variables above):
     *   a0, a1  are overwritten by the firmware with error and value.
     *
     * Inputs ("r" means "read by the asm, in a register"):
     *   a0 - a7 are the arguments. a0 and a1 appear both as inputs
     *   and outputs, because they carry an argument in and a result
     *   out. a2 - a7 are inputs only, because the firmware
     *   preserves them.
     *
     * Clobbers:
     *   "memory" tells the compiler the firmware may read or write
     *   memory (for example a buffer whose address we pass), so it
     *   must write back cached values before the call and re-read
     *   them after. */
    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");

    /* "(sbiret){ .error = a0, .value = a1 }" is a compound literal
     * with designated initializers: it builds a temporary sbiret and
     * names each field being set, so the order doesn't matter. */
    return (sbiret){.error = a0, .value = a1};

}


/* ------------------------------------------------------------------
 * putchar: print one character on the debug console
 * ------------------------------------------------------------------
 * Uses the SBI "Console Putchar" function: extension ID 0x01,
 * function ID 0, with the character as the first argument. It is
 * one of the original ("legacy") SBI calls. Newer versions of the
 * spec replace it with a Debug Console extension, but legacy calls
 * are still supported by the OpenSBI that ships with QEMU.
 *
 * What happens when you called:
 *   1. The kernel runs ecall and the CPU jumps into OpenSBI
 *      (M-mode trap handler).
 *   2. OpenSBI sees EID 1 and runs its Console Putchar code.
 *   3. OpenSBI's UART driver (an 8250-style UART) writes the byte
 *      to the serial port.
 *   4. QEMU receives it and prints it to your terminal.
 * The "driver" is in the firmware.
 *
 * Arguments to sbi_call: arg0 = ch (the character), arg1..arg5 = 0
 * (unused), fid = 0, eid = 1. The return value is ignored. The call
 * waits if the console is busy, and silently drops the character if
 * there is no console.
 */
void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
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

    const char *s = "\n\nHello World!\n";
    for (int i = 0; s[i] != '\0'; i++) {
        putchar(s[i]);
    }

    /* We loop forever because there is nothing to return to. boot()
     * entered this function with a plain jump ("j"), not a call, so
     * no return address was saved. Returning would send the CPU to
     * a garbage address.
     *
     * "wfi" (wait for interrupt) tells the CPU to idle until an
     * interrupt arrives, instead of spinning at full speed in an
     * empty loop. It is only a hint and can return early, which is
     * why it sits inside the loop. */
    for (;;) {
        __asm__ __volatile__("wfi");
    }

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
     *
     * The last section (clobbers) lists what the assembly changes
     * beyond the declared outputs, because the compiler can't read
     * the assembly. For example "t0" means the code overwrites t0,
     * so the compiler must not keep a live value there. "memory"
     * means it may read or write memory in ways the compiler can't
     * see, so cached values must be written back and re-read.
     *
     * This block needs no clobbers: it never returns to the C code,
     * so there is no compiler state left to protect.
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