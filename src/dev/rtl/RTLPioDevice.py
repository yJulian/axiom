# RTLPioDevice: a PioDevice bridged to a Verilator-simulated RTL block
# over a full AXI4 slave port. Abstract -- instantiate a leaf SimObject
# that implements the axion::Axi4SlavePins virtuals against a specific
# Verilated top module (see examples/fifo_pio_accel/FifoPioAccel.py).

from m5.objects.Device import PioDevice
from m5.params import *
from m5.proxy import *


class RTLPioDevice(PioDevice):
    type = "RTLPioDevice"
    abstract = True
    cxx_header = "dev/rtl/rtl_pio_device.hh"
    cxx_class = "gem5::RTLPioDevice"

    pio_addr = Param.Addr("Base address of the AXI4 slave register space")
    pio_size = Param.Addr(0x1000, "Size of the register space")
    pio_latency = Param.Latency(
        "0ns",
        "Extra latency added to MMIO responses on top of the RTL "
        "AXI4 handshake latency (models device-side bus overhead)",
    )
    reset_cycles = Param.Unsigned(10, "Cycles to hold rst_n low at startup")
