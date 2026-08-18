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

.PHONY: submodules verilate verilate-dma gem5 run-fifo-example \
        run-dma-example tb tb-dma clean help verilate-plugin tb-plugin \
        cocotb-env

help:
	@echo "Targets: submodules verilate verilate-dma gem5 run-fifo-example"
	@echo "         run-dma-example tb tb-dma clean verilate-plugin tb-plugin"
	@echo "         cocotb-env"
	@echo "  make submodules            # git submodule update --init ext/gem5"
	@echo "  make verilate              # build examples/fifo_pio_accel's RTL"
	@echo "  make verilate-dma          # build examples/dma_memcopy_accel's RTL"
	@echo "  make gem5                  # mirror src/ into gem5 + scons build"
	@echo "                             #   (includes both examples once verilated)"
	@echo "  make tb                    # cocotb FIFO/PIO AXI4 testbench, no gem5 needed"
	@echo "  make tb-dma                # cocotb DMA memcopy AXI4 testbench, no gem5 needed"
	@echo "  make run-fifo-example ARGS='--binary <elf>'"
	@echo "  make run-dma-example ARGS='--binary <elf>'"
	@echo "  make verilate-plugin       # build the FIFO DUT as a plugin .so"
	@echo "                             #   (opt-in dlopen path, see plugin/)"
	@echo "  make tb-plugin             # standalone plugin-ABI testbench"
	@echo "  make cocotb-env            # create .venv/ + install cocotb deps"
	@echo "  make clean                 # remove RTL build artifacts + mirrors"

submodules:
	git submodule update --init $(GEM5_DIR)

verilate:
	$(MAKE) -C examples/fifo_pio_accel

verilate-dma:
	$(MAKE) -C examples/dma_memcopy_accel

gem5: verilate verilate-dma
	scripts/build_gem5.sh
	scons -C $(GEM5_DIR) build/RISCV/gem5.opt -j$(JOBS)

run-fifo-example: gem5
	$(GEM5_BUILD) examples/fifo_pio_accel/configs/run_fifo_pio.py $(ARGS)

run-dma-example: gem5
	$(GEM5_BUILD) examples/dma_memcopy_accel/configs/run_dma_memcopy.py $(ARGS)

# Opt-in "plugin" path (dlopen-based, see plugin/rtl_plugin.mk): builds the
# same fifo_pio_accel DUT into a .so instead of linking it into gem5.opt,
# and verifies it standalone -- independent of both `verilate`/`gem5`
# above and of each other.
verilate-plugin:
	$(MAKE) -C examples/fifo_pio_accel_plugin

tb-plugin:
	$(MAKE) -C examples/fifo_pio_accel_plugin run-tb

# Standalone (no-gem5) AXI4 protocol testbenches, cocotb + cocotbext-axi
# (examples/*/cocotb/), independent of both gem5 and each other. Needs the
# venv from `make cocotb-env` (see scripts/setup_cocotb_env.sh) on PATH.
VENV_BIN := .venv/bin

cocotb-env:
	scripts/setup_cocotb_env.sh

tb: cocotb-env
	PATH="$(abspath $(VENV_BIN)):$$PATH" $(MAKE) -C examples/fifo_pio_accel/cocotb

tb-dma: cocotb-env
	PATH="$(abspath $(VENV_BIN)):$$PATH" $(MAKE) -C examples/dma_memcopy_accel/cocotb

clean:
	$(MAKE) -C examples/fifo_pio_accel clean
	$(MAKE) -C examples/dma_memcopy_accel clean
	$(MAKE) -C examples/fifo_pio_accel_plugin clean
	rm -rf examples/fifo_pio_accel/cocotb/sim_build examples/fifo_pio_accel/cocotb/results.xml
	rm -rf examples/dma_memcopy_accel/cocotb/sim_build examples/dma_memcopy_accel/cocotb/results.xml
	rm -rf $(GEM5_DIR)/src/axi $(GEM5_DIR)/src/cpu/rtl $(GEM5_DIR)/src/dev/rtl $(GEM5_DIR)/src/examples
