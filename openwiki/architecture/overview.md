---
type: architecture-overview
title: AXION Architecture Overview
description: How AXION bridges gem5's event-driven SimObjects to Verilator-simulated RTL over a pin-level AXI4 bus, the five abstract base classes that give the bridge a real gem5 class hierarchy, and the two ways to attach a DUT.
tags: [gem5, verilator, axi4, rtl-cosimulation, architecture, pcie]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-a2371d6362e5db4bc834ad03
    resource: repo://CLAUDE.md
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-1740306ca7cdbbefa703c011
    resource: repo://src/axi/verilated_model.hh
  - id: openwiki-source-67daf19517147f1306769726
    resource: repo://src/cpu/rtl/rtl_base_cpu.hh
  - id: openwiki-source-1e60a83d400d3a7077583781
    resource: repo://src/dev/rtl/rtl_dma_device.hh
  - id: openwiki-source-5788c4c455c2691a80e41fde
    resource: repo://src/dev/rtl/rtl_pci_device.hh
  - id: openwiki-source-74dee2dcb2f6b4c14d4150e5
    resource: repo://src/dev/rtl/rtl_pcie_tlp_device.hh
  - id: openwiki-source-615e789434ef4ff30faf2ee9
    resource: repo://src/dev/rtl/rtl_pio_device.hh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# AXION Architecture Overview

**AXION** is a gem5 extension that bridges gem5's event-driven `SimObject`s to
RTL blocks simulated by Verilator, over a genuine, pin-level AXI4 bus (5
channels, ID-tagged, same-ID-in-order / cross-ID-out-of-order) and, for the
PCIe paths, onto gem5's own PCI endpoint model. The goal is architecturally
accurate co-simulation: a leaf device is a real subclass of the matching gem5
base class (`BaseCPU`, `PioDevice`, `DmaDevice`, `PciEndpoint`), not a
side-channel shim, so it participates in gem5's port/event/checkpoint
machinery like any native `SimObject` while its actual register/DMA/execution
behavior is driven, cycle by cycle, by a Verilator model of real RTL.

## The five abstract base classes

Five C++ classes give this inheritance hierarchy, all abstract — each defines
a pin-accessor contract and owns the protocol/timing engine(s) that drive it,
but a concrete leaf `SimObject` must implement the pin accessors against a
specific Verilator-generated top module:

- **`RTLBaseCpu : BaseCPU`** (`src/cpu/rtl/`) — a CPU core whose execution
  happens inside the RTL. Structurally complete and compiling, but has no
  worked example core in this repository (see
  [RTL Device Base Classes](/openwiki/reference/rtl-device-classes.md)).
- **`RTLPioDevice : PioDevice`** (`src/dev/rtl/`) — a plain memory-mapped
  register/control device. See
  [fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md).
- **`RTLDmaDevice : DmaDevice`** (`src/dev/rtl/`) — a device with both a
  control/status register port and a bus-master DMA port. gem5's own
  `DmaDevice` already extends `PioDevice`, so `RTLDmaDevice` is transitively a
  `PioDevice` too — no multiple inheritance from `RTLPioDevice` is needed or
  used. See [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md).
- **`RTLPciDevice : PciEndpoint`** (`src/dev/rtl/`) — a PCIe endpoint where
  gem5 owns config space/BARs/INTx and the RTL sees only ordinary AXI4 plus
  an interrupt pin, the same split a PCIe hard IP block and its user logic
  have on real hardware. See
  [PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md).
- **`RTLPcieTlpDevice : PciEndpoint`** (`src/dev/rtl/`) — the same idea one
  protocol layer deeper: the RTL receives genuine PCIe transaction-layer
  packets on four AXI4-Stream channels and parses them itself. See
  [PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md).

## Composition over a shared base

`RTLPioDevice`/`RTLDmaDevice`/`RTLBaseCpu` sit in three gem5 base classes that
are already related or unrelated in ways that rule out a fourth shared
inheritance layer: a common base would either duplicate `PioDevice` (multiple
inheritance through both `RTLDmaDevice` and `RTLPioDevice`) or make no sense
for `RTLBaseCpu` at all. Instead, shared behavior lives in two composed
(has-a) helper classes in `src/axi/` — `Axi4SlaveEngine` and
`Axi4MasterEngine` — plus a `VerilatedRtlModel<TopT>` RAII wrapper around the
Verilated model itself. See
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md) for how
they work; every leaf class owns one or both by composition rather than by
inheriting protocol logic.

## Per-cycle protocol pattern

Every engine's `tick()` follows the same shape each gem5 cycle: drive
combinational inputs for the current state → `axiEval()` → sample which
handshakes completed → toggle `clk` 0→1→0 (the RTL's registers actually
commit on that rising edge) → advance state. `Axi4MasterEngine::tick()` takes
a `driveClock` parameter for the case where multiple engines share one
underlying Verilated model (e.g. `RTLBaseCpu`'s inst/data ports both fed by a
single RTL core) — only one of them should toggle the shared model's actual
clock pin per gem5 cycle; see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md) for the
real deadlock this guards against.

## Two ways to add RTL

**Direct-link (default).** A leaf `#include`s its Verilator-generated
`Vxxx_top.h` directly and owns one `VerilatedRtlModel<TopT>`; no `dlopen`/
abstract-backend indirection. This is a deliberate simplification versus the
prior art on this machine (see
[Prior Art](/openwiki/reference/prior-art.md)) — swapping RTL backends at
runtime is not a goal for this scaffold. A matching `.py` `SimObject`
description links the leaf into the `gem5.opt` build directly.

**Plugin (opt-in, additive).** `RTLPioDevicePlugin`/`RTLDmaDevicePlugin`
satisfy the same `Axi4SlavePins`/`Axi4MasterPins` contract as any direct-link
leaf, but `dlopen` a `.so` at construction and drive it through a small,
ABI-stable plain-C interface (`src/axi/axi4_plugin_abi.h`) instead of binding
to a `Vxxx_top.h` at compile time — no new C++ leaf class or `.py` file needed
per model. This exists because every DUT is wired through the same
`hw/axi4/axi4_pins.sv` adapter, so the flat pin names are identical across
every possible model and only the generated Verilator class name differs.
See [Plugin Path](/openwiki/architecture/plugin-abi.md). This path is purely
additive — it changes nothing about the direct-link default described above.

## The SystemVerilog layer (`hw/`)

`hw/` holds the reusable SystemVerilog side, one directory per interface:

- `hw/axi4/`: `axi4_pkg.sv` (typedefs), `axi4_if.sv` (the actual 5-channel
  interface DUTs connect to), `axi4_pins.sv` (flat-port adapters — the only
  form Verilator's generated C++ model exposes to code — for both roles:
  `axi4_pins_slave_port` for a DUT-as-slave PIO/register port,
  `axi4_pins_master_port` for a DUT-as-master DMA port).
- `hw/pcie/`: the same idea one protocol layer up, for the TLP-level PCIe
  path — `pcie_tlp_pkg.sv` (TLP header field encode/decode), `pcie_tlp_if.sv`
  (the four AXI4-Stream channels a PCIe hard IP block exposes), and
  `pcie_tlp_pins.sv` (the flat-port adapter).

A **TOP** module (e.g. `examples/fifo_pio_accel/fifo_pio_top.sv`) wires one or
more of these pin adapters to the **DUT** (Design Under Test) — the actual
SystemVerilog design under simulation, as distinct from the harness that
connects it to the pin adapters. `fifo_pio_accel.sv` and similar are DUTs;
`*_top.sv` files are harnesses.

## Source layout constraint

AXION's own sources live at the repo root (`src/`, `examples/`), and are
mirrored — never edited directly — into `ext/gem5/src` so gem5's own SCons
build can see them. See
[Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md)
for why this mirroring exists and why editing under `ext/gem5/src/{axi,pcie,
cpu/rtl,dev/rtl,examples}` directly is forbidden.
