"""
run_pcie_tlp_template.py -- AXION's bare-metal TLP-level PCIe example.

Identical in shape to run_pcie_template.py, but with
`PcieTlpTemplateAccel` in the slot instead of `PcieTemplateAccel`: an
endpoint whose RTL receives genuine PCIe transaction-layer packets and
parses them itself (src/dev/rtl/rtl_pcie_tlp_device.hh,
src/pcie/pcie_tlp_engine.cc), rather than being handed plain AXI4.

The guest program is the same test as the AXI4 variant's, and that is the
result worth having: enumeration, BAR programming, register access,
scratchpad bursts, bus-master DMA and INTx all behave the same from
software, because the difference between the two paths is confined to
where the TLP encoding happens -- in gem5 for one, in the RTL for the
other.

Why full system rather than SE mode, unlike run_fifo_pio.py: a PCI device
has no fixed address to Process.map(). Its window only exists once
something has written its BAR, which means config space, which means a
PCI host, which means a Platform -- and Platform only exists in FS mode.
But "full system" here means `RiscvBareMetal`, not Linux: no kernel, no
bootloader, no disk image, nothing to download. run_pcie_template_linux.py
is the Linux variant.

HiFive already ships the PCIe host and bus (see
ext/gem5/src/dev/riscv/HiFive.py): GenericRiscvPciHost with ECAM config
space at 0x30000000, the PCI memory window at 0x40000000, and INTx routed
into the PLIC. Nothing here builds any of that -- it only wires the
platform's existing pci_host/pci_bus to the I/O bus and hangs one device
off it. The bus wiring below is modeled on RiscvBoard._setup_io_devices()
(ext/gem5/src/python/gem5/components/boards/riscv_board.py).

Usage:
    <gem5.opt> run_pcie_tlp_template.py [--binary <elf>]

or just `make run-pcie-tlp-example` at the repo root, which builds the ELF
first.
"""

import argparse
import sys

import m5
from m5.objects import (
    AddrRange,
    Bridge,
    HiFive,
    IOXBar,
    PcieTlpTemplateAccel,
    RiscvBareMetal,
    RiscvDecoder,
    RiscvInterrupts,
    RiscvISA,
    RiscvMMU,
    RiscvRTC,
    RiscvTimingSimpleCPU,
    Root,
    SimpleMemory,
    SrcClockDomain,
    System,
    SystemXBar,
    VoltageDomain,
)
from m5.util import fatal

# Slot on bus 0. Must match PCI_DEV in riscv_test/pcie_tlp_template_test.c --
# it determines both the ECAM address the guest probes and, via
# GenericRiscvPciHost::mapPciInterrupt(), which PLIC source the device's
# INTx lands on (int_base + pci_dev = 0x10 + 1).
PCI_DEV = 1

MEM_BASE = 0x80000000
MEM_SIZE = "256MiB"

# Must match .tohost's address in riscv_test/pcie_tlp_template_test.ld.
TOHOST_ADDR = 0x80008000

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--binary",
    type=str,
    default="examples/pcie_tlp_template_accel/riscv_test/pcie_tlp_template_test.elf",
    help="Bare-metal RISC-V ELF to run",
)
parser.add_argument(
    "--max-ticks",
    type=int,
    default=500_000_000_000,
    help="Give up after this many ticks if the guest never writes tohost",
)
args = parser.parse_args()

system = System()
system.mem_mode = "timing"  # RTLPciDevice's BAR path is timing-only
system.cache_line_size = 64

system.voltage_domain = VoltageDomain(voltage="1.0V")
system.clk_domain = SrcClockDomain(
    clock="1GHz", voltage_domain=system.voltage_domain
)

system.mem_ranges = [AddrRange(MEM_BASE, size=MEM_SIZE)]

system.membus = SystemXBar()
system.iobus = IOXBar()
system.system_port = system.membus.cpu_side_ports

# Platform code indexes system.cpu as a list.
system.cpu = [RiscvTimingSimpleCPU()]
system.cpu[0].createThreads()
system.cpu[0].createInterruptController()
system.cpu[0].icache_port = system.membus.cpu_side_ports
system.cpu[0].dcache_port = system.membus.cpu_side_ports

system.mem_ctrl = SimpleMemory(range=system.mem_ranges[0], latency="30ns")
system.mem_ctrl.port = system.membus.mem_side_ports

# --- Platform (CLINT, PLIC, UART, and the PCI host + bus) ---
system.platform = HiFive()
system.platform.rtc = RiscvRTC(frequency="100MHz")
system.platform.clint.int_pin = system.platform.rtc.int_pin

# --- PCI: connect the platform's existing host and bus to the I/O bus ---
system.iobus.cpu_side_ports = system.platform.pci_host.up_request_port()
system.iobus.mem_side_ports = system.platform.pci_host.up_response_port()
system.platform.pci_bus.cpu_side_ports = (
    system.platform.pci_host.down_request_port()
)
system.platform.pci_bus.default = (
    system.platform.pci_host.down_response_port()
)
system.platform.pci_bus.config_error_port = (
    system.platform.pci_host.config_error.pio
)

# --- The device under test ---
system.pcie_accel = PcieTlpTemplateAccel(pci_dev=PCI_DEV, pci_func=0)
system.pcie_accel.upstream = system.platform.pci_host
system.pcie_accel.pio = system.platform.pci_bus.mem_side_ports
system.pcie_accel.dma = system.platform.pci_bus.cpu_side_ports

# --- CPU -> devices. Ranges come from the platform's own params rather
# than hardcoded constants, so they cannot drift out of sync with HiFive. ---
system.bridge = Bridge(delay="50ns")
system.bridge.cpu_side_port = system.membus.mem_side_ports
system.bridge.mem_side_port = system.iobus.cpu_side_ports
bridge_ranges = system.platform._off_chip_ranges()
bridge_ranges.append(
    AddrRange(
        system.platform.pci_host.conf_base,
        size=system.platform.pci_host.conf_size,
    )
)
bridge_ranges.append(
    AddrRange(system.platform.pci_host.pci_pio_base, size="16MiB")
)
bridge_ranges.append(
    AddrRange(system.platform.pci_host.pci_mem_base, size="512MiB")
)
system.bridge.ranges = bridge_ranges

# --- Devices -> memory. Without this the endpoint's bus-master DMA has no
# route back to DRAM: it leaves the PCI bus via the host's up_request_port
# onto the I/O bus, and stops there. Restricting the bridge to mem_ranges
# is what keeps it from forming a loop with system.bridge above. ---
system.iobridge = Bridge(delay="50ns", ranges=system.mem_ranges)
system.iobridge.cpu_side_port = system.iobus.mem_side_ports
system.iobridge.mem_side_port = system.membus.cpu_side_ports

system.platform.attachOnChipIO(system.membus)
system.platform.attachOffChipIO(system.iobus)
system.platform.attachPlic()
system.platform.setNumCores(1)

system.workload = RiscvBareMetal(bootloader=args.binary)

root = Root(full_system=True, system=system)
m5.instantiate()

print(
    f"TLP-level PCIe endpoint at 00:{PCI_DEV:02x}.0 -- ECAM "
    f"{int(system.platform.pci_host.conf_base) + (PCI_DEV << 3 << 12):#x}, "
    f"BAR window {int(system.platform.pci_host.pci_mem_base):#x}"
)

# Bare-metal has no exit syscall, so the guest reports through tohost
# (see pcie_template_test.c's finish()). Same polling loop shape as
# examples/flexnngine2_rtl3_accel/configs/run_flexnngine2_rtl3.py. The
# whole test runs in well under 100M ticks, so the step is sized to notice
# the result promptly rather than to amortize host-loop overhead -- the
# guest is spinning by then, and every extra step is wasted work.
TICK_STEP = 10_000_000
tohost = 0
cause = f"reached the {args.max_ticks}-tick limit"

while m5.curTick() < args.max_ticks:
    exit_event = m5.simulate(TICK_STEP)
    tohost = int.from_bytes(system.physProxy.read(TOHOST_ADDR, 8), "little")
    if tohost != 0:
        cause = f"tohost = {tohost}"
        break
    if exit_event.getCause() != "simulate() limit reached":
        cause = exit_event.getCause()
        # The exit may have raced the tohost write; re-read before judging.
        tohost = int.from_bytes(
            system.physProxy.read(TOHOST_ADDR, 8), "little"
        )
        break

print("\n" + "=" * 60)
if tohost == 1:
    print("SUCCESS: the TLP-parsing RTL PCIe endpoint passed every check")
    print("  (enumeration, BAR0 mapping, registers, RAM burst, DMA, INTx)")
    sys.exit(0)

if tohost != 0:
    # finish() encodes the failing check as step * 2 + 1; see
    # pcie_template_test.c.
    step = (tohost - 1) // 2
    print(f"FAILURE: guest reported tohost = {tohost} (check #{step} failed)")
    print("  See the check() calls in riscv_test/pcie_tlp_template_test.c")
    sys.exit(1)

print(f"FAILURE: simulation ended without a result ({cause})")
sys.exit(1)
