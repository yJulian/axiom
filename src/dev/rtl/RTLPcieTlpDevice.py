# RTLPcieTlpDevice: a PCIe endpoint bridged to a Verilator-simulated RTL
# block at the *transaction-layer* level -- the RTL receives genuine PCIe
# TLPs on four AXI4-Stream channels (completer request/completion,
# requester request/completion) and parses and builds the headers itself,
# the way user logic behind a PCIe hard IP block does.
#
# The AXI4-bridge alternative is RTLPciDevice, where gem5 keeps PCIe
# entirely to itself and the RTL sees an ordinary AXI4 slave + master pair.
# Prefer that one unless the PCIe protocol itself is the thing being
# modelled; this path exists to make the protocol visible to the RTL, not
# because it is the more convenient way to attach an accelerator.
#
# Abstract -- instantiate a leaf SimObject implementing
# axion::PcieTlpCompleterPins, axion::PcieTlpRequesterPins and rtlGetIrq()
# against a concrete Verilated top module. A leaf must also set the usual
# PciEndpoint identity params and at least one BAR.

from m5.objects.PciDevice import PciEndpoint
from m5.params import *
from m5.proxy import *


class RTLPcieTlpDevice(PciEndpoint):
    type = "RTLPcieTlpDevice"
    abstract = True
    cxx_header = "dev/rtl/rtl_pcie_tlp_device.hh"
    cxx_class = "gem5::RTLPcieTlpDevice"

    # See RTLPciDevice.py on why this is not called pio_latency.
    rtl_pio_latency = Param.Latency(
        "0ns",
        "Extra latency added to BAR responses on top of the RTL's own TLP "
        "round-trip (models device-side bus overhead)",
    )
    reset_cycles = Param.Unsigned(10, "Cycles to hold rst_n low at startup")
    idle_gate_cycles = Param.Unsigned(
        16,
        "Consecutive cycles the RTL must report isIdle() with no pending "
        "BAR/DMA work and no asserted interrupt before the clock is gated; "
        "0 disables gating.",
    )

    # Placed in the requester ID of every request TLP gem5 sends, and
    # checked against the completer ID the RTL puts in its completions.
    # Defaults to the conventional bus 0 / device 1 / function 0 encoding.
    requester_id = Param.UInt16(
        0x0100, "Requester ID gem5 puts in the TLPs it sends to the device"
    )
    completer_id = Param.UInt16(
        0x0008, "Completer ID gem5 puts in the completions it returns"
    )
