---
type: architecture-component
title: "PCIe Path: AXI4-Bridge Endpoint"
description: How RTLPciDevice splits a PCIe endpoint between gem5 (config space, BAR decode, INTx) and RTL (ordinary AXI4 user logic), including BAR-relative addressing, DMA address translation, interrupt edges, and the RangeForwardPort fix.
tags: [pcie, axi4, gem5, pci-endpoint, interrupts, dma]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-9cb87773f897ec5a5db390ce
    resource: repo://src/dev/rtl/rtl_pci_device.cc
  - id: openwiki-source-5788c4c455c2691a80e41fde
    resource: repo://src/dev/rtl/rtl_pci_device.hh
  - id: openwiki-source-0b1acda11b25f62b73336e0b
    resource: repo://src/dev/rtl/RTLPciDevice.py
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# PCIe Path: AXI4-Bridge Endpoint

`RTLPciDevice` (`src/dev/rtl/rtl_pci_device.{hh,cc}`, `RTLPciDevice.py`) is a
fourth abstract base alongside `RTLPioDevice`/`RTLDmaDevice`/`RTLBaseCpu`,
extending gem5's `PciEndpoint`. It implements the same split a real FPGA
design has: gem5 plays the **PCIe hard IP block** (Xilinx PCIe-to-AXI Bridge /
XDMA and equivalents) — owning the 256-byte config header, BAR decode, and
INTx routing through the PCI host — while the RTL is the **user logic**
behind it and sees only two ordinary AXI4 ports plus an interrupt pin.
Nothing in `hw/axi4/` or in the DUT knows PCIe exists.

`PciDevice` already extends `DmaDevice` (which extends `PioDevice`), so
`RTLPciDevice` is transitively a `PioDevice` too — the same "compose, don't
multiply-inherit" pattern the other RTL device bases use: behavior is shared
via the `Axi4SlaveEngine`/`Axi4MasterEngine` engine classes
(see [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)),
not a common RTL base class. A leaf implements `Axi4SlavePins`,
`Axi4MasterPins`, and `rtlGetIrq()` against a concrete Verilated top module
(see [pcie_template_accel](/openwiki/examples/pcie-template-accel.md)).

## BAR-relative addressing

The BAR window is driven through the same `Axi4SlaveEngine` `RTLPioDevice`/
`RTLDmaDevice` use, but `pioStart()` runs the guest-visible address through
`PciDevice::getBAR()` first and issues via the `Axi4SlaveEngine::issue(pkt,
axiAddr, onDone)` overload that drives an explicit address, so the RTL sees
a fixed BAR-relative offset rather than wherever the guest's enumeration
happened to map BAR0. `pioStart()` `panic`s if `getBAR()` fails, since the
bus only ever routes addresses this device itself advertised.

BAR accesses are timing-only: `RtlPioPort::recvTimingReq()` queues non-config
requests onto `pioQueue` and calls `wakeUp()`; `recvAtomic()` panics for a
BAR access (`"BAR accesses to an RTL-backed PCIe device are timing-only"`),
since completing one means running the RTL clock, which only the tick loop
does. Config-space accesses are the one exception: `isConfigAccess()` checks
`upstreamInterface.configRange()`, and such packets are answered
synchronously out of `PciDevice`'s own header state without ever reaching
the RTL — a hard IP block's user logic has no config space either.
`readDevice()`/`writeDevice()` (`PciDevice`'s two pure virtuals) are
implemented only to panic cleanly, since the timing path never calls them.

## DMA

Bus-master DMA runs through the same `Axi4MasterEngine` `RTLDmaDevice`'s DMA
port uses. `issueRead()`/`issueWrite()` (the `Axi4MasterEngine::Backend`
overrides) translate the RTL-driven PCI bus address through `pciToDma()`
before calling gem5's `dmaRead()`/`dmaWrite()` — a no-op under HiFive's
default `pci_dma_base` of 0, but not in general, and wrong translation is
invisible until someone configures a nonzero base. Each completion event
calls `masterEngine.completeRead()`/`completeWrite()` and then `wakeUp()`.

## Interrupts

A leaf implements `rtlGetIrq()`; `sampleIrq()` runs every tick and turns
*edges* on that pin into `intrPost()`/`intrClear()` — only a 0→1 or 1→0
transition triggers a call, not every cycle it's held. On RISC-V, the PLIC
source the interrupt actually lands on is derived from the device's **PCI
slot**, not its `InterruptLine` param:
`GenericRiscvPciHost::mapPciInterrupt()` returns `int_base + (pci_dev %
int_count)`, i.e. `0x10 + pci_dev` under HiFive's defaults — getting this
wrong looks like "the interrupt never arrives."

## The `RangeForwardPort` fix

gem5's own config-space code calls `pioPort.sendRangeChange()` directly at
several places in `PciDevice::writeConfig()`/`PciEndpoint::writeConfig()` —
on every BAR write and every COMMAND-register write — which is how a BAR
window becomes visible on the bus at all. But like the other RTL device
bases, `getPort("pio")` hands out `rtlPio` and leaves the inherited
`pioPort` unbound, and `ResponsePort::sendRangeChange()` dereferences its
peer unconditionally, which would segfault. Rather than reimplementing (and
forever re-syncing) gem5's BAR-decode logic to redirect those calls,
`init()` binds `pioPort` to a `RangeForwardPort` stub — a `RequestPort`
whose only real behavior is `recvRangeChange() { dev.rtlPio.sendRangeChange();
}`; `recvTimingResp()`/`recvReqRetry()` both `panic()`, since no packet is
ever routed through it. This binding must happen in `init()` (before
anything can write a BAR or the COMMAND register), not `startup()`. With
that stub in place, `PioDevice::init()`'s own `pioPort.sendRangeChange()`
call is not merely safe but correct — so unlike `RTLPioDevice`/`RTLDmaDevice`,
`RTLPciDevice::init()` does call up to `PciEndpoint::init()`.

## Reset and idle-gating

`tick()` first drives reset for `resetCycles` cycles (`driveResetInputs()`
holds `rst_n` low and toggles the clock once per gem5 cycle), then begins
normal operation: `pioStart()` (issue the next queued BAR access if the
slave engine is free), `slaveEngine.tick()`, `masterEngine.tick(false)`
(the shared Verilated model's clock pin was already toggled by
`slaveEngine.tick()` this cycle, so the master engine must not toggle it
again), then `sampleIrq()`.

Idle clock gating reuses `RTLDmaDevice`'s `isIdle()` hint (default `false`,
opt-in per leaf) and an `idle_gate_cycles` param (default 16), but with one
extra condition beyond `RTLDmaDevice`'s: the device must also **not** have an
asserted interrupt line, since nothing on the gem5 side calls `wakeUp()` when
software acknowledges an interrupt (that acknowledgment write is a BAR
access, which does wake the device, but the RTL's own deassertion happens a
cycle or two later) — staying awake while `irqAsserted` is true keeps that
falling edge observable. `wakeUp()` resets `idleCycles` and reschedules the
tick event if it isn't already scheduled; `pioStart()`/`issueRead()`/
`issueWrite()`'s completion events all call it.

## Params (`RTLPciDevice.py`)

`rtl_pio_latency` (default `0ns`) is named distinctly from `PciDevice`'s own
`pio_latency` param — the latter is the flat per-access latency an ordinary
PCI device charges, while `rtl_pio_latency` is only the extra device-side bus
overhead added on top of the RTL's own AXI4 handshake latency, since most of
the real latency comes from actually running the AXI4 protocol. `reset_cycles`
(default 10) and `idle_gate_cycles` (default 16, `0` disables gating) round
out the params. Unlike `RTLPioDevice`/`RTLDmaDevice`, there is no
`pio_addr`/`pio_size` param here — the BAR (which a leaf must configure,
along with `VendorID`/`DeviceID`/`ClassCode`) defines the address range, and
the guest's own enumeration decides where it lands.
