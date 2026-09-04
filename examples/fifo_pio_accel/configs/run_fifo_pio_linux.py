"""
run_fifo_pio_linux.py -- AXION FS-mode Linux boot worked example.

Boots RISC-V Linux (Ubuntu 24.04) under gem5's `RiscvBoard`
(gem5.components), with `FifoPioAccel` wired in as an extra off-chip PIO
device -- proving the accelerator is reachable from genuine Linux userspace
(via /dev/mem + mmap()) rather than only from gem5 SE mode's Process.map()
shortcut (see run_fifo_pio.py).

No RISC-V KVM CPU model exists in gem5 (only ARM/X86 have one), and KVM
can't cross-virtualize a RISC-V guest on an x86 host anyway -- so instead
of a real-hardware-speed boot, this uses gem5's `SimpleSwitchableProcessor`
to fast-forward the boring part of boot (kernel + systemd) under `ATOMIC`,
then switches to a real `TIMING` core exactly at the point the disk image's
own `after_boot.sh` starts (hypercall 2, see SwitchToTimingExitHandler
below) -- so the actual FIFO/PIO accelerator interaction (run via the
`readfile` mechanism, see --readfile-contents) is the one part that
executes under detailed timing. The switch is CPU-model-only; FifoPioAccel
is a plain bus-attached PioDevice with no notion of which core is driving
it, and it is never touched before the switch happens.

This reuses gem5's packaged "riscv-ubuntu-24.04-boot" FS workload resource
(kernel + OpenSBI bootloader + Ubuntu disk image, see
ext/gem5/configs/example/gem5_library/riscv-ubuntu-run.py for the upstream
worked example this is modeled on), but swaps in a caller-supplied disk
image -- normally a modified copy of the original with a compiled control
program injected onto it (see riscv_test/fifo_pio_linux_ctl.c and the
debugfs-based injection steps used to produce it).

Usage:
    <gem5.opt> run_fifo_pio_linux.py --disk-image <path/to/modified/disk.img> \
        [--readfile-contents <path/to/script.sh>]

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

from m5.objects import FifoPioAccel


class SwitchToTimingExitHandler(AfterBootExitHandler):
    """Fires on hypercall 2 ("started after_boot.sh"), i.e. right before the
    guest's after_boot.sh runs our readfile-supplied fifo_pio_linux_ctl.
    Switches the (previously ATOMIC, fast-booting) core to TIMING here so
    only the actual accelerator interaction is cycle-timed. Inherits
    hypercall_num=2 from AfterBootExitHandler automatically (see
    ExitHandlerMeta in gem5.simulate.exit_handler)."""

    @overrides(AfterBootExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        print("Switching ATOMIC -> TIMING core ahead of the FIFO accel test")
        simulator.switch_processor()


FIFO_BASE = 0x10010000


class FifoRiscvBoard(RiscvBoard):
    """RiscvBoard + AXION's FifoPioAccel wired in as an extra off-chip PIO
    device. `_setup_board()` runs at `set_kernel_disk_workload()` time (via
    `_set_fullsystem(True)`), before `_setup_io_devices()`/`_setup_pma()`
    consume `_off_chip_devices` -- appending here is enough to get the
    accelerator both bridged onto the I/O bus and registered as an
    uncacheable PMA range, with no other board plumbing to touch."""

    def _setup_board(self) -> None:
        super()._setup_board()
        if self.is_fullsystem():
            self.fifo = FifoPioAccel(pio_addr=FIFO_BASE)
            self._off_chip_devices.append(self.fifo)


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

board = FifoRiscvBoard(
    clk_freq="1GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

# Packaged FS workload: kernel + OpenSBI bootloader + Ubuntu disk image, all
# resolved together. We only override `disk_image` (our modified copy) --
# `get_parameters()` returns the live parameter dict, so mutating it here
# is reflected in the `set_kernel_disk_workload(**params)` call that
# `board.set_workload()` makes below.
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

print(f"FIFO/PIO accelerator wired in at 0x{FIFO_BASE:x} (see /dev/mem in guest)")
simulator = Simulator(board=board)
simulator.run()
