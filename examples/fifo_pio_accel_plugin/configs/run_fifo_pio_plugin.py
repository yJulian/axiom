"""
run_fifo_pio_plugin.py -- plugin-path analog of
examples/fifo_pio_accel/configs/run_fifo_pio.py: identical System wiring,
but the hand-written FifoPioAccel C++ leaf class is replaced by
RTLPioDevicePlugin pointed at the .so built by `make -C
examples/fifo_pio_accel_plugin plugin-so` -- no new C++/Python needed for
this DUT at all. Same caveat as the direct-link config: this is correct
integration wiring, but exercising it end-to-end needs a compiled RISC-V
binary performing loads/stores against the accelerator's MMIO range (see
FIFO_BASE below) via --binary; this sandbox has no RISC-V cross-toolchain
to produce one. See examples/fifo_pio_accel_plugin/tb_fifo_pio_plugin.cc
for genuine, fully-automated verification of the plugin ABI + AXI4
protocol logic, independent of gem5 and the RISC-V toolchain (`make
tb-plugin`).

Usage:
    <gem5.opt> run_fifo_pio_plugin.py --binary <path/to/riscv/elf>
"""

import argparse

import m5
from m5.objects import (
    AddrRange,
    Process,
    RiscvTimingSimpleCPU,
    Root,
    RTLPioDevicePlugin,
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

system.fifo = RTLPioDevicePlugin(
    pio_addr=FIFO_BASE,
    rtl_library="examples/fifo_pio_accel_plugin/libfifo_pio_top_plugin.so",
)
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

print("Beginning simulation, FIFO/PIO accelerator (plugin) at 0x%x" % FIFO_BASE)
exit_event = m5.simulate()
print(
    "Exiting @ tick %i because %s"
    % (m5.curTick(), exit_event.getCause())
)
