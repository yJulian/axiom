# RTLDmaDevicePlugin: concrete RTLDmaDevice for the opt-in "plugin" path --
# dlopen()s a .so built by plugin/rtl_plugin.mk (any DUT wired through
# hw/axi4/axi4_pins.sv, exposing both an AXI4 slave control/status port and
# an AXI4 master DMA port) instead of requiring a hand-written C++ leaf
# class per RTL model.

from m5.objects.RTLDmaDevice import RTLDmaDevice
from m5.params import *
from m5.proxy import *


class RTLDmaDevicePlugin(RTLDmaDevice):
    type = "RTLDmaDevicePlugin"
    cxx_header = "dev/rtl/rtl_dma_device_plugin.hh"
    cxx_class = "gem5::RTLDmaDevicePlugin"

    rtl_library = Param.String(
        "Path to the .so built by plugin/rtl_plugin.mk, implementing the "
        "AXION RTL plugin ABI (src/axi/axi4_plugin_abi.h). Must report "
        "both an AXI4 slave and an AXI4 master port."
    )
