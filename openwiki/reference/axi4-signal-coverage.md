---
type: reference
title: AXI4 Signal Coverage
description: Tracks the full pin-level AXI4 signal set AXION's bridge is expected to generate correctly — what is audited and complete, what remains for RTLBaseCpu, and the two documented-not-fixed gaps in Axi4MasterEngine.
tags: [axi4, signal-coverage, bresp, rresp, burst-types]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-94289f382f46c728dc6f01ec
    resource: repo://AXI4_signals.md
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-6c29950b236a167d2b44f6bf
    resource: repo://src/axi/axi4_master_engine.cc
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# AXI4 Signal Coverage

`AXI4_signals.md` (repo root) tracks the full pin-level AXI4 signal set
AXION's bridge is expected to generate correctly — the base reference is a
34-row table of every AW/W/B/AR/R channel signal (ID, ADDR, LEN, SIZE,
BURST, LOCK, CACHE, PROT for the address channels; DATA/STRB/LAST for
write data; RESP/VALID/READY handshakes throughout), each annotated with
its source (master or slave) and direction.

## Audited and complete

`RTLPioDevice`/`RTLDmaDevice` (and their plugin-path counterparts,
`RTLPioDevicePlugin`/`RTLDmaDevicePlugin`) have been audited and brought to
completion:

- **AWLOCK/AWCACHE/AWPROT/AWQOS/AWREGION** (and their AR equivalents) are
  driven end-to-end — the SV interface, the pins adapter
  (`hw/axi4/axi4_pins.sv`), the C++ pin contract
  (`axion::Axi4SlavePins`/`Axi4MasterPins`, `src/axi/axi4_types.hh`), both
  engines, the `fifo_pio_accel` example, and the plugin ABI
  (`src/axi/axi4_plugin_abi.h`) all carry these fields.
- **`Axi4SlaveEngine` propagates real BRESP/RRESP error codes** from the
  RTL into the gem5 packet instead of silently dropping them —
  `pkt->setBadCommand()` on `SlvErr`, `pkt->setBadAddress()` on `DecErr`
  (see
  [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)),
  including tracking the worst RRESP across a multi-beat read's individual
  beats before applying it.
- **`Axi4MasterEngine` streams true multi-beat read completions**: `RLAST`
  is asserted only on the final beat of a burst, not on every beat, and it
  rejects non-INCR bursts loudly (`panic_if`) instead of silently
  mis-executing them.

## Not yet covered: `RTLBaseCpu`

`RTLBaseCpu` (`src/cpu/rtl/`) has **not** been covered by this audit pass
and is an explicit follow-up. Its master/slave port roles are reversed
relative to `RTLDmaDevice`'s DMA port: on `RTLBaseCpu` the RTL core is the
AXI4 master on both its inst and data ports (see
[RTL Device Base Classes](/openwiki/reference/rtl-device-classes.md)), so
the same signal-completeness work needs to be redone against that role
split rather than assumed to carry over from `RTLDmaDevice`'s audit.

## Two documented, deliberately-not-fixed gaps

Both gaps trace to the same root cause: gem5's `DmaDevice::dmaRead()`/
`dmaWrite()` convenience API only takes a completion `Event*` and never
exposes packet/error status back to the caller, so `Axi4MasterEngine`
(which reaches gem5's memory system through exactly that API via its
`Backend` interface — see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)) has
nowhere to route real status even where it would want to:

- **`Axi4MasterEngine` always drives BRESP/RRESP as `OKAY`** — there is
  currently no path for a real gem5 memory-system error to reach the DMA
  master port's response, short of bypassing `dmaRead`/`dmaWrite` for raw
  packet-level DMA. `driveR()`/`driveB()` in `axi4_master_engine.cc`
  hard-code `AxiResp::Okay` on every completion.
- **`Axi4MasterEngine` `panic`s on FIXED/WRAP bursts rather than executing
  them** — `Backend::issueRead`/`issueWrite` model one linear memory
  access per burst (exactly INCR addressing); real FIXED/WRAP support
  needs per-beat addressing, the same category of rework as the point
  above. `sampleAr()`/`sampleAwAndW()` `panic_if` on this case for any
  burst with more than one beat (a single-beat burst is
  address-computation-identical under any burst type, so it is allowed
  through regardless).

Both gaps are architectural consequences of the `DmaDevice` convenience API
AXION builds on, not oversights — fixing either means bypassing that API
for raw packet-level DMA, which is a larger change than the signal-coverage
audit itself.
