# DmaMemcopyAccel: the concrete DMA memcopy accelerator SimObject for
# AXION's second worked example -- wraps dma_memcopy_top.sv (NUM_CH
# independent copy channels behind a control/status AXI4 slave port, each
# driving its own AXI4 DMA read+write burst over a shared master port,
# tagged by its own channel index as AXI ID) via Verilator.

from m5.objects.RTLDmaDevice import RTLDmaDevice
from m5.params import *
from m5.proxy import *


class DmaMemcopyAccel(RTLDmaDevice):
    type = "DmaMemcopyAccel"
    cxx_header = "dma_memcopy_device.hh"
    cxx_class = "gem5::DmaMemcopyAccel"

    pio_size = 0x1000

    def generateDeviceTree(self, state):
        node = self.generateBasicPioDeviceNode(
            state, "dma_memcopy_accel", self.pio_addr, self.pio_size
        )
        node.appendCompatible(["axion,dma-memcopy-accel"])
        yield node
