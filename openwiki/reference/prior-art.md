---
type: reference
title: "Prior Art: gem5_cva6 and gem5-verilator-ghdl"
description: The two sibling on-machine projects AXION was designed by mining — gem5_cva6's dlopen'd AccelInterface/RtlAccelerator/dma_master_engine.cc, and gem5-verilator-ghdl's earlier build-system gotchas — and which specific AXION pieces model which.
tags: [prior-art, gem5_cva6, dlopen, build-system]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-a2371d6362e5db4bc834ad03
    resource: repo://CLAUDE.md
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Prior Art: gem5_cva6 and gem5-verilator-ghdl

Two sibling projects on this machine solve adjacent problems and were mined
for patterns while designing AXION — worth checking if something in this
repository seems to be reinventing a wheel.

## `~/gem5_cva6`

A mature, working gem5↔Verilator↔AXI bridge for the CVA6 RISC-V core,
using a `dlopen`'d abstract-interface pattern (`AccelInterface`,
`RtlCoreInterface`) instead of AXION's direct-link pin contracts. Several
AXION pieces are direct models of specific `gem5_cva6` pieces:

- **`RtlAccelerator`** (a `DmaDevice`) is the direct model for
  [`RTLDmaDevice`](/openwiki/reference/rtl-device-classes.md).
- **`src/accel/dma_master_engine.cc`**, specifically its
  `pickOldestEligible()` selection rule, is the direct model for
  [`Axi4MasterEngine`](/openwiki/architecture/axi4-bridge-engines.md)'s
  same-ID-in-order/cross-ID-out-of-order completion arbitration
  (`pickOldestEligibleRead()`/`Write()`) — described by AXION's own source
  comments as "the one piece of real, working full-AXI4-with-IDs logic
  anywhere," worth revisiting `gem5_cva6`'s source if that logic ever needs
  reworking.
- **`AccelInterface`/`RtlAccelerator`'s `dlopen` mechanics** are the direct
  model for AXION's own opt-in
  [plugin path](/openwiki/architecture/plugin-abi.md), including
  `RtlAccelerator`'s pattern of resolving the `.so` at construction rather
  than deferring to `init()` (which `RTLPioDevicePlugin`/
  `RTLDmaDevicePlugin` also follow). **One deliberate deviation**: AXION's
  plugin path uses a plain C ABI (`src/axi/axi4_plugin_abi.h`) instead of a
  `dlopen`'d C++ vtable, because C++ vtable/ABI layout isn't guaranteed
  stable across an independently-compiled shared-library boundary the way a
  plain C struct/function ABI is — `gem5_cva6`'s `AccelInterface` does
  `dlopen` a C++ vtable and relies on both sides being built with a
  matching compiler/stdlib.
- The mirroring convention AXION uses (`src/axi` → `ext/gem5/src/axi`, not
  some wrapped or prefixed location) matches how `gem5_cva6` mirrors its
  own custom sources (`gem5/src/cpu/rtl/`) — see
  [Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md).
- `plugin/rtl_plugin.mk`'s Verilator-to-`.so` build recipe (`verilator
  --cc`, then `make -f Vxxx.mk`, then a manual `g++ -shared -fPIC` link)
  mirrors the proven pattern from `~/gem5_cva6/accelerator/Makefile`.
- The idle-clock-gating pattern `RTLDmaDevice`/`RTLPciDevice` use
  (`isIdle()` hook + `idle_gate_cycles` hysteresis counter, default 16) is
  ported from `gem5_cva6`'s `RtlAccelerator`/`AccelInterface::is_idle()` —
  see [flexnngine2_rtl3_accel](/openwiki/examples/flexnngine2-rtl3-accel.md)
  for the bug this port fixed.

## `~/Development/gem5-verilator-ghdl`

An earlier, less mature prototype. Its central design choice — "flatten
AXI inside the RTL, cross the C++ boundary with scalars" — is explicitly
what AXION does **not** do, because it loses AXI ID and out-of-order
semantics that AXION's whole `Axi4MasterEngine` design exists to preserve.
Despite that, its build-system gotchas are real and worth remembering if
AXION's own RTL build ever breaks in a similar way:

- The `cp -rs` mirroring trick this repo also uses (see
  [Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md))
  to make gem5's `followlinks=False` SConscript walk see externally-owned
  sources.
- The `verilated_fst_c.cpp`-must-be-compiled-in-not-`--whole-archive`d FST
  link fix — a specific linker gotcha around Verilator's FST trace support
  that this project already worked through once.
