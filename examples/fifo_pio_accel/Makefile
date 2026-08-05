# Verilates fifo_pio_top.sv (+ DUT + hw/axi4's shared building blocks)
# into a standalone static library + generated headers under obj_dir/,
# linked directly into gem5 (no dlopen) via this example's SConscript.
# Run this before `make gem5` at the repo root.

VERILATOR  ?= verilator
TOP_MODULE  = fifo_pio_top
AXI4_DIR    = ../../hw/axi4
OBJ_DIR     = obj_dir

SOURCES = \
	$(AXI4_DIR)/axi4_pkg.sv \
	$(AXI4_DIR)/axi4_if.sv \
	$(AXI4_DIR)/axi4_pins.sv \
	fifo_pio_accel.sv \
	fifo_pio_top.sv

VERILATOR_FLAGS = \
	--cc --build -j 0 \
	--top-module $(TOP_MODULE) \
	--trace-fst \
	-Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL \
	-CFLAGS "-fPIC -std=c++17" \
	-Mdir $(OBJ_DIR)

LIB = $(OBJ_DIR)/V$(TOP_MODULE)__ALL.a

TB_OBJ_DIR = obj_dir_tb
TB_EXE     = $(TB_OBJ_DIR)/tb_fifo_pio

.PHONY: all tb run-tb clean

all: $(LIB)

$(LIB): $(SOURCES)
	$(VERILATOR) $(VERILATOR_FLAGS) $(SOURCES)

# Standalone testbench (tb_fifo_pio.cc): drives the DUT's AXI4 pins
# directly, independent of gem5 -- see that file's header comment.
tb: $(TB_EXE)

$(TB_EXE): $(SOURCES) tb_fifo_pio.cc
	$(VERILATOR) --cc --build -j 0 --top-module $(TOP_MODULE) \
		--exe -o tb_fifo_pio \
		-Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL \
		-CFLAGS "-fPIC -std=c++17" \
		-Mdir $(TB_OBJ_DIR) \
		$(SOURCES) tb_fifo_pio.cc

run-tb: tb
	$(TB_EXE)

clean:
	rm -rf $(OBJ_DIR) $(TB_OBJ_DIR)
