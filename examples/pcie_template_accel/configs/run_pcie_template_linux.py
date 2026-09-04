"""
run_pcie_template_linux.py -- AXION's FS-mode Linux PCIe worked example.

Boots RISC-V Linux (Ubuntu 24.04) under gem5's `RiscvBoard` with
`PcieTemplateAccel` (an RTL-backed PCIe endpoint, see
src/dev/rtl/rtl_pci_device.hh) plugged into slot 1 of the board's PCI bus,
and lets the kernel enumerate it. Companion to run_pcie_template.py, which
does the same thing bare metal.

What this adds over the bare-metal run is precisely the part a hand-written
probe cannot check: nothing here writes a BAR or a COMMAND register.
Linux's own PCI subsystem walks ECAM, sizes BAR0, assigns it an address,
and publishes the result through sysfs -- so the guest program
(riscv_test/pcie_template_linux_ctl.c) just mmap()s
/sys/bus/pci/devices/0000:00:01.0/resource0 and finds the RTL behind it.
If the endpoint's config space were subtly wrong, this is where it would
show.

Note there is no device-tree node for the endpoint and none is needed --
PCIe devices are discovered by walking config space. Only the host bridge
gets a DT node, and RiscvBoard.generate_device_tree() already emits it.
(That is a real advantage over the MMIO fifo_pio_accel example, where
RiscvBoard's hand-written device tree ignores per-device
generateDeviceTree() entirely and the guest has to know a hardcoded
physical address.)

Like run_fifo_pio_linux.py this uses `SimpleSwitchableProcessor` to
fast-forward the boring part of boot under ATOMIC and switch to a real
TIMING core exactly when the disk image's own after_boot.sh starts, so
only the accelerator interaction is cycle-timed. The switch is
CPU-model-only; the endpoint is never touched before it happens.

The disk image is a modified copy of gem5's packaged
"riscv-ubuntu-24.04-boot" resource with the compiled control program
injected at /root/pcie_template_linux_ctl (same debugfs-based injection
used for the FIFO example). It is several GiB and is not downloaded by
default -- this script is the optional counterpart to
`make run-pcie-example`, which needs nothing.

Usage:
    <gem5.opt> run_pcie_template_linux.py --disk-image <path/to/disk.img> \
        [--readfile-contents ../riscv_test/after_boot_readfile.sh]

The simulated console is written to `m5out/board.platform.terminal`.
"""

import argparse

from gem5.components.boards.riscv_board import RiscvBoard
from gem5.components.cachehierarchies.classic.private_l1_private_l2_walk_cache_hierarchy import (
    PrivateL1PrivateL2WalkCacheHierarchy,
)
from gem5.components.memory import SingleChannelDDR3_1600
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_switchable_processor import (
    SimpleSwitchableProcessor,
)
from gem5.isas import ISA
from gem5.resources.resource import DiskImageResource, obtain_resource
from gem5.simulate.exit_handler import AfterBootExitHandler
from gem5.simulate.simulator import Simulator
from gem5.utils.override import overrides

from m5.objects import PcieTemplateAccel

# Slot 0 is already taken by RiscvBoard's own IGbE_e1000; 1 is the first
# free one. Must match PCI_SLOT in riscv_test/pcie_template_linux_ctl.c.
PCI_DEV = 1


class SwitchToTimingExitHandler(AfterBootExitHandler):
    """Fires on hypercall 2 ("started after_boot.sh"), i.e. right before
    the guest runs our readfile-supplied control program. Switches the
    (previously ATOMIC, fast-booting) core to TIMING here so only the
    actual endpoint interaction is cycle-timed. Inherits hypercall_num=2
    from AfterBootExitHandler automatically."""

    @overrides(AfterBootExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        print("Switching ATOMIC -> TIMING core ahead of the PCIe test")
        simulator.switch_processor()


class PcieRiscvBoard(RiscvBoard):
    """RiscvBoard + AXION's PcieTemplateAccel on the PCI bus.

    Unlike the MMIO FifoPioAccel case (run_fifo_pio_linux.py, which
    appends to `_off_chip_devices` in `_setup_board()`), a PCI endpoint
    attaches to `platform.pci_bus` rather than the I/O bus, so the hook
    is `_setup_io_devices()` -- called after `super()` has already wired
    the host, the bus and the three PCI address windows into both
    `bridge.ranges` and the PMA checker. There is nothing else to plumb:
    no address to pick, no bridge range to add, no device-tree node.
    """

    @overrides(RiscvBoard)
    def _setup_io_devices(self) -> None:
        super()._setup_io_devices()
        self.pcie_accel = PcieTemplateAccel(pci_dev=PCI_DEV, pci_func=0)
        self.pcie_accel.upstream = self.platform.pci_host
        self.pcie_accel.pio = self.platform.pci_bus.mem_side_ports
        self.pcie_accel.dma = self.platform.pci_bus.cpu_side_ports


parser = argparse.ArgumentParser()
parser.add_argument(
    "--disk-image",
    type=str,
    required=True,
    help="Local path to a (possibly modified) riscv-ubuntu disk image",
)
parser.add_argument(
    "--readfile-contents",
    type=str,
    default=None,
    help="Path to a script whose contents get auto-run in the guest after "
    "boot via the `m5 readfile` mechanism",
)
args = parser.parse_args()

cache_hierarchy = PrivateL1PrivateL2WalkCacheHierarchy(
    l1d_size="16KiB", l1i_size="16KiB", l2_size="256KiB"
)
memory = SingleChannelDDR3_1600(size="1GiB")
processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.ATOMIC,
    switch_core_type=CPUTypes.TIMING,
    num_cores=1,
    isa=ISA.RISCV,
)

board = PcieRiscvBoard(
    clk_freq="1GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

# Packaged FS workload: kernel + OpenSBI bootloader + Ubuntu disk image.
# Only `disk_image` is overridden (our modified copy); get_parameters()
# returns the live dict, so mutating it here is reflected in the
# set_kernel_disk_workload(**params) call board.set_workload() makes.
fs_workload = obtain_resource(
    "riscv-ubuntu-24.04-boot", resource_version="2.0.0"
)
params = fs_workload.get_parameters()
orig_disk = params["disk_image"]
params["disk_image"] = DiskImageResource(
    local_path=args.disk_image,
    root_partition=orig_disk.get_root_partition(),
)
if args.readfile_contents is not None:
    with open(args.readfile_contents) as f:
        params["readfile_contents"] = f.read()

board.set_workload(fs_workload)

print(
    f"PCIe template endpoint wired in at 00:{PCI_DEV:02x}.0 -- Linux will "
    "enumerate it; see /sys/bus/pci/devices/ in the guest"
)
simulator = Simulator(board=board)
simulator.run()
