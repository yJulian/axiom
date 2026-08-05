# rtl_plugin.mk -- reusable Makefile template for AXION's opt-in "plugin"
# path. `include` this from a directory containing your own DUT + TOP
# wrapper .sv files (see examples/fifo_pio_accel_plugin/Makefile for a
# worked example reusing examples/fifo_pio_accel/'s DUT) to turn them into
# a .so implementing src/axi/axi4_plugin_abi.h's stable C ABI -- no new
# C++ leaf class or .py SimObject file required. Point RTLPioDevicePlugin's
# or RTLDmaDevicePlugin's `rtl_library` param at the resulting .so.
#
# Caller-set variables (before the `include`):
#   TOP_MODULE        (required) -- same meaning as `verilator --top-module`.
#   PLUGIN_SOURCES     (required) -- your DUT + TOP wrapper .sv files only;
#                       this template prepends hw/axi4's three shared files.
#   PLUGIN_HAS_SLAVE   (default 1) -- TOP module exposes an s_axi_* slave port.
#   PLUGIN_HAS_MASTER  (default 0) -- TOP module exposes an m_axi_* master port.
#   OUT_DIR            (default obj_dir_plugin)
#   SO_NAME            (default lib$(TOP_MODULE)_plugin.so)
#
# Targets: plugin-so, plugin-clean.
#
# Build recipe mirrors the proven Verilator-to-.so pattern from
# ~/gem5_cva6/accelerator/Makefile (see AXION's CLAUDE.md, "Prior art on
# this machine"): `verilator --cc` (not `--build`, since a *shared* object
# needs its own link step, not Verilator's own executable-producing
# --build link), then `make -f Vxxx.mk` to produce the static archive of
# Verilated model objects, then a manual `g++ -shared -fPIC` link of the
# generic shim + Verilator runtime + that archive into the final .so.

ifndef TOP_MODULE
$(error TOP_MODULE is required, e.g. TOP_MODULE=fifo_pio_top)
endif
ifndef PLUGIN_SOURCES
$(error PLUGIN_SOURCES is required -- your DUT + TOP wrapper .sv files)
endif

PLUGIN_HAS_SLAVE  ?= 1
PLUGIN_HAS_MASTER ?= 0
OUT_DIR           ?= obj_dir_plugin
SO_NAME           ?= lib$(TOP_MODULE)_plugin.so

VERILATOR      ?= verilator
VERILATOR_ROOT ?= /usr/local/share/verilator
CXX            ?= g++

# This file's own directory, so `include`-ing it works regardless of how
# deeply nested the caller's directory is (unlike examples/fifo_pio_accel/
# Makefile's hardcoded ../../hw/axi4).
PLUGIN_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
REPO_ROOT     := $(PLUGIN_MK_DIR)..
AXI4_DIR      := $(REPO_ROOT)/hw/axi4
SHIM_SRC      := $(PLUGIN_MK_DIR)rtl_plugin_shim.cc

ALL_SOURCES := $(AXI4_DIR)/axi4_pkg.sv $(AXI4_DIR)/axi4_if.sv \
               $(AXI4_DIR)/axi4_pins.sv $(PLUGIN_SOURCES)

VL_RUNTIME := $(VERILATOR_ROOT)/include/verilated.cpp \
              $(VERILATOR_ROOT)/include/verilated_threads.cpp

.PHONY: plugin-so plugin-clean

plugin-so: $(SO_NAME)

$(SO_NAME): $(ALL_SOURCES) $(SHIM_SRC)
	$(VERILATOR) --cc -CFLAGS "-fPIC -std=c++17" --top-module $(TOP_MODULE) \
		-Wno-fatal -Wno-DECLFILENAME -Wno-UNUSEDSIGNAL \
		-Mdir $(OUT_DIR) $(ALL_SOURCES)
	$(MAKE) -C $(OUT_DIR) -f V$(TOP_MODULE).mk
	$(CXX) -shared -fPIC -std=c++17 -o $(SO_NAME) \
		-DAXION_TOP_CLASS=V$(TOP_MODULE) \
		-DAXION_PLUGIN_HAS_SLAVE=$(PLUGIN_HAS_SLAVE) \
		-DAXION_PLUGIN_HAS_MASTER=$(PLUGIN_HAS_MASTER) \
		-include V$(TOP_MODULE).h \
		-I$(VERILATOR_ROOT)/include -I$(VERILATOR_ROOT)/include/vltstd \
		-I$(OUT_DIR) -I$(REPO_ROOT)/src \
		$(SHIM_SRC) $(VL_RUNTIME) $(OUT_DIR)/V$(TOP_MODULE)__ALL.a

plugin-clean:
	rm -rf $(OUT_DIR) $(SO_NAME)
