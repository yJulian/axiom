# PcieTemplateAccel: the concrete PCIe template endpoint for AXION's PCIe
# worked example -- wraps pcie_template_top.sv (a small register file +
# scratchpad RAM + bus-master DMA engine + interrupt source, all behind
# plain AXI4) via Verilator.
#
# Everything PCIe about this device is declared here, in gem5's own
# PciEndpoint params, and none of it reaches the RTL: the DUT has no
# config space, no BARs and no notion of a link -- exactly the division of
# labor between a PCIe hard IP block and the user logic behind it on a
# real FPGA. See src/dev/rtl/rtl_pci_device.hh.
#
# Unlike FifoPioAccel there is deliberately no generateDeviceTree(): a PCIe
# endpoint is discovered by walking ECAM config space, so it needs no
# device-tree node of its own. (The host bridge does get one, and
# RiscvBoard.generate_device_tree() already emits it.)

from m5.objects.PciDevice import PciMemBar
from m5.objects.RTLPciDevice import RTLPciDevice
from m5.params import *
from m5.proxy import *


class PcieTemplateAccel(RTLPciDevice):
    type = "PcieTemplateAccel"
    cxx_header = "pcie_template_device.hh"
    cxx_class = "gem5::PcieTemplateAccel"

    # Not an assigned PCI-SIG vendor ID -- 0x1DE5/0x0001 is a made-up pair
    # for this test device, and must stay out of any range a real driver
    # would bind to.
    VendorID = 0x1DE5
    DeviceID = 0x0001
    Revision = 0x01
    ClassCode = 0xFF  # Unassigned class ("misc device")
    SubClassCode = 0x00
    ProgIF = 0x00

    # 64 KiB: 0x0000-0x0FFF registers, 0x1000-0x1FFF scratchpad RAM, the
    # rest reserved. Must stay a power of two and match the RAM_WORDS the
    # DUT is elaborated with (see pcie_template_accel.sv).
    BAR0 = PciMemBar(size="64KiB")

    # INTx line A. On RISC-V the PLIC source this ends up on is derived
    # from the device's slot, not from InterruptLine:
    # GenericRiscvPciHost::mapPciInterrupt() returns
    # int_base + (pci_dev % int_count), i.e. 0x10 + pci_dev under HiFive's
    # defaults. InterruptLine is still set so config-space reads of it are
    # meaningful to a guest.
    InterruptLine = 0x11
    InterruptPin = 0x01
