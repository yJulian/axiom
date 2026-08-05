# Builds fifo_pio_accel/'s DUT (reused read-only, no second DUT needed to
# prove the plugin ABI) into a .so via the generic plugin/rtl_plugin.mk
# template, plus a standalone testbench that drives that .so purely
# through src/axi/axi4_plugin_abi.h -- no Vfifo_pio_top.h, no Verilator
# headers at all -- proving the ABI round-trips with zero compile-time
# knowledge of the DUT. Run before `make gem5` isn't required (this
# example has no SConscript and is never linked into gem5.opt).

TOP_MODULE       = fifo_pio_top
PLUGIN_SOURCES   = ../fifo_pio_accel/fifo_pio_accel.sv \
                    ../fifo_pio_accel/fifo_pio_top.sv
PLUGIN_HAS_SLAVE  = 1
PLUGIN_HAS_MASTER = 0

include ../../plugin/rtl_plugin.mk

REPO_ROOT  = ../..
TB_EXE     = tb_fifo_pio_plugin

.PHONY: tb run-tb clean

tb: $(TB_EXE)

$(TB_EXE): tb_fifo_pio_plugin.cc $(SO_NAME)
	$(CXX) -std=c++17 -I$(REPO_ROOT)/src -o $(TB_EXE) tb_fifo_pio_plugin.cc -ldl

run-tb: tb
	./$(TB_EXE) ./$(SO_NAME)

clean: plugin-clean
	rm -f $(TB_EXE)
