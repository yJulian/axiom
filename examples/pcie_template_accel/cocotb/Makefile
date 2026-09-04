# cocotb testbench for pcie_template_top, via Verilator. See
# examples/fifo_pio_accel/cocotb/Makefile for the venv setup note -- same
# pattern here.

SIM ?= verilator
TOPLEVEL_LANG ?= verilog

AXI4_DIR := ../../../hw/axi4

VERILOG_SOURCES := \
	$(AXI4_DIR)/axi4_pkg.sv \
	$(AXI4_DIR)/axi4_if.sv \
	$(AXI4_DIR)/axi4_pins.sv \
	../pcie_template_accel.sv \
	../pcie_template_top.sv

TOPLEVEL := pcie_template_top
MODULE   := test_pcie_template

EXTRA_ARGS += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNOPTFLAT

include $(shell cocotb-config --makefiles)/Makefile.sim
