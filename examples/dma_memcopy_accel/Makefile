# Verilates dma_memcopy_top.sv (+ DUT + hw/axi4's shared building blocks)
# into a standalone static library + generated headers under obj_dir/,
# linked directly into gem5 (no dlopen) via this example's SConscript.
# Run this before `make gem5` at the repo root.
#
# Standalone (no-gem5) verification of this DUT lives in cocotb/ (cocotb +
# cocotbext-axi -- see cocotb/Makefile and the repo root's `make tb-dma`).

VERILATOR  ?= verilator
TOP_MODULE  = dma_memcopy_top
AXI4_DIR    = ../../hw/axi4
OBJ_DIR     = obj_dir

SOURCES = \
	$(AXI4_DIR)/axi4_pkg.sv \
	$(AXI4_DIR)/axi4_if.sv \
	$(AXI4_DIR)/axi4_pins.sv \
	dma_memcopy.sv \
	dma_memcopy_top.sv

VERILATOR_FLAGS = \
	--cc --build -j 0 \
	--top-module $(TOP_MODULE) \
	--trace-fst \
	-Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL -Wno-UNOPTFLAT \
	-CFLAGS "-fPIC -std=c++17" \
	-Mdir $(OBJ_DIR)

LIB = $(OBJ_DIR)/V$(TOP_MODULE)__ALL.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(SOURCES)
	$(VERILATOR) $(VERILATOR_FLAGS) $(SOURCES)

clean:
	rm -rf $(OBJ_DIR)
