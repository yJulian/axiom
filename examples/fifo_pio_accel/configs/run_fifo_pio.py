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
system.mem_ranges = [AddrRange("512MB")]

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

print("Beginning simulation, FIFO/PIO accelerator at 0x%x" % FIFO_BASE)
exit_event = m5.simulate()
print(
    "Exiting @ tick %i because %s"
    % (m5.curTick(), exit_event.getCause())
)
