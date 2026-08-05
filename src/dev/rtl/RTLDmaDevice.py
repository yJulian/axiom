# RTLDmaDevice: a DmaDevice bridged to a Verilator-simulated RTL block
# over a control/status AXI4 slave port plus an AXI4 master (DMA) port
# with real ID-ordered out-of-order completion. Abstract -- instantiate a
# leaf SimObject implementing both axion::Axi4SlavePins and
# axion::Axi4MasterPins against a concrete Verilated top module.

from m5.objects.Device import DmaDevice
from m5.params import *
from m5.proxy import *


class RTLDmaDevice(DmaDevice):
    type = "RTLDmaDevice"
    abstract = True
    cxx_header = "dev/rtl/rtl_dma_device.hh"
    cxx_class = "gem5::RTLDmaDevice"

    pio_addr = Param.Addr("Base address of the AXI4 slave register space")
    pio_size = Param.Addr(0x1000, "Size of the register space")
    pio_latency = Param.Latency(
        "0ns",
        "Extra latency added to MMIO responses on top of the RTL "
        "AXI4 handshake latency (models device-side bus overhead)",
    )
    reset_cycles = Param.Unsigned(10, "Cycles to hold rst_n low at startup")
