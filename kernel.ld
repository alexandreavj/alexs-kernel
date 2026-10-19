ENTRY(boot)
/* Tells the linker (and other tools like gdb) that execution starts
   at the symbol "boot". This gets recorded in the ELF header (first
   chunk of data of an Executable and Linkable Format file) as the
   entry point. OpenSBI jumps to this address when it hands off control. */

SECTIONS {
    /* The SECTIONS block defines how input sections from your .o files
       get arranged into the final output binary's layout. */

    . = 0x80200000;
    /* "." is the location counter — the linker's current address as where
       we are standing in the code. This sets the starting address of the
       whole kernel image to 0x80200000, which is where QEMU's "virt"
       machine and OpenSBI expect the supervisor-mode kernel to be loaded. */


    /* The text block defines the .text output section: where the actual
       machine code instructions from the source code live. */
    .text : {
        KEEP(*(.text.boot));
        /* Place the .text.boot section first, from any input file ("*").
           This should contain your boot/entry code (the "boot" symbol),
           so it ends up at the very start of .text — i.e., at 0x80200000,
           matching ENTRY(boot). KEEP() tells the linker not to discard
           this section even if garbage collection (--gc-sections) is on,
           since nothing else visibly "calls" it — control just jumps
           here from outside the program. */

        *(.text .text.*);
        /* Then place all other .text sections (regular code) and any
           .text.* sections (e.g., .text.foo from function-level sections)
           from every input file, right after .text.boot. */
    }


    /* The rodata block defines the .rodata output section: read-only data,
       such as string literals, const arrays, etc.. */
    .rodata : ALIGN(4) {
        /* ALIGN(4) forces this section to start on a 4-byte boundary
           for correct access alignment. */

        *(.rodata .rodata.*);
        /* Read-only data: string literals, const arrays, etc., from
           every input file. */
    }


    /* The data block defines the .data output section: Initialized global/
       static variables (non-zero initial values), from every input file.. */
    .data : ALIGN(4) {
        *(.data .data.*);
        /* Initialized global/static variables (non-zero initial values),
           from every input file. These need actual storage in the
           binary since they have specific starting values. */
    }


    /* The bss block defines the .bss output section: uninitialized global/
       static variables, which occupy no space in the file itself since
       they're implicitly zero-filled at runtime. */
    .bss : ALIGN(4) {
        __bss = .;
        /* Record the current address (start of this section) into the
           symbol __bss, so your startup code can find where .bss begins. */

        *(.bss .bss.* .sbss .sbss.*);
        /* Uninitialized global/static variables (.bss), plus "small bss"
           (.sbss) sections, which some RISC-V toolchains use for small
           data objects accessed via the global pointer register. These
           occupy no space in the file itself — only in memory at
           runtime — since they're implicitly zero. */

        __bss_end = .;
        /* Record the address after all .bss content into __bss_end.
           Your boot code typically loops from __bss to __bss_end and
           writes zeros, since the ELF loader doesn't do this for you
           in this bare-metal setup. */
    }


    . = ALIGN(4);
    /* Re-align the location counter to a 4-byte boundary before
       reserving stack space. */

    . += 128 * 1024;
    /* Advance the location counter by 128 KiB without placing any
       actual section content here. This reserves 128 KB of address
       space to be used as the kernel's stack — nothing is emitted
       into the binary for it. */

    __stack_top = .;
    /* After advancing by 128 KB, record this address as __stack_top.
       Since RISC-V (like most architectures) grows its stack
       downward, your boot code sets the stack pointer (sp) register
       to __stack_top, so the stack grows down from the top of that
       128 KB region back toward __bss_end. */

}