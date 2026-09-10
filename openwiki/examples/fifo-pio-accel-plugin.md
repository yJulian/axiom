---
type: worked-example
title: "Worked Example: fifo_pio_accel_plugin"
description: The same fifo_pio_accel FIFO DUT built through AXION's opt-in dlopen plugin path instead of direct-link, with a standalone testbench that proves the plugin ABI is genuinely usable with zero compile-time knowledge of the DUT.
tags: [plugin, dlopen, axi4, fifo, worked-example]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-a459c9a477c9837bbc1a853f
    resource: repo://examples/fifo_pio_accel_plugin/configs/run_fifo_pio_plugin.py
  - id: openwiki-source-dd656d4a5ddeacecf8129b86
    resource: repo://examples/fifo_pio_accel_plugin/Makefile
  - id: openwiki-source-6fef3671a0bf5d8d4923f120
    resource: repo://examples/fifo_pio_accel_plugin/tb_fifo_pio_plugin.cc
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: fifo_pio_accel_plugin

`fifo_pio_accel_plugin` is the plugin-path analog of
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md): the *exact same* DUT
(`../fifo_pio_accel/fifo_pio_accel.sv`, wired through
`../fifo_pio_accel/fifo_pio_top.sv`, reused read-only — no second DUT needed
to prove the plugin ABI) built into a `.so` via
[the plugin path](/openwiki/architecture/plugin-abi.md) instead of linked
directly into `gem5.opt`. This directory has no `SConscript` and is never
linked into `gem5.opt` — running `make verilate`/`make gem5` is not a
prerequisite for anything here.

## Building the `.so`

`examples/fifo_pio_accel_plugin/Makefile` sets `TOP_MODULE = fifo_pio_top`,
`PLUGIN_SOURCES` to the two `fifo_pio_accel` `.sv` files, `PLUGIN_HAS_SLAVE
= 1`, `PLUGIN_HAS_MASTER = 0`, and `include`s `../../plugin/rtl_plugin.mk`
— the generic template described in
[Plugin Path (dlopen ABI)](/openwiki/architecture/plugin-abi.md). `make
verilate-plugin` at the repo root (or `make -C
examples/fifo_pio_accel_plugin plugin-so`) produces
`libfifo_pio_top_plugin.so`.

## The standalone testbench (`tb_fifo_pio_plugin.cc`)

The Makefile also builds `tb_fifo_pio_plugin` (`$(CXX) -std=c++17
-I$(REPO_ROOT)/src ... -ldl`), a small C++ program that `dlopen`s the `.so`
built above and drives it **purely through `src/axi/axi4_plugin_abi.h`** —
no `Vfifo_pio_top.h`, no Verilator headers at all, no compile-time knowledge
of the DUT whatsoever. This is the concrete proof that the plugin ABI is
genuinely usable generically, not just in theory: `make -C
examples/fifo_pio_accel_plugin run-tb` (or `make tb-plugin` at the repo
root) builds and runs it with `./tb_fifo_pio_plugin
./libfifo_pio_top_plugin.so`.

It exercises the exact same AXI4 slave protocol and the exact same push/pop
round-trip as `fifo_pio_accel`'s own cocotb test
(`test_fifo_pio.py`), against the exact same DUT:

1. `dlopen()`s the `.so`, resolves every `axion_rtl_*` symbol via `dlsym`
   into an `Abi` struct of function pointers (fatal exit on any missing
   symbol), and checks `abi.abiVersion()` against
   `AXION_RTL_PLUGIN_ABI_VERSION` and `abi.hasSlavePort()`.
2. Creates one `AxionRtlInstance`, holds reset low for 10 cycles via
   `abi.setRstN(inst, 0)` + repeated `tick()`, then releases it.
3. Reads STATUS (offset `0x00`) and asserts `empty=1, full=0`.
4. Writes `0xdeadbeefcafef00d` to `FIFO_DATA` (offset `0x08`) with AWID `5`,
   then reads STATUS again and asserts not-empty, then reads `FIFO_DATA`
   back with ARID `7` and asserts the popped value matches what was pushed,
   then reads STATUS once more and asserts empty again — using distinct
   AWID/ARID values on each transaction specifically to exercise the ID
   field itself (echoed back on B/R), not just the data path.

The per-cycle `tick()` helper matches `Axi4SlaveEngine::tick()`'s own
protocol pattern (drive inputs → `setClk(0)` → `eval` → `setClk(1)` → `eval`
→ `setClk(0)` → `eval` → sample outputs — see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)); the
`axiWrite()`/`axiRead()` helpers poll the relevant `*ready`/`*valid` pin from
the *previous* sample before driving a new handshake, since a ready pin can
legitimately drop the very cycle a request is accepted, and re-polling after
each new edge would make a legitimately-busy-after-accepting slave
indistinguishable from "still hasn't accepted."

## `configs/run_fifo_pio_plugin.py`

The plugin-path analog of `fifo_pio_accel`'s own
`configs/run_fifo_pio.py`: identical `System` wiring (`RiscvTimingSimpleCPU`
+ `SystemXBar`, `FIFO_BASE = 0x10010000`), but the hand-written
`FifoPioAccel` C++ leaf class is replaced entirely by `RTLPioDevicePlugin`
constructed with `rtl_library="examples/fifo_pio_accel_plugin/libfifo_pio_top_plugin.so"`
— no new C++ or Python needed for this DUT at all. Same caveat as the
direct-link config: this is correct integration wiring, but exercising it
via `--binary` needs a compiled RISC-V SE-mode binary, which this
environment cannot produce — see
[Verifying Without gem5 or a RISC-V Toolchain](/openwiki/workflows/testing-without-gem5.md).
`tb_fifo_pio_plugin.cc` above is the genuine, fully-automated verification
of the plugin ABI + AXI4 protocol logic, independent of both gem5 and the
RISC-V toolchain.
