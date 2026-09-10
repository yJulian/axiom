# RTLPciDevice: a PCIe endpoint bridged to a Verilator-simulated RTL block.
# gem5 models config space, BARs and INTx (inherited from PciEndpoint); the
# RTL sees the BAR window as an AXI4 slave port, does bus-master DMA over an
# AXI4 master port, and raises an interrupt pin -- the same split a PCIe hard
# IP block and its user logic have on real hardware. Abstract -- instantiate
# a leaf SimObject implementing axion::Axi4SlavePins, axion::Axi4MasterPins
# and rtlGetIrq() against a concrete Verilated top module.
#
# A leaf must additionally set the usual PciEndpoint identity params
# (VendorID/DeviceID/ClassCode) and at least one BAR; the BAR is what
# defines the device's address range, so there is no pio_addr/pio_size here
# the way there is on RTLPioDevice/RTLDmaDevice -- the guest's enumeration
# decides where the window lands.

from m5.objects.PciDevice import PciEndpoint
from m5.params import *
from m5.proxy import *


class RTLPciDevice(PciEndpoint):
    type = "RTLPciDevice"
    abstract = True
    cxx_header = "dev/rtl/rtl_pci_device.hh"
    cxx_class = "gem5::RTLPciDevice"

    # Named rtl_pio_latency rather than pio_latency (the name
    # RTLPioDevice/RTLDmaDevice use) because PciDevice already defines a
    # pio_latency param of its own, with different meaning: that one is the
    # flat latency an ordinary PCI device charges for a BAR access, whereas
    # here the bulk of the latency comes from the RTL's own AXI4 handshake
    # and this is only the extra device-side bus overhead on top of it.
    rtl_pio_latency = Param.Latency(
        "0ns",
        "Extra latency added to BAR responses on top of the RTL AXI4 "
        "handshake latency (models device-side bus overhead)",
    )
    reset_cycles = Param.Unsigned(10, "Cycles to hold rst_n low at startup")
    idle_gate_cycles = Param.Unsigned(
        16,
        "Consecutive cycles the RTL must report isIdle() with no pending "
        "BAR/DMA work and no asserted interrupt before the clock is gated "
        "(stops ticking until the next BAR access or DMA completion); 0 "
        "disables gating. No effect on a leaf that doesn't override "
        "isIdle() (default: never idle).",
    )
