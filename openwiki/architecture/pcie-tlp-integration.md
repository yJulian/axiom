---
type: architecture-component
title: "PCIe Path: TLP-Level Endpoint"
description: The deeper PCIe path where RTL parses genuine transaction-layer packets on four AXI4-Stream channels itself, via PcieTlpCompleterEngine/PcieTlpRequesterEngine, including posted-write and tagged-completion semantics and the two documented header simplifications.
tags: [pcie, tlp, axi4-stream, gem5, pci-endpoint]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-8ab4ae8c7682fd655db16ba5
    resource: repo://hw/pcie/pcie_tlp_if.sv
  - id: openwiki-source-afa3c24730640f379b52b64a
    resource: repo://hw/pcie/pcie_tlp_pkg.sv
  - id: openwiki-source-74dee2dcb2f6b4c14d4150e5
    resource: repo://src/dev/rtl/rtl_pcie_tlp_device.hh
  - id: openwiki-source-a057d945887719e896a3b8a5
    resource: repo://src/pcie/pcie_tlp_engine.hh
  - id: openwiki-source-ffc4532bbdbad492330f2c82
    resource: repo://src/pcie/pcie_tlp_types.hh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# PCIe Path: TLP-Level Endpoint

`RTLPcieTlpDevice` (`src/dev/rtl/rtl_pcie_tlp_device.{hh,cc}`,
`RTLPcieTlpDevice.py`) is the second, deeper PCIe path, layered on top of
[the AXI4-bridge path](/openwiki/architecture/pcie-integration.md) rather
than replacing it. `RTLPciDevice` hands the RTL an ordinary AXI4 slave port
and keeps every trace of PCIe on the gem5 side. `RTLPcieTlpDevice` instead
hands the RTL **genuine transaction-layer packets** on the four AXI4-Stream
channels a hard IP block exposes (CQ/CC/RQ/RC), and the RTL parses and
builds the headers itself: fmt, type, length, requester ID, tag, byte
enables, and 64-bit address on the way in; completer ID, completion status,
byte count, and lower address on the way out.

**Which path to use:** the AXI4-bridge path, unless the PCIe protocol itself
is the thing being modelled. It is what a real FPGA design uses, it reuses
both existing bridge engines, and its DUTs are far simpler. This path exists
to make the protocol visible *to the RTL* — worth it when that is the
subject, not a better way to attach an accelerator.

Structurally `RTLPcieTlpDevice` is near-identical to `RTLPciDevice`: same
deferred-response timing port (`RtlPioPort`), same `RangeForwardPort` stub
for gem5's direct `pioPort.sendRangeChange()` calls, same interrupt-edge
sampling via `rtlGetIrq()`, same idle-clock-gating pattern. Only the engines
differ — deliberately, so the pair of worked examples
([pcie_template_accel](/openwiki/examples/pcie-template-accel.md) and
[pcie_tlp_template_accel](/openwiki/examples/pcie-tlp-template-accel.md))
is a comparison of abstraction levels rather than of two unrelated
implementations. A leaf implements `axion::PcieTlpCompleterPins`,
`axion::PcieTlpRequesterPins`, and `rtlGetIrq()` against a concrete
Verilated top module.

## The two engines (`src/pcie/pcie_tlp_engine.{hh,cc}`)

The TLP-level counterparts to `Axi4SlaveEngine`/`Axi4MasterEngine`, following
the same per-cycle drive/eval/sample/clock-toggle shape (with a
`driveClock` parameter on both `tick()`s for the shared-model case).

- **`PcieTlpCompleterEngine`** turns one gem5 `PacketPtr` at a time into a
  MemRd (no data) or MemWr (data) TLP on CQ, and for reads decodes the CplD
  that comes back on CC. `issue(pkt, tlpAddr, onDone)` takes an explicit
  BAR-relative address (the same idea as `Axi4SlaveEngine`'s address
  overload) so the DUT sees fixed offsets regardless of where enumeration
  mapped the BAR, and returns `false` if a transaction is already in flight.
  A **memory write is posted**: PCIe defines no completion for MemWr, so the
  engine answers the gem5 packet as soon as its last request beat is
  accepted rather than waiting for any response — a real protocol difference
  from the AXI4 path, where every write waits for a B response. Completion
  status (`TlpCplStatus`) decoded off the CC header is held in `cplStatus_`
  rather than applied immediately, because `Packet::setBadAddress()`/
  `setBadCommand()` require the packet to already be a response, and
  `makeResponse()` only happens once the last completion beat lands.
- **`PcieTlpRequesterEngine`** decodes the DUT's own MemRd/MemWr requests on
  RQ, hands them to a `Backend` (the same `issueRead(seq, addr, size)`/
  `issueWrite(seq, addr, size, data)` shape as `Axi4MasterEngine::Backend`,
  implemented via gem5's DMA machinery), and returns CplD completions on RC.
  Reads are tracked per PCIe **tag** rather than AXI ID; completions are
  emitted in whatever order the backend finishes them, which across
  different tags may differ from issue order — exactly what PCIe permits,
  and the same out-of-order property `Axi4MasterEngine` implements per AXI
  ID (see [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)).
  Reusing a tag that is still outstanding is a fatal error rather than
  silently mismatched data. `idle()` reports true only when there are no
  outstanding reads, no posted writes counted, and no queued completions
  (`postedWrites_ == 0 && cplQueue_.empty()`).

## Pin contract: 16 pins vs AXI4's 88

`src/pcie/pcie_tlp_types.hh` defines `PcieTlpCompleterPins`
(`cq*`/`cc*` accessors) and `PcieTlpRequesterPins` (`rq*`/`rc*` accessors),
both inheriting `PcieTlpClockPins` virtually (same reasoning as
`Axi4ClockPins`: a leaf implementing both roles gets exactly one clock/reset/
eval triple). Each stream exposes only `tdata`/`tlast`/`tvalid`/`tready` —
sixteen pins total, against AXI4's 88 — because the four TLP streams carry
addressing, length, and byte-enable information *inside* the packet rather
than on sideband wires. The header field encode/decode helpers in the
`tlp::` namespace (`makeDw0`/`dw0Fmt`/`dw0Type`/`dw0LengthDw`,
`makeReqDw1`/`reqDw1*`, `makeCplDw1`/`cplDw1*`, `makeCplDw2`/`cplDw2Tag`,
`beat`/`beatLowDw`/`beatHighDw`) implement the PCIe base spec's real bit
positions, not a local convention, and are free functions so both engines
and their tests can share them without dragging state along.

## Two documented simplifications (`hw/pcie/pcie_tlp_pkg.sv`)

Both are packing conventions on the local stream, not changes to any header
field — every field the RTL parses and builds is the real thing at the real
bit position:

1. **Requests use the 4-DW (64-bit address) header form only.** AXION's
   engines never emit the 3-DW form and the DUT rejects it, so a request's
   payload always starts on a beat boundary (TLPs travel on a 64-bit stream,
   two DWs per beat).
2. **Completion headers are 3 DW, padded to 4 DW with one reserved DW.** A
   genuine 3-DW completion header would leave its payload straddling beats;
   rather than build a barrel shifter into a template, the extra DW keeps
   completion payload beat-aligned too. A production endpoint must handle
   both header sizes and the misalignment they imply — a template does not.

Separately, `hw/pcie/pcie_tlp_if.sv`'s four streams carry **no TKEEP**: a
hard IP block carries one as a convenience, but here it would be a second
source of truth for something the header's `length` field plus first/last
byte enables already state exactly, and two encodings of one fact can
disagree. Beats are therefore always full `DATA_WIDTH`; a final beat
carrying a single DW simply leaves its upper DW undefined, and the receiver
knows to ignore it from `length`.

## Testing

`src/pcie/pcie_tlp_engine.test.cc` is a gtest suite (run via `scons
build/RISCV/unittests.opt`, target `PcieTlpEngineTest`) covering the
engines' own header encode/decode, posted-write behavior, completion-status
mapping, and out-of-order completion across tags. `make tb-pcie-tlp` is the
cocotb testbench for the full worked example — see
[pcie_tlp_template_accel](/openwiki/examples/pcie-tlp-template-accel.md).
