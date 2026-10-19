#!/bin/bash

# Shell options:
#   -x  print each command before running it
#   -u  treat unset variables as errors
#   -e  exit immediately if any command fails
set -xue

# The emulator that runs our kernel: a 32-bit RISC-V system emulator.
QEMU=qemu-system-riscv32

# Path to clang and compiler flags
CC=clang

# Compiler flags, one by one:
#   -std=c11                   Use the C11 language standard.
#   -O2                        Optimize for speed (level 2).
#   -g3                        Include the most detailed debug info
#                              (symbols, line numbers, macros), so GDB
#                              can show source code.
#   -Wall -Wextra              Enable most warnings (and some extra
#                              ones) to catch likely mistakes.
#   --target=riscv32-unknown-elf
#                              Cross-compile: generate 32-bit RISC-V code
#                              for a bare-metal target (no OS, ELF output),
#                              not for the machine we are running on.
#   -fuse-ld=lld               Link with LLVM's linker (lld) instead of
#                              the system default.
#   -fno-stack-protector       Do not add stack canaries. They need
#                              runtime support (a guard variable and a
#                              failure handler) that doesn't exist yet in
#                              our kernel. Revisit this when adding
#                              hardening.
#   -ffreestanding             Do not assume a hosted environment: the
#                              standard library may not exist, and
#                              "main" has no special meaning. We provide
#                              our own memset, etc.
#   -nostdlib                  Do not link the standard C library or the
#                              default startup files. The linker script
#                              and boot() replace them.
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"

# Build the kernel
#   -Wl,<opts>       Pass the comma-separated options to the linker.
#   -Wl,-Tkernel.ld  Use kernel.ld as the linker script (memory layout,
#                    entry point, stack region).
#   -Wl,-Map=kernel.map
#                    Write a map file showing where every section and
#                    symbol ended up. Useful to check that .text.boot is
#                    at 0x80200000 and to see __bss, __bss_end and
#                    __stack_top.
#   -o kernel.elf    Name of the output file (an ELF executable).
#   kernel.c         Source file(s) to compile and link.
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel.c

# Start QEMU
#   -machine virt    Emulate QEMU's generic "virt" RISC-V board (UART,
#                    virtio devices, RAM starting at 0x80000000).
#   -bios default    Use the OpenSBI firmware that ships with QEMU. It
#                    runs first, then jumps to the kernel at 0x80200000.
#   -nographic       No graphical window. Serial output appears in this
#                    terminal.
#   -serial mon:stdio
#                    Connect the serial port to the terminal and share
#                    it with the QEMU monitor. Press Ctrl-A then X to
#                    quit QEMU, or Ctrl-A then C to switch to the monitor.
#   --no-reboot      If the guest tries to reboot (for example after a
#                    crash), exit instead of restarting. Makes failures
#                    visible instead of looping forever.
#   -kernel kernel.elf
#                    Load our ELF file into memory at the addresses in
#                    its program headers, and make it the next stage
#                    after OpenSBI.
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -kernel kernel.elf