# cocotb testbench for fifo_pio_top, via Verilator -- the standalone
# (no-gem5) verification of this DUT. Needs cocotb + cocotbext-axi on
# PATH/PYTHONPATH -- run `scripts/setup_cocotb_env.sh` once, then either
# `source ../../../.venv/bin/activate` before `make`, or just run the
# repo root's `make tb` target, which activates the venv itself.

SIM ?= verilator
TOPLEVEL_LANG ?= verilog

AXI4_DIR := ../../../hw/axi4

VERILOG_SOURCES := \
	$(AXI4_DIR)/axi4_pkg.sv \
	$(AXI4_DIR)/axi4_if.sv \
	$(AXI4_DIR)/axi4_pins.sv \
	../fifo_pio_accel.sv \
	../fifo_pio_top.sv

TOPLEVEL := fifo_pio_top
MODULE   := test_fifo_pio

EXTRA_ARGS += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL

include $(shell cocotb-config --makefiles)/Makefile.sim
