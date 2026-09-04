/* Minimal static-ELF linker script for the SE-mode fifo_pio_mmio_test.
 * 0x10000 is a conventional low, fixed base for statically-linked riscv SE-mode
 * test binaries -- well clear of FIFO_BASE (0x10010000) and gem5 SE's own
 * auto-mapped stack region (high addresses), so no collision risk. */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x10000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
