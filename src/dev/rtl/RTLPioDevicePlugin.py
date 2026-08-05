# RTLPioDevicePlugin: concrete RTLPioDevice for the opt-in "plugin" path --
# dlopen()s a .so built by plugin/rtl_plugin.mk (any DUT wired through
# hw/axi4/axi4_pins.sv) instead of requiring a hand-written C++ leaf class
# per RTL model. See examples/fifo_pio_accel_plugin/ for a worked example.

from m5.objects.RTLPioDevice import RTLPioDevice
from m5.params import *
from m5.proxy import *


class RTLPioDevicePlugin(RTLPioDevice):
    type = "RTLPioDevicePlugin"
    cxx_header = "dev/rtl/rtl_pio_device_plugin.hh"
    cxx_class = "gem5::RTLPioDevicePlugin"

    rtl_library = Param.String(
        "Path to the .so built by plugin/rtl_plugin.mk, implementing the "
        "AXION RTL plugin ABI (src/axi/axi4_plugin_abi.h)"
    )
