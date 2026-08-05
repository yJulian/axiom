# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**AXION** is a gem5 extension that bridges gem5's event-driven `SimObject`s to RTL
blocks simulated by Verilator, over a genuine, pin-level AXI4 (5 channels, ID-tagged,
same-ID-in-order / cross-ID-out-of-order). Three C++ classes give a real,
API-accurate inheritance hierarchy onto gem5's own class tree:

- `RTLBaseCpu : BaseCPU` (`src/cpu/rtl/`)
- `RTLPioDevice : PioDevice` (`src/dev/rtl/`)
- `RTLDmaDevice : DmaDevice` (`src/dev/rtl/`) -- gem5's own `DmaDevice` already
  extends `PioDevice`, so `RTLDmaDevice` is transitively a `PioDevice` too; no
  multiple inheritance from `RTLPioDevice` is needed or used.

All three are abstract: they define the AXI4 pin-accessor contract
(`axion::Axi4SlavePins` / `axion::Axi4MasterPins`, `src/axi/axi4_types.hh`) and the
protocol/timing engines that drive it (`Axi4SlaveEngine`, `Axi4MasterEngine`,
`src/axi/`), but a concrete leaf `SimObject` must implement the pin accessors against
a specific Verilator-generated top module (see `examples/fifo_pio_accel/` for the one
worked example). RTL is linked directly (no `dlopen`/abstract-backend indirection) --
that's a deliberate simplification versus prior art on this machine, not an oversight.

`hw/axi4/` is the reusable SystemVerilog side: `axi4_pkg.sv` (typedefs), `axi4_if.sv`
(the actual 5-channel interface DUTs connect to), `axi4_pins.sv` (flat-port adapters --
the only form Verilator's generated C++ model exposes to code -- in both roles:
`axi4_pins_slave_port` for a DUT-as-slave PIO/register port, `axi4_pins_master_port`
for a DUT-as-master DMA port). A `TOP` module (e.g.
`examples/fifo_pio_accel/fifo_pio_top.sv`) wires one or more `axi4_pins` instances to
the **DUT** (Design Under Test) -- see Terminology below.

## Terminology

**DUT** (Design Under Test): the actual SystemVerilog design being simulated (e.g.
`fifo_pio_accel.sv`), as opposed to the surrounding `TOP` harness that connects it to
`axi4_pins`. Chosen over the user's original "SUD" shorthand as the standard
verification-industry term -- use it consistently in code, comments, and file/module
naming (`*_top.sv` for harnesses, plain descriptive names for DUTs).

## Source layout -- read before editing anything under `ext/gem5/src/{axi,cpu/rtl,dev/rtl,examples}`

`ext/gem5` is a git submodule (pinned, shallow). AXION's own C++ sources live at the
repo root under `src/` (and the worked example under `examples/`) -- **never** edit
anything under `ext/gem5/src/axi/`, `ext/gem5/src/cpu/rtl/`, `ext/gem5/src/dev/rtl/`,
or `ext/gem5/src/examples/` directly. Those are mirrors created by
`scripts/build_gem5.sh` (`cp -rs`: real directories, individual files symlinked back to
the true source) so gem5's own build can see them -- gem5's `src/SConscript` walks
with `followlinks=False`, so a plain symlinked directory would be invisible to the
build. Each of our subdirectories mirrors at the *same relative path* it would occupy
inside gem5's own tree (`src/axi` -> `ext/gem5/src/axi`, `src/cpu/rtl` ->
`ext/gem5/src/cpu/rtl`, ...) -- this matters because our own headers use gem5-style
`#include "axi/verilated_model.hh"` paths resolved against gem5's `src/` as the include
root; none of these four paths exist in stock gem5, so there's no collision. Edits
under a mirror are overwritten (or simply invisible to git) the next time it re-runs.

## Build & run commands

```bash
scripts/setup_env.sh          # git submodule update --init ext/gem5 + first mirror pass
make verilate                 # build examples/fifo_pio_accel's RTL (Verilator -> obj_dir/*.a)
make gem5                     # re-mirror (now including the verilated .a) + scons build
                               #   -> build/RISCV/gem5.opt at the repo root (scons -C
                               #   ext/gem5 places build/ at the invocation dir, not
                               #   inside the submodule -- same convention gem5_cva6 uses)
make tb                       # standalone AXI4 testbench, no gem5 needed (see below)
make run-fifo-example ARGS='--binary <path/to/riscv/elf>'
make clean                    # remove RTL build artifacts + the src mirrors
```

Order matters the first time: `verilate` before `gem5` -- the mirror step needs
`examples/fifo_pio_accel/obj_dir/*.a` to already exist to pick it up. `make gem5`
re-runs the mirror itself, so once verilated, `make gem5` alone is always safe.

### Verifying without gem5 or a RISC-V toolchain

`examples/fifo_pio_accel/configs/run_fifo_pio.py` is correct integration wiring (a
`RiscvTimingSimpleCPU` + `SystemXBar` + `FifoPioAccel` `System`), but actually running
it needs a compiled RISC-V SE-mode binary that performs loads/stores against the
accelerator's MMIO range -- this environment has no RISC-V cross-toolchain to produce
one. For genuine, fully-automated verification of the RTL + AXI4 protocol logic itself,
independent of gem5, use `make tb`: `examples/fifo_pio_accel/tb_fifo_pio.cc` drives
`fifo_pio_top`'s AXI4 slave pins directly (full AW/W/B write burst, AR/R read burst,
including distinct AWID/ARID values to exercise the ID field), pushes a value into the
FIFO and pops it back, and asserts it round-trips.

## Architecture

### The two composed bridge engines (`src/axi/`)

Shared behavior lives in composed (has-a) helper classes, not a fourth inheritance
layer -- `RTLPioDevice`/`RTLDmaDevice`/`RTLBaseCpu` sit in three already-related-or-unrelated
gem5 base classes, so a shared *base* would either duplicate `PioDevice` (multiple
inheritance through `RTLDmaDevice` and `RTLPioDevice` both) or make no sense for
`RTLBaseCpu` at all.

- **`Axi4SlaveEngine`**: drives a slave pin set from gem5 `PacketPtr`s, one AXI4 burst
  at a time (full multi-beat AW/W/B and AR/R, honoring `awlen`/`arlen`). There is
  exactly one requester on a slave port (gem5 itself), so there's nothing to reorder --
  it simply tags the single in-flight transaction with a fixed ID and echoes it back.
  Used by `RTLPioDevice` (its only port) and `RTLDmaDevice` (its control/status
  register port).
- **`Axi4MasterEngine`**: the one place genuine AXI4 ID-based out-of-order semantics
  are implemented. Tracks outstanding transactions in per-ID queues plus a global
  issue-order sequence number; `pickOldestEligibleRead()`/`Write()` select, among
  ready completions, the one with the smallest sequence number **that is also the
  oldest for its ID** -- same-ID transactions complete in issue order, different IDs
  may complete in any order. Modeled directly on `gem5_cva6`'s
  `src/accel/dma_master_engine.cc` (`pickOldestEligible()`), found on this machine as
  the one piece of real, working full-AXI4-with-IDs logic anywhere -- see that
  project's `CLAUDE.md`/source if this logic ever needs revisiting. Used by
  `RTLDmaDevice`'s DMA port and `RTLBaseCpu`'s inst/data ports.
- **`VerilatedRtlModel<TopT>`**: thin RAII wrapper (`VerilatedContext` +
  `clockEdge()`/`settle()`/trace hooks). No `dlopen` indirection -- a leaf class
  `#include`s its `Vxxx_top.h` directly and owns one of these.

### Per-cycle protocol pattern

Both engines follow the same shape every `tick()`: drive combinational inputs for the
current state -> `axiEval()` -> sample which handshakes completed -> toggle
`clk` 0->1->0 (the RTL's registers actually commit on that rising edge) -> advance
state. `Axi4MasterEngine::tick()` takes a `driveClock` parameter for the case where
multiple engines share one underlying Verilated model (e.g. `RTLBaseCpu`'s inst/data
ports both fed by a single RTL core) -- only one of them should toggle the shared
model's actual clock pin per gem5 cycle.

### RTLBaseCpu scope note

`RTLBaseCpu` ships as a structurally complete, compiling abstract base (tick loop
modeled on `BaseKvmCPU` rather than `AtomicSimpleCPU`, since execution happens inside
the RTL, not gem5) but has **no worked example core** -- there's no CPU-core-shaped RTL
to hand in this pass, only the FIFO/PIO accelerator. A concrete leaf class wrapping a
real core also owns `ThreadContext`/ISA/interrupt wiring, exactly like any other
`BaseCPU` subclass (see `RTLBaseCpu.py`'s docstring) -- gem5's checkpointing and
interrupt-injection machinery needs a real `ThreadContext`, and only a concrete core
implementation can meaningfully provide one, since the RTL core holds the actual
PC/register state, not gem5.

## Prior art on this machine (context, not dependencies)

Two sibling projects solve adjacent problems and were mined for patterns while
designing AXION -- worth checking if something here seems to be reinventing a wheel:

- `~/gem5_cva6`: mature, working gem5<->Verilator<->AXI bridge for the CVA6 RISC-V
  core, using a `dlopen`'d abstract-interface pattern (`AccelInterface`,
  `RtlCoreInterface`) instead of AXION's direct-link pin contracts. Its
  `RtlAccelerator` (a `DmaDevice`) and `dma_master_engine.cc` are the direct models for
  `RTLDmaDevice` and `Axi4MasterEngine` respectively.
- `~/Development/gem5-verilator-ghdl`: earlier, less mature prototype. Its
  "flatten AXI inside the RTL, cross the C++ boundary with scalars" design is
  explicitly what AXION does *not* do (loses ID/OoO semantics) -- but its build-system
  gotchas (the `cp -rs` mirroring trick this repo also uses, the
  `verilated_fst_c.cpp`-must-be-compiled-in-not-`--whole-archive`d FST link fix) are
  real and worth remembering if the RTL build ever breaks in a similar way.
