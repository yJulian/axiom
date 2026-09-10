---
type: worked-example
title: "Worked Example: flexnngine2_rtl3_accel"
description: An externally-sourced GEMM accelerator run both standalone via cocotb and as a real RTLDmaDevice leaf under a full gem5 CPU — the first such end-to-end run, which surfaced and fixed two real AXION framework bugs and left one open GEMM-completion issue.
tags: [dma, axi4, rtl-cosimulation, gemm, idle-gating, worked-example]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-b875e9ccafcc1f0e273f54d0
    resource: repo://examples/flexnngine2_rtl3_accel/flexnngine2_rtl3_device.cc
  - id: openwiki-source-773962990a9b4c14c108ee1f
    resource: repo://examples/flexnngine2_rtl3_accel/flexnngine2_rtl3_device.hh
  - id: openwiki-source-84256d81d11ecf17acee372e
    resource: repo://examples/flexnngine2_rtl3_accel/README.md
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: flexnngine2_rtl3_accel

`rtl3` is the scratchpad-fed FleXNNgine2 DMA GEMM accelerator from the
sibling `gem5_cva6` repo (`ext/flexnngine2_adapter/rtl3` there, see
[Prior Art](/openwiki/reference/prior-art.md)) — a two-tile (A/B) systolic-
array GEMM engine with three distinct AXI IDs on one physical `m_axi` master
port and a chained-ACCUM mode that pins partial sums in the PEs across jobs
instead of round-tripping them through memory. Its own sources are **not**
vendored into this repo — every Makefile/SConscript here pulls them from a
sibling `gem5_cva6` checkout (`GEM5_CVA6` variable, default
`/home/julian/gem5_cva6`).

This example matters beyond being a third `RTLDmaDevice` leaf: it is the
first time `RTLDmaDevice` was ever run against a real gem5 CPU (previously
only `dma_memcopy_accel`'s cocotb path was exercised), and doing so found
two genuine framework bugs in `src/dev/rtl/rtl_dma_device.{hh,cc}`.

## Two ways to exercise it

- **`cocotb/`** drives `flexnngine2_rtl3_top.sv` directly as `TOPLEVEL`, no
  AXION wrapper involved — proves the RTL itself is correct under AXION's
  Verilator + cocotb + cocotbext-axi stack, independent of gem5. Both tests
  pass (`single_tile_gemm`, `accum_chain_gemm`, the latter covering rtl3's
  ACCUM-chain feature). Run via `make tb-rtl3` at the repo root.
- **A real gem5 `RTLDmaDevice` leaf** (`Flexnngine2Rtl3Accel`, this
  directory's `.sv`/`.hh`/`.cc`/`.py`) — the direct-link pattern, same as
  [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md).
  `flexnngine2_rtl3_axion_top.sv` wraps the DUT with a full-AXI4 pin set
  (padding the missing `lock`/`cache`/`prot`/`qos`/`region` fields on both
  ports, and a small ID-latch for the control port, which is genuine
  AXI4-Lite). `configs/run_flexnngine2_rtl3.py` builds a full-system,
  bare-metal (`RiscvBareMetal`, not SE mode) `RiscvTimingSimpleCPU` system
  and reuses **gem5_cva6's own precompiled test ELF**
  (`scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.elf`) unmodified — full-
  system because that ELF signals completion via a `tohost` write, not an
  OS syscall, so SE mode isn't the right fit; this sidesteps AXION's "no
  RISC-V toolchain in this sandbox" limitation entirely by borrowing an
  already-built binary.

```bash
make -C examples/flexnngine2_rtl3_accel        # verilate the wrapper
scripts/build_gem5.sh && scons -C ext/gem5 build/RISCV/gem5.opt -j$(nproc)
./build/RISCV/gem5.opt examples/flexnngine2_rtl3_accel/configs/run_flexnngine2_rtl3.py
```

## `Flexnngine2Rtl3Accel` (leaf class)

Implements both `axion::Axi4SlavePins` (control/status port) and
`axion::Axi4MasterPins` (DMA port) directly against the Verilator-generated
`Vflexnngine2_rtl3_axion_top`, the same direct-link pattern as
`DmaMemcopyAccel` — just against the wrapper top module in this directory
instead of a DUT-native one, since rtl3's own top module needs padding for
the AXI4 fields it doesn't natively drive.

`isIdle()` overrides `RTLDmaDevice`'s idle-gating hook by reading the DUT's
own `dma_busy_o` pin (`!rtl_.top()->dma_busy_o`) rather than inferring
idleness from AXI activity alone — `dma_busy_o` stays high for the whole
duration of a job, including the compute phase where neither AXI port has
any traffic in flight, so gating on AXI-side idleness alone would freeze the
clock mid-computation and nothing would ever un-gate it again.
`debugEngineState()` is a temporary raw hierarchical-reference debug tap
(reads `dbg_state_o`, packed `{store,compute,load}` engine states) kept in
place, unused by default, for whoever continues debugging the open issue
below — it is not part of the `Axi4SlavePins`/`Axi4MasterPins` contract.

## Two real framework bugs found and fixed

Getting rtl3 running under a real gem5 CPU (never done before this) found
two genuine `RTLDmaDevice` bugs, both fixed in
`src/dev/rtl/rtl_dma_device.{hh,cc}` / `RTLDmaDevice.py` /
`src/axi/axi4_master_engine.hh`:

1. **No idle clock gating.** `RTLDmaDevice::tick()` rescheduled itself
   unconditionally every cycle for the entire simulation, regardless of
   whether the device had any work at all. Fixed by porting `gem5_cva6`'s
   `RtlAccelerator`/`AccelInterface::is_idle()` pattern: an optional
   `isIdle()` hook a leaf can override (default `false`, so existing leaves
   are unaffected unless they opt in), gated by a new `idle_gate_cycles`
   param (default 16, mirrors `gem5_cva6`'s default) and hysteresis
   counter.
2. **Double clock-edge — a real deadlock.** `RTLDmaDevice::tick()` called
   both `slaveEngine.tick()` and `masterEngine.tick()`, both toggling the
   one shared Verilated model's clock pin (`Axi4MasterEngine::tick()` has a
   `driveClock` parameter for exactly this shared-model case, but the call
   site never passed `false` — see
   [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)).
   Two rising edges per device cycle double-advanced every registered FSM
   in the DUT, silently skipping any single-cycle handshake window —
   concretely, rtl3's axion-hdl-generated AXI4-Lite register block asserts
   `ARREADY` for exactly one cycle (`state == READ_ADDR`), which the
   doubled edge always skipped, so the control port's read engine got stuck
   in `ArHandshake` forever. This looked exactly like a hang from the gem5
   side (confirmed via a temporary `Axi4SlaveEngine` pin trace before the
   fix, and by the register block's generated FSM source after). Fixed with
   `masterEngine.tick(false)`.

Both were verified with the exact reproduction: without fix 2, the very
first `STATUS` register read after `START` never returns, and the guest
spins forever. With both fixes, MMIO reads/writes complete immediately and
`isIdle()`-driven gating kicks in during the guest's unrelated (non-MMIO)
work, exactly as `gem5_cva6`'s own accelerator does.

## Open issue: the GEMM job itself never signals DONE

With both fixes in place, the guest's `wait_done()` polling loop runs (each
`STATUS` read completes correctly, `BUSY` reads back `1`), but `DONE` never
sets. The `debugEngineState()`/`dbg_state_o` debug tap shows **both**
operand-load FSMs (`flexnngine2_rtl3_load_engine`'s `a_state`/`b_state`)
parked in `LS_WAIT` indefinitely, even though `Axi4MasterEngine` reports
both 8-beat AR/R bursts (A tile id=0, B tile id=2) fully delivered (`RLAST`
seen, `beatsSent == totalBeats`). Something between the AXI4 R channel and
the AXI-stream `taxi_axi_dma_rd` hand-off to `load_engine` isn't registering
completion under gem5's real DMA timing, despite the *same* DUT passing
`accum_chain_gemm` cleanly in `cocotb/` (which drives the master port with
`cocotbext-axi`'s `AxiRam` instead of `Axi4MasterEngine`) — this looks like
a timing-sensitive interaction specific to `Axi4MasterEngine`'s delivery
pattern (it presents each outstanding read's 8 beats as one uninterrupted
burst rather than interleaving between the two concurrently-outstanding
IDs; `taxi_axi_crossbar_rd`/`taxi_axi_dma_rd`'s expectations around that
haven't been root-caused). Not yet investigated with a waveform
(`--trace-fst` is already wired into both Makefiles).
