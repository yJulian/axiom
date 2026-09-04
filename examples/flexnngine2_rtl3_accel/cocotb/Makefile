# cocotb testbench for flexnngine2_rtl3_top (the scratchpad-fed FleXNNgine2
# DMA GEMM accelerator, "rtl3" -- see ../README.md), via Verilator. Same
# pattern as examples/dma_memcopy_accel/cocotb/Makefile, except the DUT's
# own top module is used directly as TOPLEVEL -- no AXION axi4_pins wrapper
# exists for rtl3 yet (see ../README.md for what that would take), so this
# testbench drives flexnngine2_rtl3_top's s_axi_*/m_axi_* pins straight,
# same as gem5_cva6's own accelerator/Makefile-verilated .so does.
#
# rtl3's sources are NOT vendored into this repo -- they, and the ext/taxi +
# CVFPU sources they depend on, live in the sibling gem5_cva6 checkout this
# was ported from. Point GEM5_CVA6 at it if it isn't a sibling of this repo.

GEM5_CVA6 ?= /home/julian/gem5_cva6
RTL3_DIR  := $(GEM5_CVA6)/ext/flexnngine2_adapter/rtl3
TAXI_DIR  := $(GEM5_CVA6)/ext/taxi/src
CVFPU_DIR := $(GEM5_CVA6)/ext/flexnngine2_adapter/engine/hdl/ext/cvfpu/src
ENGINE_DIR := $(GEM5_CVA6)/ext/flexnngine2_adapter/engine/hdl

SIM ?= verilator
TOPLEVEL_LANG ?= verilog

MESH_PE_ROW ?= 8
MESH_PE_COL ?= 8
TILE_K      ?= $(MESH_PE_ROW)

# Same file list as $(RTL3_DIR)/Makefile's SV_SRC (that Makefile Verilates
# rtl3 into gem5_cva6's own dlopen'd .so; this one Verilates the identical
# sources into cocotb's Verilator flow instead).
VERILOG_SOURCES := \
	$(CVFPU_DIR)/common_cells/src/cf_math_pkg.sv \
	$(ENGINE_DIR)/flexnngine_pkg.sv \
	$(CVFPU_DIR)/common_cells/src/lzc.sv \
	$(CVFPU_DIR)/fpnew_pkg.sv \
	$(CVFPU_DIR)/fpnew_classifier.sv \
	$(CVFPU_DIR)/fpnew_rounding.sv \
	$(CVFPU_DIR)/fpnew_fma.sv \
	$(ENGINE_DIR)/mac.sv \
	$(ENGINE_DIR)/pe.sv \
	$(ENGINE_DIR)/pe_array.sv \
	$(TAXI_DIR)/axi/rtl/taxi_axi_if.sv \
	$(TAXI_DIR)/axis/rtl/taxi_axis_if.sv \
	$(TAXI_DIR)/dma/rtl/taxi_dma_desc_if.sv \
	$(TAXI_DIR)/dma/rtl/taxi_axi_dma_rd.sv \
	$(TAXI_DIR)/dma/rtl/taxi_axi_dma_wr.sv \
	$(TAXI_DIR)/prim/rtl/taxi_penc.sv \
	$(TAXI_DIR)/prim/rtl/taxi_arbiter.sv \
	$(TAXI_DIR)/prim/rtl/taxi_ram_1r1w_1c.sv \
	$(TAXI_DIR)/axi/rtl/taxi_axi_register_rd.sv \
	$(TAXI_DIR)/axi/rtl/taxi_axi_crossbar_addr.sv \
	$(TAXI_DIR)/axi/rtl/taxi_axi_crossbar_rd.sv \
	$(RTL3_DIR)/generated/flexnngine2_rtl3_top_axion_reg.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_spad.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_deskew.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_load_engine.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_compute_engine.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_store_engine.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_engine.sv \
	$(RTL3_DIR)/flexnngine2_rtl3_top.sv

VERILOG_INCLUDE_DIRS := $(CVFPU_DIR)/common_cells/include

TOPLEVEL := flexnngine2_rtl3_top
MODULE   := test_flexnngine2_rtl3

EXTRA_ARGS += -GMESH_PE_ROW=$(MESH_PE_ROW) -GMESH_PE_COL=$(MESH_PE_COL) -GTILE_K=$(TILE_K)
EXTRA_ARGS += --timescale 1ns/1ps
EXTRA_ARGS += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNOPTFLAT \
	-Wno-WIDTHEXPAND -Wno-PINMISSING -Wno-ASCRANGE

include $(shell cocotb-config --makefiles)/Makefile.sim
