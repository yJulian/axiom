---
type: architecture-component
title: AXI4 Bridge Engines
description: How Axi4SlaveEngine and Axi4MasterEngine translate gem5 PacketPtrs into pin-level AXI4 bursts against Verilator RTL, and how VerilatedRtlModel wraps the underlying Verilated model.
tags: [axi4, verilator, bridge-engine, out-of-order, rtl-cosimulation]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-6c29950b236a167d2b44f6bf
    resource: repo://src/axi/axi4_master_engine.cc
  - id: openwiki-source-4b63d01526c692ebaece058f
    resource: repo://src/axi/axi4_master_engine.hh
  - id: openwiki-source-a3aafa3b6e6c027268e7f4ca
    resource: repo://src/axi/axi4_master_engine.test.cc
  - id: openwiki-source-49308b329d87e0cb117c6b65
    resource: repo://src/axi/axi4_slave_engine.cc
  - id: openwiki-source-d3c51e18b9137b1f0f359b64
    resource: repo://src/axi/axi4_slave_engine.hh
  - id: openwiki-source-1740306ca7cdbbefa703c011
    resource: repo://src/axi/verilated_model.hh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# AXI4 Bridge Engines

AXION's C++/RTL boundary is crossed by two composed "engine" classes in
`src/axi/`, plus a thin RAII wrapper around the Verilated model itself. Every
leaf device (`RTLPioDevice`, `RTLDmaDevice`, `RTLPciDevice`, `RTLBaseCpu`, and
their plugin-path counterparts) owns one or both engines instead of
implementing AXI4 timing itself — a leaf only has to implement the pin
accessor contracts in `src/axi/axi4_types.hh` (`Axi4SlavePins` /
`Axi4MasterPins`, plus the shared `Axi4ClockPins` clock/reset/eval methods
both inherit virtually so a class implementing both contracts on the same
underlying model, like `RTLDmaDevice`, gets exactly one `axiSetClk`/`axiEval`
rather than an ambiguous pair).

## Two engines, two roles

- **`Axi4SlaveEngine`** (`src/axi/axi4_slave_engine.{hh,cc}`) drives an
  `Axi4SlavePins` port from gem5 `PacketPtr`s — gem5 (or another RTL master)
  is the sole requester on this port, so there is nothing to reorder. It runs
  one full multi-beat AW/W/B write or AR/R read burst to completion before
  accepting the next transaction. Used by `RTLPioDevice`'s only port and by
  `RTLDmaDevice`'s control/status register port.
- **`Axi4MasterEngine`** (`src/axi/axi4_master_engine.{hh,cc}`) services an
  `Axi4MasterPins` port that the DUT itself drives as requester — the engine
  plays the role gem5's memory system occupies. This is the one place genuine
  AXI4 ID-based out-of-order semantics are implemented. Used by
  `RTLDmaDevice`'s DMA port and `RTLBaseCpu`'s inst/data ports.

Both engines follow the same per-cycle shape: drive combinational inputs for
the current state, call `axiEval()`, sample which handshakes completed, then
toggle `clk` 0→1→0 (the RTL's registers actually commit on that rising edge),
then advance state. `Axi4SlaveEngine::tick()` and `Axi4MasterEngine::tick()`
both hard-code this pattern internally.

## `Axi4SlaveEngine`: one transaction at a time

`issue(PacketPtr pkt, onDone)` (or the `issue(pkt, axiAddr, onDone)` overload
that drives an explicit address instead of the packet's own — used by
`RTLPciDevice` to hand the DUT a BAR-relative offset) queues a packet and
returns `false` if a transaction is already in flight, so the caller must hold
the packet and retry. Internally the engine is a state machine
(`Idle → AwHandshake → WBeats → BHandshake` for writes, `Idle → ArHandshake →
RBeats` for reads) driven one cycle at a time by `tick()`.

Beats are fixed at 8 bytes; `totalBeats_` is derived from `pkt->getSize()`.
Write beats copy `beatBytes_`-sized (or smaller, on the final partial beat)
slices of the packet's data onto `WDATA`, with `WSTRB` set to `0xff` for a
full beat or a partial mask (`(1u << n) - 1`) for the last one. Read beats
copy `RDATA` back into the packet at the corresponding offset. AWLOCK/AWCACHE/
AWPROT/AWQOS (and the AR equivalents) are derived from the gem5 `Request`'s
own flags (`isLockedRMW()`, `isUncacheable()`, `isPriv()`/`isSecure()`/
`isInstFetch()`, `qosValue()`); AWREGION/ARREGION are always driven 0 since
gem5 has no equivalent concept.

`BRESP`/`RRESP` are propagated into the gem5 packet: `pkt->setBadCommand()` on
`SlvErr`, `pkt->setBadAddress()` on `DecErr`. For a multi-beat read, the
engine tracks `worstResp_` — the highest-severity RRESP seen across all
beats (`AxiResp`'s enumerators are ordered by severity: `Okay < ExOkay <
SlvErr < DecErr`) — and applies only that worst response once the last beat
completes. The transaction's completion callback (`onDone_`) is deferred to
`pendingDone_` and invoked only *after* the rising-edge toggle inside
`tick()`, so it never runs from inside a combinational `axiEval()`.

## `Axi4MasterEngine`: same-ID-in-order, cross-ID-out-of-order

Modeled directly on `gem5_cva6`'s `src/accel/dma_master_engine.cc`, this
engine is the one place real AXI4 ID-based out-of-order completion semantics
are implemented: **transactions sharing an AXI ID complete in issue order;
transactions with different IDs may complete in any order.**

It reaches gem5's memory system through a small `Backend` interface
(`issueRead(seq, addr, size)` / `issueWrite(seq, addr, size, data)`,
completion reported back via `completeRead(seq, data, size)` /
`completeWrite(seq)`), implemented by `RTLDmaDevice` via `dmaRead()`/
`dmaWrite()` and by `RTLBaseCpu` via its `RequestPort`'s `sendTimingReq()`.
Every accepted AR/AW burst is assigned a strictly increasing global sequence
number (`nextSeq_++`) and pushed onto a per-ID issue-order deque
(`readOrder_[id]` / `writeOrder_[id]`) — only the *front* of each ID's deque
is eligible to complete next.

`pickOldestEligibleRead()`/`Write()` scan every ID's deque, consider only
fronts whose data/response is actually ready (`dataReady` / `respReady`, set
by `completeRead`/`completeWrite`), and select the smallest sequence number
among those — i.e., the globally oldest transaction that is also the oldest
outstanding for its own ID. `driveR()`/`driveB()` use this to decide what to
present next on the R/B channels once the previously in-flight transaction's
last beat has handshaken (`rInFlightSeq_`/`bInFlightSeq_` track "currently
presenting"); a multi-beat read streams `beatsSent` beats one per cycle with
`RLAST` asserted only on the final beat (`RLAST = beatsSent == totalBeats-1`).

Address-channel acceptance is always-ready at the pin level
(`axiMasterSetArReady(1)`/`AwReady(1)`/`WReady(1)` every cycle) — backpressure
toward the DUT is not modeled there; real memory-system latency shows up
later, on R/B, via the `pickOldestEligible*` gating. Only `INCR` bursts are
supported for multi-beat transactions: `sampleAr()`/`sampleAwAndW()`
`panic_if` on `FIXED`/`WRAP` when `len > 0`, because `Backend::issueRead`/
`issueWrite` model one linear memory access per burst rather than per-beat
addressing (a single-beat burst, e.g. an AXI4 exclusive LR/SC, is address-
computation-identical under any burst type, so it's allowed through). AxLOCK/
AxCACHE/AxPROT/AxQOS/AxREGION are sampled off the address channel into an
`AxAttrs` struct for pin-accuracy but are currently captured-and-unused
downstream, the same status as AWID/ARID before any reordering logic needed
them.

`tick(bool driveClock = true)` exists because some leaves feed multiple
engines from a single underlying Verilated model — `RTLBaseCpu`'s inst and
data ports share one RTL core. Only one engine may toggle the shared model's
actual `clk` pin per gem5 cycle; the other calls `tick(false)` so its own
combinational sample/drive still runs every cycle but it skips the
rising-edge toggle. Getting this wrong is a real, previously-hit bug — see
[flexnngine2_rtl3_accel](/openwiki/examples/flexnngine2-rtl3-accel.md)'s
double-clock-edge deadlock, caused by a call site that toggled the shared
clock from both `slaveEngine.tick()` and `masterEngine.tick()` on the same
device.

`idle()` reports whether the engine has no outstanding or undrained read/write
at all — used by `RTLDmaDevice`'s optional idle clock gating, though it is
only one input to that decision since the DUT itself can be busy computing
with zero AXI traffic in flight.

## `VerilatedRtlModel<TopT>`

`src/axi/verilated_model.hh` is a thin RAII wrapper: owns a `VerilatedContext`
and a `TopT` instance (the Verilator-generated top module class, `#include`d
directly by the leaf — no `dlopen` indirection on this path, deliberately
unlike `gem5_cva6`'s `AccelInterface`, since swapping RTL backends at runtime
isn't a goal here), and exposes `clockEdge()`/`settle()` (both just call
`top_->eval()` — the naming records *why* an eval is happening, not a
behavioral difference) plus `openTrace()`/`dumpTrace()`/`closeTrace()` FST
trace hooks gated on `VM_TRACE`. `~VerilatedRtlModel()` calls `top_->final()`
and closes any open trace. The engines above do not use this wrapper
directly — they operate purely against the `Axi4SlavePins`/`Axi4MasterPins`
interfaces a leaf class implements, typically by forwarding each pin method
onto a `VerilatedRtlModel<Vxxx_top>` member's `top()`.

## Test coverage

`src/axi/axi4_master_engine.test.cc` and `axi4_slave_engine.test.cc` are gtest
suites covering both engines directly against a mock `Axi4MasterPins`/
`Axi4SlavePins` implementation (no real Verilator model). Notably
`CrossIdReadsCompleteOutOfOrderSameIdStaysInOrder` and
`CrossIdWritesCompleteOutOfOrderSameIdStaysInOrder` are the only tests in the
repository — mocked, not RTL-backed — that exercise literal out-of-order
completion across IDs; `MultiBeatReadStreamsRlastOnlyOnFinalBeat`,
`FixedArBurstThrows`, and `WrapAwBurstThrows` cover the other behaviors
described above.
