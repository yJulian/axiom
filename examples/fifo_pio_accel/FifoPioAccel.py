# FifoPioAccel: the concrete FIFO/PIO accelerator SimObject for AXION's
# worked example -- wraps fifo_pio_top.sv (an N-deep FIFO queue behind a
# full AXI4 slave register port) via Verilator.

from m5.objects.RTLPioDevice import RTLPioDevice
from m5.params import *
from m5.proxy import *


class FifoPioAccel(RTLPioDevice):
    type = "FifoPioAccel"
    cxx_header = "fifo_pio_device.hh"
    cxx_class = "gem5::FifoPioAccel"

    pio_size = 0x1000
