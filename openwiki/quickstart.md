---
type: quickstart
title: Quickstart
description: Task-routing map for the AXION wiki — what AXION is, and where to go for architecture, build/run workflows, per-example walkthroughs, and reference material.
tags: [quickstart, routing, gem5, verilator, axi4]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-a2371d6362e5db4bc834ad03
    resource: repo://CLAUDE.md
  - id: openwiki-source-012f2c78e3b1446dfc35803f
    resource: repo://Makefile
  - id: openwiki-source-4c452e03a7d9866e4f9185f9
    resource: repo://scripts/setup_env.sh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Quickstart

**AXION** is a gem5 extension that bridges gem5's event-driven `SimObject`s
to RTL blocks simulated by Verilator, over a genuine, pin-level AXI4 bus (5
channels, ID-tagged, same-ID-in-order / cross-ID-out-of-order) and, for the
PCIe paths, onto gem5's own PCI endpoint model. Five abstract C++ base
classes (`RTLBaseCpu`, `RTLPioDevice`, `RTLDmaDevice`, `RTLPciDevice`,
`RTLPcieTlpDevice`) give real gem5 inheritance roots; a leaf `SimObject`
implements a pin-accessor contract against a specific Verilator-generated
top module, and composed engine classes (`Axi4SlaveEngine`,
`Axi4MasterEngine`, and their TLP-level counterparts) drive the protocol.
RTL is linked directly by default (no `dlopen` indirection), with an
additive, opt-in `dlopen`-based plugin path also available.

## Where to go

**Understanding the architecture** — start here if you're new to the repo:

- [AXION Architecture Overview](/openwiki/architecture/overview.md) — the
  big picture: the five base classes, why behavior is composed rather than
  inherited, the per-cycle protocol pattern, and the two ways to add RTL.
- [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md) —
  how `Axi4SlaveEngine`/`Axi4MasterEngine` actually drive AXI4 pins from
  gem5 packets, including the AXI-ID out-of-order completion logic.
- [RTL Device Base Classes](/openwiki/reference/rtl-device-classes.md) —
  concrete per-class reference: gem5 base, engines/ports owned, `.py`
  params, and `RTLBaseCpu`'s no-worked-example scope note.
- [Plugin Path (dlopen ABI)](/openwiki/architecture/plugin-abi.md) — the
  opt-in runtime-generic RTL loading path and its plain-C ABI.
- [PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md)
  and
  [PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md)
  — the two PCIe integration levels, gem5-owns-config-space vs
  RTL-parses-TLPs-itself.
- [Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md)
  — why `ext/gem5/src/{axi,pcie,cpu/rtl,dev/rtl,examples}` must never be
  edited directly.

**Building and running:**

- [Building and Running AXION](/openwiki/workflows/build-and-verilate.md)
  — the top-level `Makefile`: verilate-before-gem5 ordering, building
  `gem5.opt`, and running each worked example.
- [Verifying Without gem5 or a RISC-V Toolchain](/openwiki/workflows/testing-without-gem5.md)
  — the cocotb testbenches and gtest suites that verify RTL and protocol
  logic without needing gem5 or a RISC-V cross-toolchain (this environment
  has neither a compiled RISC-V binary nor a way to produce one).

**Worked examples**, roughly simplest to most involved:

- [fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md) — the simplest
  complete example, a pure `RTLPioDevice` FIFO register block.
- [fifo_pio_accel_plugin](/openwiki/examples/fifo-pio-accel-plugin.md) —
  the same DUT built through the plugin path instead.
- [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md) — the
  `RTLDmaDevice` example, three AXI-ID-tagged concurrent DMA channels.
- [pcie_template_accel](/openwiki/examples/pcie-template-accel.md) and
  [pcie_tlp_template_accel](/openwiki/examples/pcie-tlp-template-accel.md)
  — the same PCIe endpoint device built on each of the two PCIe
  abstraction levels, for direct comparison.
- [flexnngine2_rtl3_accel](/openwiki/examples/flexnngine2-rtl3-accel.md) —
  an externally-sourced GEMM accelerator, the first `RTLDmaDevice` run
  against a real gem5 CPU, which found and fixed two real framework bugs.

**Reference material:**

- [AXI4 Signal Coverage](/openwiki/reference/axi4-signal-coverage.md) —
  which pins are audited-complete, what's outstanding for `RTLBaseCpu`,
  and two documented-not-fixed gaps in `Axi4MasterEngine`.
- [Prior Art: gem5_cva6 and gem5-verilator-ghdl](/openwiki/reference/prior-art.md)
  — the two sibling on-machine projects AXION's design was mined from.

## Fastest path to "does it work"

```bash
scripts/setup_env.sh   # one-time: checkout ext/gem5 submodule + mirror
make cocotb-env        # one-time: cocotb venv
make tb                # verify fifo_pio_accel's RTL, no gem5 needed
make verilate && make gem5   # build gem5.opt (needs a RISC-V ELF to actually run)
```
