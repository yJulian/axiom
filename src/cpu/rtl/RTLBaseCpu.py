# RTLBaseCpu: a BaseCPU bridged to a Verilator-simulated RTL core over
# two full AXI4 master ports (instruction, data). Abstract, and
# deliberately ISA-agnostic -- a concrete leaf class wraps a specific
# core and is responsible for its own ArchISA/ArchDecoder/ArchInterrupts/
# ArchMMU wiring in its __init__, exactly as any other BaseCPU subclass
# (see e.g. gem5's own RiscvTimingSimpleCPU for the pattern).

from m5.objects.BaseCPU import BaseCPU
from m5.params import *
from m5.proxy import *


class RTLBaseCpu(BaseCPU):
    type = "RTLBaseCpu"
    abstract = True
    cxx_header = "cpu/rtl/rtl_base_cpu.hh"
    cxx_class = "gem5::RTLBaseCpu"

    # icache_port/dcache_port are inherited from BaseCPU -- do not
    # redeclare them here. Shadowing an inherited Param creates a
    # disconnected field in the generated C++ params struct that the
    # parent's C++ constructor never sees (confirmed footgun, see
    # gem5_cva6's Cva6RtlCPU.py comment on the same issue).

    reset_cycles = Param.Unsigned(10, "Cycles to hold rst_n low at startup")
