# PcieTlpTemplateAccel: the concrete TLP-level PCIe endpoint for AXION's
# TLP worked example -- wraps pcie_tlp_template_top.sv via Verilator.
#
# Functionally identical to PcieTemplateAccel (same registers, same
# scratchpad RAM, same DMA engine, same interrupt), and deliberately so:
# the pair exists to compare the two abstraction levels on the same device,
# not to compare two different devices. What differs is that this one's RTL
# parses and builds PCIe transaction-layer packets itself, where the other
# one sees plain AXI4.
#
# As with the AXI4 variant there is no generateDeviceTree(): a PCIe
# endpoint is discovered by walking ECAM config space.

from m5.objects.PciDevice import PciMemBar
from m5.objects.RTLPcieTlpDevice import RTLPcieTlpDevice
from m5.params import *
from m5.proxy import *


class PcieTlpTemplateAccel(RTLPcieTlpDevice):
    type = "PcieTlpTemplateAccel"
    cxx_header = "pcie_tlp_template_device.hh"
    cxx_class = "gem5::PcieTlpTemplateAccel"

    # Not assigned PCI-SIG IDs; DeviceID 0x0002 distinguishes this from the
    # AXI4-bridge variant's 0x0001 so a guest can tell which it is talking
    # to when both are present.
    VendorID = 0x1DE5
    DeviceID = 0x0002
    Revision = 0x01
    ClassCode = 0xFF  # Unassigned class ("misc device")
    SubClassCode = 0x00
    ProgIF = 0x00

    BAR0 = PciMemBar(size="64KiB")

    # INTx line A. The PLIC source this actually lands on is slot-derived
    # (int_base + pci_dev = 0x11 at slot 1), not taken from InterruptLine --
    # see RTLPciDevice's rtlGetIrq() comment. InterruptLine is set to match
    # so a guest reading config space is not misled.
    InterruptLine = 0x11
    InterruptPin = 0x01

    # The DUT hardcodes COMPLETER_ID = 16'h0008 (bus 0, dev 1, func 0) into
    # the completions it builds, so a guest reading a completion header sees
    # that. Keep completer_id in step with wherever the device is actually
    # slotted if that ever matters to a driver.
    completer_id = 0x0008
