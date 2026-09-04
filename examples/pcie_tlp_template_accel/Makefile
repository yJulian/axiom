# Verilates pcie_tlp_template_top.sv (+ DUT + hw/pcie's shared building
# blocks) into a standalone static library + generated headers under
# obj_dir/, linked directly into gem5 (no dlopen) via this example's
# SConscript. Run this before `make gem5` at the repo root.
#
# Note this example pulls from hw/pcie/, not hw/axi4/: the RTL here speaks
# PCIe transaction-layer packets rather than AXI4 (see
# ../pcie_template_accel/ for the AXI4-bridge variant of the same device).
#
# Standalone (no-gem5) verification lives in cocotb/ -- see the repo
# root's `make tb-pcie-tlp`.

VERILATOR  ?= verilator
TOP_MODULE  = pcie_tlp_template_top
PCIE_DIR    = ../../hw/pcie
OBJ_DIR     = obj_dir

SOURCES = \
	$(PCIE_DIR)/pcie_tlp_pkg.sv \
	$(PCIE_DIR)/pcie_tlp_if.sv \
	$(PCIE_DIR)/pcie_tlp_pins.sv \
	pcie_tlp_template_accel.sv \
	pcie_tlp_template_top.sv

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
