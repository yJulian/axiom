# Flexnngine2Rtl3Accel: the concrete SimObject for the rtl3 (scratchpad-fed)
# FleXNNgine2 GEMM accelerator port -- wraps flexnngine2_rtl3_axion_top.sv
# (control/status AXI4-Lite port + one AXI4 DMA master port carrying two
# tile-read masters and one write master under three distinct AXI IDs) via
# Verilator. See ../README.md for what "port" means here (the RTL comes
# from a sibling gem5_cva6 checkout) and register-map details.

from m5.objects.RTLDmaDevice import RTLDmaDevice
from m5.params import *
from m5.proxy import *


class Flexnngine2Rtl3Accel(RTLDmaDevice):
    type = "Flexnngine2Rtl3Accel"
    cxx_header = "flexnngine2_rtl3_device.hh"
    cxx_class = "gem5::Flexnngine2Rtl3Accel"

    pio_size = 0x1000

    def generateDeviceTree(self, state):
        node = self.generateBasicPioDeviceNode(
            state, "flexnngine2_rtl3_accel", self.pio_addr, self.pio_size
        )
        node.appendCompatible(["axion,flexnngine2-rtl3-accel"])
        yield node
