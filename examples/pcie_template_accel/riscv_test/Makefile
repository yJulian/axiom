# Builds the bare-metal RISC-V guest program for the PCIe template
# example. Uses the host's riscv64-linux-gnu-gcc rather than a bare-metal
# (riscv-none-elf) toolchain, which this environment does not have --
# -nostdlib -nostartfiles -ffreestanding means no libc or CRT is ever
# linked in, so the "linux" target triple is irrelevant to the output.
#
# -static -no-pie is load-bearing, not cargo cult: without it the linker
# emits .interp/.dynsym/.rela.dyn ahead of our sections and pushes
# .tohost off the address the gem5 config script polls (the linker
# script's ASSERT catches that, but only after the fact).
# --build-id=none and -fno-asynchronous-unwind-tables just keep the map
# tidy, so _start lands exactly at 0x80000000 instead of behind a
# .note.gnu.build-id.

RISCV_GCC ?= riscv64-linux-gnu-gcc

CFLAGS = -march=rv64imafdc -mabi=lp64d -mcmodel=medany \
         -static -no-pie -nostdlib -nostartfiles -ffreestanding \
         -fno-asynchronous-unwind-tables -Wl,--build-id=none \
         -O2 -Wall -Wextra

TARGET = pcie_template_test.elf

.PHONY: all clean
all: $(TARGET)

$(TARGET): pcie_template_test.c pcie_template_test.ld
	$(RISCV_GCC) $(CFLAGS) -T pcie_template_test.ld $< -o $@

clean:
	rm -f $(TARGET)
