"""
run_flexnngine2_rtl3.py -- AXION worked example: Flexnngine2Rtl3Accel on a
full-system, bare-metal RISC-V system.

Unlike run_fifo_pio.py/run_dma_memcopy.py (SE mode, needs a RISC-V
cross-toolchain this sandbox doesn't have -- see those files' docstrings),
this reuses gem5_cva6's ALREADY-COMPILED bare-metal test ELF
(scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.elf) unmodified: it writes
to a well-known "tohost" address instead of making OS syscalls, so it
needs full-system mode (RiscvBareMetal, no kernel) and this config's own
tohost-polling loop instead of SE mode's Process/exit-syscall handling --
see gem5_cva6's README.md section 5 / deprecated/configs/garnet2/run_loop.py
for the same convention this borrows.

Usage:
    <gem5.opt> run_flexnngine2_rtl3.py [--binary <elf>] [--gem5-cva6 <path>]
"""

import argparse
import os
import sys

import m5
from m5.objects import (
    AddrRange,
    Flexnngine2Rtl3Accel,
    Root,
    RiscvBareMetal,
    RiscvTimingSimpleCPU,
    SimpleMemory,
    SrcClockDomain,
    SystemXBar,
    VoltageDomain,
)

TOHOST_ADDR = 0x80001000  # scratch/link.ld's convention, gem5_cva6-side

parser = argparse.ArgumentParser()
parser.add_argument(
    "--gem5-cva6", type=str, default="/home/julian/gem5_cva6",
    help="path to a gem5_cva6 checkout (for its default --binary)",
)
parser.add_argument(
    "--binary", type=str, default=None,
    help="RISC-V bare-metal ELF (default: gem5_cva6's own "
         "scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.elf)",
)
parser.add_argument("--max-ticks", type=int, default=200_000_000_000)
args = parser.parse_args()

binary = args.binary or os.path.join(
    args.gem5_cva6, "scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.elf")
if not os.path.exists(binary):
    sys.exit(f"binary not found: {binary} (build it with "
              f"`make -C scratch flexnngine2_rtl3_test` in gem5_cva6, or "
              f"pass --binary/--gem5-cva6)")

ACCEL_BASE = 0x10000000  # must stay clear of the DRAM range below

system = m5.objects.System()
system.clk_domain = SrcClockDomain(clock="1GHz", voltage_domain=VoltageDomain())
system.mem_mode = "timing"
# scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.ld places .text at
# 0x80000000 and the test's A/B/C staging buffers up to ~0x80404000 --
# 64MiB gives headroom without overlapping ACCEL_BASE.
system.mem_ranges = [AddrRange(0x80000000, size="64MiB")]

system.cpu = RiscvTimingSimpleCPU()
system.cpu.createThreads()
system.cpu.createInterruptController()

system.membus = SystemXBar()
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

system.accel = Flexnngine2Rtl3Accel(pio_addr=ACCEL_BASE)
system.accel.pio = system.membus.mem_side_ports
system.accel.dma = system.membus.cpu_side_ports

system.mem_ctrl = SimpleMemory(range=system.mem_ranges[0])
system.mem_ctrl.port = system.membus.mem_side_ports

system.system_port = system.membus.cpu_side_ports
system.workload = RiscvBareMetal(bootloader=binary)

root = Root(full_system=True, system=system)

print("Instantiating SimObjects...")
m5.instantiate()

print(f"Beginning simulation, Flexnngine2Rtl3Accel MMIO at 0x{ACCEL_BASE:x}, "
      f"binary={binary}")

tohost_val = 0
exit_cause = "Maximum simulation ticks reached"
tick_step = 100_000_000
while m5.curTick() < args.max_ticks:
    exit_event = m5.simulate(tick_step)
    tohost_val = int.from_bytes(
        system.physProxy.read(TOHOST_ADDR, 8), "little")
    if tohost_val != 0:
        exit_cause = f"tohost written with value {tohost_val}"
        break
    exit_cause = exit_event.getCause()
    if exit_cause != "simulate() limit reached":
        break

print("\n============================================")
print(f"Simulated ticks: {m5.curTick()}")
if tohost_val == 1:
    print(f"tohost = {tohost_val} -> SUCCESS")
    sys.exit(0)
elif tohost_val != 0:
    code = tohost_val >> 1 if tohost_val > 1 else 1
    print(f"tohost = {tohost_val} -> FAILURE (exit code {code})")
    sys.exit(code)
else:
    print(f"Simulation ended because: {exit_cause}")
    sys.exit(1)
