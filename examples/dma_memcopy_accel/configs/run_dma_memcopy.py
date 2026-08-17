"""
run_dma_memcopy.py -- AXION worked example: DmaMemcopyAccel on a RISC-V
system.

Builds a minimal System (RiscvTimingSimpleCPU + SystemXBar + a
DmaMemcopyAccel with NUM_CH concurrent copy channels) in SE
(syscall-emulation) mode. This is the integration wiring only --
exercising it end-to-end needs a compiled RISC-V binary driving the
accelerator's control/status registers (see DMA_BASE below) via
--binary; this sandbox has no RISC-V cross-toolchain to produce one. See
examples/dma_memcopy_accel/tb_dma_memcopy.cc for a standalone Verilator
testbench that exercises the same DUT's control port *and* its DMA
master port -- including out-of-order completion across concurrent
channels -- directly, without needing gem5 or a compiled workload.

Usage:
    <gem5.opt> run_dma_memcopy.py --binary <path/to/riscv/elf>
"""

import argparse

import m5
from m5.objects import (
    AddrRange,
    DmaMemcopyAccel,
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

DMA_BASE = 0x10010000

system = m5.objects.System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
# Must stay below DMA_BASE (0x10010000), same reasoning as
# run_fifo_pio.py: SystemXBar routes by address range, and a DRAM range
# overlapping the accelerator's PIO window fails at m5.instantiate().
system.mem_ranges = [AddrRange("256MB")]

system.cpu = RiscvTimingSimpleCPU()
system.membus = SystemXBar()

system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

system.dma_accel = DmaMemcopyAccel(pio_addr=DMA_BASE)
system.dma_accel.pio = system.membus.mem_side_ports
system.dma_accel.dma = system.membus.cpu_side_ports

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
# raw physical MMIO addresses like DMA_BASE are NOT reachable by just
# writing to them (GenericPageTableFault: Process::fixupFault() only knows
# how to grow the stack/heap, not map arbitrary device ranges). Identity-map
# the accelerator's control/status window into the process's own page table
# so its loads/stores against DMA_BASE actually reach system.dma_accel
# instead of faulting. Must happen after m5.instantiate() -- Process.map()
# is a @cxxMethod, only callable once the underlying C++ object exists.
system.cpu.workload[0].map(DMA_BASE, DMA_BASE, 0x1000)

print("Beginning simulation, DMA memcopy accelerator at 0x%x" % DMA_BASE)
exit_event = m5.simulate()
print(
    "Exiting @ tick %i because %s"
    % (m5.curTick(), exit_event.getCause())
)
