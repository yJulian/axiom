# Verilates flexnngine2_rtl3_axion_top.sv (+ the rtl3 DUT and its ext/taxi +
# CVFPU dependencies, pulled from a sibling gem5_cva6 checkout) into a
# standalone static library + generated headers under obj_dir/, linked
# directly into gem5 (no dlopen) via this example's SConscript. Run this
# before `make gem5` at the repo root.
#
# rtl3's own sources are NOT vendored into this repo -- see ../README.md.
# Point GEM5_CVA6 at a gem5_cva6 checkout if it isn't at the default path.
#
# Standalone (no-gem5) verification of the DUT itself (bypassing this
# wrapper) lives in cocotb/ -- see cocotb/Makefile and the repo root's
# `make tb-rtl3`.

GEM5_CVA6 ?= /home/julian/gem5_cva6
RTL3_DIR   := $(GEM5_CVA6)/ext/flexnngine2_adapter/rtl3
TAXI_DIR   := $(GEM5_CVA6)/ext/taxi/src
CVFPU_DIR  := $(GEM5_CVA6)/ext/flexnngine2_adapter/engine/hdl/ext/cvfpu/src
ENGINE_DIR := $(GEM5_CVA6)/ext/flexnngine2_adapter/engine/hdl

VERILATOR  ?= verilator
TOP_MODULE  = flexnngine2_rtl3_axion_top
OBJ_DIR     = obj_dir

MESH_PE_ROW ?= 8
MESH_PE_COL ?= 8
TILE_K      ?= $(MESH_PE_ROW)

# Same file list as cocotb/Makefile's VERILOG_SOURCES, plus this example's
# own wrapper top on top.
SOURCES = \
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
	$(RTL3_DIR)/flexnngine2_rtl3_top.sv \
	flexnngine2_rtl3_axion_top.sv

VERILATOR_FLAGS = \
	--cc --build -j 0 \
	--top-module $(TOP_MODULE) \
	--trace-fst \
	-Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNOPTFLAT \
	-Wno-WIDTHEXPAND -Wno-PINMISSING -Wno-ASCRANGE \
	--timescale 1ns/1ps \
	-GMESH_PE_ROW=$(MESH_PE_ROW) -GMESH_PE_COL=$(MESH_PE_COL) -GTILE_K=$(TILE_K) \
	-I$(CVFPU_DIR)/common_cells/include \
	-CFLAGS "-fPIC -std=c++17" \
	-Mdir $(OBJ_DIR)

LIB = $(OBJ_DIR)/V$(TOP_MODULE)__ALL.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(SOURCES)
	$(VERILATOR) $(VERILATOR_FLAGS) $(SOURCES)

clean:
	rm -rf $(OBJ_DIR)
