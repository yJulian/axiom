---
type: worked-example
title: "Worked Example: pcie_template_accel"
description: The minimal AXI4-bridge PCIe endpoint (RTLPciDevice leaf) — ID/VERSION/SCRATCH/COUNTER registers, a 4KiB scratchpad, single-channel bus-master DMA, an interrupt source, its 8-byte register stride rationale, and the three ways to run it.
tags: [pcie, axi4, rtl-cosimulation, interrupts, worked-example]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-2250ff6b4b4a1e0fdf38df00
    resource: repo://examples/pcie_template_accel/cocotb/test_pcie_template.py
  - id: openwiki-source-c5a5449d2a7f25b35e49df10
    resource: repo://examples/pcie_template_accel/configs/run_pcie_template_linux.py
  - id: openwiki-source-2502ff5d025611a66ae1e772
    resource: repo://examples/pcie_template_accel/configs/run_pcie_template.py
  - id: openwiki-source-ba4af6f1c987e0f381c9ef44
    resource: repo://examples/pcie_template_accel/pcie_template_accel.sv
  - id: openwiki-source-2141277d341dbd2d24973806
    resource: repo://examples/pcie_template_accel/pcie_template_device.hh
  - id: openwiki-source-ebff03578af9c76a360f94c2
    resource: repo://examples/pcie_template_accel/PcieTemplateAccel.py
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: pcie_template_accel

`pcie_template_accel` is AXION's worked example for
[the AXI4-bridge PCIe path](/openwiki/architecture/pcie-integration.md) —
a `RTLPciDevice` leaf that is deliberately a minimal "does it work at all"
endpoint. Like every real FPGA PCIe design built on a hard IP block, this
RTL knows **nothing** about PCIe itself: the hard block (played by gem5)
terminates the link, config space, and BARs, and hands the user logic two
ordinary AXI4 ports plus two sideband pins — `s_axi` (the BAR0 window, DUT
as AXI4 slave, addresses already BAR-relative), `m_axi` (bus-master DMA,
DUT as AXI4 master), `irq_o` (drives an INTx assert/deassert through gem5's
PCI host into the PLIC), and `busy_o` (feeds `RTLPciDevice::isIdle()`'s
clock gating).

## Register map (BAR0, 64 KiB)

| Offset | Register | Access | Width | Meaning |
| --- | --- | --- | --- | --- |
| `0x0000` | `ID` | RO | 32b | `0x50434945` (`"PCIE"`) — proves enumeration + BAR mapping |
| `0x0008` | `VERSION` | RO | 32b | `0x00010000` |
| `0x0010` | `SCRATCH` | RW | 32b | read/write round-trip |
| `0x0018` | `COUNTER` | RO | 32b | free-running cycle counter — proves the RTL clock ticks |
| `0x0020` | `DMA_SRC` | RW | 64b | host-memory source address |
| `0x0028` | `DMA_DST` | RW | 64b | host-memory destination address |
| `0x0030` | `DMA_LEN` | RW | 32b | bytes to copy |
| `0x0038` | `CTRL` | RW | 32b | bit `0`=START (pulse), bit `1`=IRQ_EN, bit `2`=IRQ_FORCE (pulse) |
| `0x0040` | `STATUS` | RO | 32b | bit `0`=BUSY, bit `1`=DONE, bit `2`=IRQ_PENDING |
| `0x0048` | `IRQ_ACK` | WO | 32b | write 1 to bit `0`: clears IRQ_PENDING and DONE |
| `0x1000`–`0x1FFF` | scratchpad RAM | RW | — | 4 KiB, multi-beat bursts + byte strobes |

The DMA engine copies `DMA_LEN` bytes SRC→DST as a sequence of single-beat
8-byte read/write pairs, tagging reads with AXI ID `0` and writes with AXI
ID `1` so both of `Axi4MasterEngine`'s per-ID queues are genuinely
exercised (see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)). A
trailing partial word (`DMA_LEN` not a multiple of 8) is written with a
masked WSTRB.

## Why every register sits on an 8-byte stride

Even though most registers hold only 32 bits, they are laid out one per
8-byte beat: `Axi4SlaveEngine` (`src/axi/axi4_slave_engine.cc`) is
LSB-aligned, not byte-lane-aligned — an n-byte access always travels in
`wdata`/`rdata` bits `[n*8-1:0]` with strobe `(1<<n)-1`, whatever the
address's offset within the 8-byte beat. A strict AXI4 requester (the
cocotb testbench's `AxiMaster`, and real hardware) instead places a 4-byte
access at offset 4 on byte lanes 4..7. The two conventions only agree when
every access starts at a beat boundary, so keeping the register file on an
8-byte stride makes this DUT behave identically under gem5 and under
cocotb, with no lane-shifting logic baked into the register decode.
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md) (regs at
`0x00`/`0x08`) and [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md)
(`0x20` stride) already do the same thing. The scratchpad RAM region is
different: it implements true per-lane WSTRB semantics, so sub-beat writes
there are correct under either requester convention.

## The leaf class (`PcieTemplateAccel`)

`pcie_template_device.hh`/`.cc` implement `axion::Axi4SlavePins` (BAR0
window), `axion::Axi4MasterPins` (bus-master DMA), plus the two sideband
pins `RTLPciDevice` needs that have no AXI4 equivalent: `rtlGetIrq()` and
`isIdle()` (reading the DUT's `irq_o`/`busy_o` directly). Same direct-link
pattern as every other worked example, against `Vpcie_template_top`.

`PcieTemplateAccel.py` sets everything PCIe-specific — none of it reaches
the RTL, exactly the division of labor between a hard IP block and user
logic:

- `VendorID = 0x1DE5`, `DeviceID = 0x0001` — a made-up, not-PCI-SIG-assigned
  pair kept deliberately out of any range a real driver would bind to.
  `ClassCode = 0xFF` ("misc device"), `Revision = 0x01`.
- `BAR0 = PciMemBar(size="64KiB")` — `0x0000`–`0x0FFF` registers,
  `0x1000`–`0x1FFF` scratchpad RAM, rest reserved; must stay a power of two
  and match the DUT's `RAM_WORDS` elaboration parameter.
- `InterruptLine = 0x11`, `InterruptPin = 0x01` (INTx line A) — on RISC-V
  the PLIC source this actually lands on is derived from the device's
  *slot*, not `InterruptLine` (see
  [PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md));
  `InterruptLine` is still set so config-space reads of it are meaningful
  to a guest.

Unlike `FifoPioAccel`, there is deliberately no `generateDeviceTree()`: a
PCIe endpoint is discovered by walking ECAM config space, so it needs no
device-tree node of its own (the host bridge gets one, already emitted by
`RiscvBoard.generate_device_tree()`).

## Three ways to run it, in increasing cost

1. **`make tb-pcie`** — cocotb, no gem5. `test_pcie_template.py` drives
   the BAR0 window's AXI4 slave port with an `AxiMaster` (playing the role
   gem5's `RTLPciDevice` plays once a BAR has been programmed, handing down
   the same BAR-relative offsets) and plays host memory on the bus-master
   DMA port with an `AxiRam` model. Nothing here knows about PCIe — config
   space, BARs, and INTx are gem5's job; what the RTL owes is ordinary
   AXI4 behavior, which is what gets checked. Covers registers, burst +
   strobes, DMA against `AxiRam`, and interrupt assert/ack.
2. **`make run-pcie-example`** — **the actual proof.** Runs
   `examples/pcie_template_accel/riscv_test/pcie_template_test.elf`
   (built by `pcie-test-elf`, using the host's `riscv64-linux-gnu-gcc`,
   with `-static -no-pie` load-bearing — without it the dynamic sections
   displace `.tohost` from the address the config script polls) on a plain
   `RiscvTimingSimpleCPU` against gem5's HiFive platform, with
   `PcieTemplateAccel` plugged into slot 1 of the platform's PCI bus. The
   guest enumerates it over ECAM, programs BAR0, and drives the RTL
   through the resulting window — covering the whole path cocotb cannot:
   PCI config space, BAR decode, bus-master DMA through gem5's real memory
   system, and an INTx arriving at the PLIC. Full system (a PCI device has
   no fixed address for `Process.map()`, so SE mode cannot work) but with
   `RiscvBareMetal` — no kernel, no bootloader, no disk image, nothing to
   download.
3. **`make run-pcie-linux-example ARGS='--disk-image <img>'`** — boots
   Ubuntu under `RiscvBoard` and lets **Linux itself** enumerate and
   assign the BAR (nothing in the config writes a BAR or COMMAND register
   — Linux's own PCI subsystem walks ECAM, sizes BAR0, assigns it an
   address, and publishes it through sysfs); the guest program
   (`riscv_test/pcie_template_linux_ctl.c`) just `mmap()`s
   `/sys/bus/pci/devices/0000:00:01.0/resource0`. Needs the multi-GiB
   Ubuntu disk image, not fetched by default. If the endpoint's config
   space were subtly wrong, this is where it would show. No device-tree
   node is needed for the endpoint itself, only the host bridge.
