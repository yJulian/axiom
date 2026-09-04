# cocotb testbench for pcie_tlp_template_top, via Verilator. See
# examples/fifo_pio_accel/cocotb/Makefile for the venv setup note -- same
# pattern here.

SIM ?= verilator
TOPLEVEL_LANG ?= verilog

PCIE_DIR := ../../../hw/pcie

VERILOG_SOURCES := \
	$(PCIE_DIR)/pcie_tlp_pkg.sv \
	$(PCIE_DIR)/pcie_tlp_if.sv \
	$(PCIE_DIR)/pcie_tlp_pins.sv \
	../pcie_tlp_template_accel.sv \
	../pcie_tlp_template_top.sv

TOPLEVEL := pcie_tlp_template_top
MODULE   := test_pcie_tlp_template

EXTRA_ARGS += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNOPTFLAT

include $(shell cocotb-config --makefiles)/Makefile.sim
