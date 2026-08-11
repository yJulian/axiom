"""
run_fifo_pio.py -- AXION worked example: FifoPioAccel on a RISC-V system.

Builds a minimal System (RiscvTimingSimpleCPU + SystemXBar + a
FifoPioAccel FIFO/PIO accelerator) in SE (syscall-emulation) mode. This
is the integration wiring only -- exercising it end-to-end needs a
compiled RISC-V binary performing loads/stores against the accelerator's
MMIO range (see FIFO_BASE below) via --binary; this sandbox has no
RISC-V cross-toolchain to produce one. See
examples/fifo_pio_accel/tb_fifo_pio.cc for a standalone Verilator
testbench that exercises the same DUT's AXI4 protocol directly, without
needing gem5 or a compiled workload.

Usage:
    <gem5.opt> run_fifo_pio.py --binary <path/to/riscv/elf>
"""

import argparse

import m5
from m5.objects import (
    AddrRange,
    FifoPioAccel,
    Process,
    RiscvTimingSimpleCPU,
    Root,
    SEWorkload,
    SrcClockDomain,
    SystemXBar,
    VoltageDomain,
)

parser = argparse.ArgumentParser()
parser.add_argument(
    "--binary", type=str, required=True, help="RISC-V SE-mode binary to run"
)
args = parser.parse_args()

FIFO_BASE = 0x10010000

system = m5.objects.System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
# Must stay below FIFO_BASE (0x10010000): SystemXBar routes by address
# range, and a 512MB DRAM range [0, 0x20000000) would overlap the
# accelerator's PIO window sitting inside it -- caught at m5.instantiate()
# time ("system.membus has two ports responding within range ...").
system.mem_ranges = [AddrRange("256MB")]

system.cpu = RiscvTimingSimpleCPU()
system.membus = SystemXBar()

system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

system.fifo = FifoPioAccel(pio_addr=FIFO_BASE)
system.fifo.pio = system.membus.mem_side_ports

system.mem_ctrl = m5.objects.SimpleMemory()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

system.cpu.createInterruptController()

system.system_port = system.membus.cpu_side_ports

process = Process()
process.cmd = [args.binary]
system.cpu.workload = process
system.cpu.createThreads()

system.workload = SEWorkload.init_compatible(args.binary)

root = Root(full_system=False, system=system)
m5.instantiate()

# SE mode gives the process a normal demand-paged virtual address space --
# raw physical MMIO addresses like FIFO_BASE are NOT reachable by just
# writing to them (GenericPageTableFault: Process::fixupFault() only knows
# how to grow the stack/heap, not map arbitrary device ranges). Identity-map
# the accelerator's PIO window into the process's own page table so its
# loads/stores against FIFO_BASE actually reach system.fifo instead of
# faulting. Must happen after m5.instantiate() -- Process.map() is a
# @cxxMethod, only callable once the underlying C++ object exists.
system.cpu.workload[0].map(FIFO_BASE, FIFO_BASE, 0x1000)

print("Beginning simulation, FIFO/PIO accelerator at 0x%x" % FIFO_BASE)
exit_event = m5.simulate()
print(
    "Exiting @ tick %i because %s"
    % (m5.curTick(), exit_event.getCause())
)
