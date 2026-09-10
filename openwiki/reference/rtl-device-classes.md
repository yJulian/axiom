---
type: reference
title: RTL Device Base Classes
description: Concrete reference for AXION's five leaf-facing abstract base classes (RTLBaseCpu, RTLPioDevice, RTLDmaDevice, RTLPciDevice, RTLPcieTlpDevice) — gem5 base, engines/ports owned, SimObject params, idle-gating, and RTLBaseCpu's no-worked-example scope note.
tags: [rtl-base-class, gem5, simobject, idle-gating]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-67daf19517147f1306769726
    resource: repo://src/cpu/rtl/rtl_base_cpu.hh
  - id: openwiki-source-74defa1b692caf20e94b2497
    resource: repo://src/cpu/rtl/RTLBaseCpu.py
  - id: openwiki-source-1e60a83d400d3a7077583781
    resource: repo://src/dev/rtl/rtl_dma_device.hh
  - id: openwiki-source-615e789434ef4ff30faf2ee9
    resource: repo://src/dev/rtl/rtl_pio_device.hh
  - id: openwiki-source-f86f5dc6dda6cb43e8cfbf85
    resource: repo://src/dev/rtl/RTLDmaDevice.py
  - id: openwiki-source-067d29075fd77585a3a7fa8d
    resource: repo://src/dev/rtl/RTLPioDevice.py
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# RTL Device Base Classes

AXION defines five abstract C++ base classes, each giving a real gem5
inheritance root and each abstract in its `.py` (`abstract = True` —
instantiate a concrete leaf, never these directly). See
[AXION Architecture Overview](/openwiki/architecture/overview.md) for why
none of them share a common RTL-specific base.

## `RTLPioDevice : PioDevice`

`src/dev/rtl/rtl_pio_device.{hh,cc}`, `RTLPioDevice.py`. The device's own
address range and MMIO queueing live here; the AXI4 pin handshake itself is
delegated to a composed `axion::Axi4SlaveEngine`. A leaf implements
`axion::Axi4SlavePins` against its specific Verilated top module (see
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md)).

`RtlPioPort` (a `SimpleTimingPort`) queues requests and answers them only
once the AXI4 handshake with the RTL has actually completed, so the
requester observes real hardware latency — mirrors `gem5_cva6`'s
`RtlAccelerator::AccelPioPort`. `read()`/`write()` (the legacy `PioDevice`
entry points) are implemented but never called, since the custom timing
port replaces the default one via `getPort()`.

**Params:** `pio_addr` (required), `pio_size` (default `0x1000`),
`pio_latency` (default `0ns` — extra latency on top of the RTL AXI4
handshake latency, modeling device-side bus overhead), `reset_cycles`
(default 10).

## `RTLDmaDevice : DmaDevice`

`src/dev/rtl/rtl_dma_device.{hh,cc}`, `RTLDmaDevice.py`. Bridges over
**two** full AXI4 ports: a slave port for control/status registers (the
same `Axi4SlaveEngine` `RTLPioDevice` uses) and a master port for DMA into
system memory (`Axi4MasterEngine`, with real ID-ordered out-of-order
completion — see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)).
gem5's own `DmaDevice` already extends `PioDevice`, so `RTLDmaDevice` is
transitively a `PioDevice` too — no separate inheritance from
`RTLPioDevice` (that would create a duplicate `PioDevice` base); the two
device classes share code via composition of the same engine classes
instead. A leaf implements both `axion::Axi4SlavePins` (control/status)
and `axion::Axi4MasterPins` (DMA) against a concrete Verilated top module
(see [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md)).

**Params:** `pio_addr`, `pio_size` (default `0x1000`), `pio_latency`
(default `0ns`), `reset_cycles` (default 10), and `idle_gate_cycles`
(default 16) — the optional idle-clock-gating hint: consecutive cycles the
RTL must report `isIdle()` with no pending PIO/DMA work before the clock is
gated (stops ticking until the next PIO request or DMA completion); `0`
disables gating; has no effect on a leaf that doesn't override `isIdle()`
(default: never idle). See
[flexnngine2_rtl3_accel](/openwiki/examples/flexnngine2-rtl3-accel.md) for
the real deadlock this gating mechanism's sibling fix (shared-clock double
toggle) addressed, and why `isIdle()` must reflect the DUT's *own* busy
state rather than gem5-side AXI activity alone.

## `RTLPciDevice : PciEndpoint`

`src/dev/rtl/rtl_pci_device.{hh,cc}`, `RTLPciDevice.py`. A PCIe endpoint
where gem5 owns config space/BAR decode/INTx and the RTL sees ordinary
AXI4 plus an interrupt pin. Full detail in
[PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md).

**Params:** `rtl_pio_latency` (default `0ns`, distinct from `PciDevice`'s
own `pio_latency`), `reset_cycles` (default 10), `idle_gate_cycles`
(default 16, with an extra "not currently asserting an interrupt"
condition versus `RTLDmaDevice`'s gating). No `pio_addr`/`pio_size` — the
BAR (which a leaf must configure, along with `VendorID`/`DeviceID`/
`ClassCode`) defines the address range instead.

## `RTLPcieTlpDevice : PciEndpoint`

`src/dev/rtl/rtl_pcie_tlp_device.{hh,cc}`, `RTLPcieTlpDevice.py`. The same
PCIe split one protocol layer deeper — the RTL receives genuine
transaction-layer packets on four AXI4-Stream channels and parses them
itself, rather than plain AXI4. Full detail in
[PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md).
Structurally near-identical to `RTLPciDevice` (same deferred-response
timing port, range-forwarder, interrupt-edge sampling, idle gating);
`PcieTlpCompleterEngine`/`PcieTlpRequesterEngine` replace
`Axi4SlaveEngine`/`Axi4MasterEngine`.

## `RTLBaseCpu : BaseCPU`

`src/cpu/rtl/rtl_base_cpu.{hh,cc}`, `RTLBaseCpu.py`. Bridges a Verilator-
simulated RTL core over **two full AXI4 master ports** (instruction, data)
— the RTL core is the AXI4 *master* on both, the reverse role from
`RTLDmaDevice`'s DMA port, each backed by its own `Axi4MasterEngine` with
real ID-ordered out-of-order completion. Its tick loop is modeled on
`BaseKvmCPU` (`src/cpu/kvm/base.hh`) rather than `AtomicSimpleCPU`: run the
external engine (here, the RTL core) for a slice of cycles, then drain
whatever AXI4 transactions it issued — because the actual instruction
execution happens inside the RTL, not in gem5.

A leaf implements two pure-virtual pin-accessor hooks:
`instAxiPins()`/`dataAxiPins()`, each wrapping a specific Verilated core's
instruction-side/data-side AXI4 master pins. These **may alias the same
object** for a core with one unified AXI4 master port — `RtlCorePort`'s doc
comment explains the consequence: if `instAxiPins()`/`dataAxiPins()` return
the same underlying pins, `RTLBaseCpu::tick()` only ticks `instPort_`'s
engine at all, skipping `dataPort_.tick()` entirely (not merely clock-
suppressing it), because `Axi4MasterEngine::tick()` unconditionally
samples/drives the AR/AW/W/R/B channels regardless of its `driveClock`
argument (that parameter only gates the `clk_i` toggle) — two independent
engines both sampling/driving the *same* physical AXI channel every cycle
would race each other for the same transactions. `dataPort_` stays a live,
connectable gem5 `RequestPort` in the shared case; it simply never issues
anything.

`postTick()` is a no-op-by-default hook for a leaf CPU to run per-cycle
debug/exit logic once both AXI4 ports have ticked for a cycle (e.g.
polling core-specific ebreak/illegal-instruction pins and calling
`exitSimLoop()`, or updating instruction/op counters from a commit-count
pin) — only called once reset has completed.

**Params (`RTLBaseCpu.py`):** deliberately just `reset_cycles` (default
10) — `icache_port`/`dcache_port` are inherited from `BaseCPU` and must
**not** be redeclared, since shadowing an inherited `Param` creates a
disconnected field in the generated C++ params struct that the parent's
C++ constructor never sees (a confirmed footgun, per `gem5_cva6`'s
`Cva6RtlCPU.py` comment on the same issue — see
[Prior Art](/openwiki/reference/prior-art.md)). `RTLBaseCpu` is
deliberately ISA-agnostic: a concrete leaf class is responsible for its
own `ArchISA`/`ArchDecoder`/`ArchInterrupts`/`ArchMMU` wiring in its own
`__init__`, exactly as any other `BaseCPU` subclass (e.g. gem5's own
`RiscvTimingSimpleCPU`).

### Scope note: no worked example core

`RTLBaseCpu` ships as a structurally complete, compiling abstract base —
the AXI4 master pin contract and port/tick plumbing are real and match
`RTLPioDevice`/`RTLDmaDevice`'s patterns — but has **no worked example
core** in this repository: there is no CPU-core-shaped RTL to hand in this
pass, only the FIFO/PIO accelerator. A concrete leaf class wrapping a real
core would also own `ThreadContext`/ISA/interrupt wiring, exactly like any
other `BaseCPU` subclass, since gem5's checkpointing and interrupt-
injection machinery needs a real `ThreadContext`, and only a concrete core
implementation can meaningfully provide one — the RTL core holds the
actual PC/register state, not gem5. `RTLBaseCpu` is also the one base
class not yet covered by
[the AXI4 signal-completeness audit](/openwiki/reference/axi4-signal-coverage.md),
since its reversed master/slave role split needs that work redone rather
than assumed to carry over from `RTLDmaDevice`'s audit.
