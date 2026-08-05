# AXION top-level Makefile: submodule setup, RTL verilation, gem5 build,
# and running the worked FIFO/PIO example.
#
# Order matters: verilate before gem5 -- the src/ -> ext/gem5/src mirror
# (scripts/build_gem5.sh) needs to run *after* Verilator has produced
# examples/fifo_pio_accel/obj_dir/*.a so that gets mirrored in too; the
# `gem5` target re-runs the mirror itself so `make gem5` alone is always
# safe to run standalone as long as `make verilate` ran at some point.

GEM5_DIR   := ext/gem5
# `scons -C $(GEM5_DIR) build/RISCV/gem5.opt` places build/ at the
# directory scons was launched *from* (this repo's root), not inside
# the submodule -- same convention gem5_cva6 documents and relies on.
GEM5_BUILD := build/RISCV/gem5.opt
JOBS       ?= $(shell nproc)

.PHONY: submodules verilate gem5 run-fifo-example tb clean help

help:
	@echo "Targets: submodules verilate gem5 run-fifo-example tb clean"
	@echo "  make submodules            # git submodule update --init ext/gem5"
	@echo "  make verilate              # build examples/fifo_pio_accel's RTL"
	@echo "  make gem5                  # mirror src/ into gem5 + scons build"
	@echo "  make tb                    # standalone AXI4 testbench (no gem5)"
	@echo "  make run-fifo-example ARGS='--binary <elf>'"
	@echo "  make clean                 # remove RTL build artifacts + mirrors"

submodules:
	git submodule update --init $(GEM5_DIR)

verilate:
	$(MAKE) -C examples/fifo_pio_accel

tb:
	$(MAKE) -C examples/fifo_pio_accel run-tb

gem5: verilate
	scripts/build_gem5.sh
	scons -C $(GEM5_DIR) build/RISCV/gem5.opt -j$(JOBS)

run-fifo-example: gem5
	$(GEM5_BUILD) examples/fifo_pio_accel/configs/run_fifo_pio.py $(ARGS)

clean:
	$(MAKE) -C examples/fifo_pio_accel clean
	rm -rf $(GEM5_DIR)/src/axi $(GEM5_DIR)/src/cpu/rtl $(GEM5_DIR)/src/dev/rtl $(GEM5_DIR)/src/examples
