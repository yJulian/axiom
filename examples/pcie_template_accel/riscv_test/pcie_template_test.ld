/* Linker script for the bare-metal PCIe template test.
 *
 * 0x80000000 is where the gem5 config script (../configs/run_pcie_template.py)
 * puts DRAM and points RiscvBareMetal's entry at. `.tohost` is pinned at
 * 0x80008000 -- the config script polls that exact physical address to
 * decide pass/fail, so the two must agree; the ASSERT below fails the
 * build rather than the simulation if the program ever grows into it.
 *
 * The stack top and the DMA test buffers live further up the same DRAM
 * range; keeping them 1 MiB apart is arbitrary but leaves the map obvious
 * at a glance (see pcie_template_test.c, which must agree on the buffer
 * addresses). */

OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;
  .text : { *(.text.init) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }

  . = 0x80008000;
  .tohost : { *(.tohost) }

  _stack_top = 0x80100000;

  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

ASSERT(ADDR(.tohost) == 0x80008000,
       ".tohost must stay at the address run_pcie_template.py polls")
