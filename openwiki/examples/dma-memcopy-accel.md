---
type: worked-example
title: "Worked Example: dma_memcopy_accel"
description: The DMA-side worked example — a RTLDmaDevice leaf with a control/status AXI4 slave port and a shared AXI4 master DMA port, three concurrent channels tagged by AXI ID, and the cocotb testbench proving ID-based R/B demux against real RTL.
tags: [dma, axi4, rtl-cosimulation, cocotb, worked-example]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-5861b69d3b5a156bb50383fe
    resource: repo://examples/dma_memcopy_accel/cocotb/test_dma_memcopy.py
  - id: openwiki-source-4ab245c4a0b4c0a3f45d36ca
    resource: repo://examples/dma_memcopy_accel/configs/run_dma_memcopy.py
  - id: openwiki-source-e488ff492b16ac419cc235da
    resource: repo://examples/dma_memcopy_accel/dma_memcopy_device.cc
  - id: openwiki-source-15b507b213b90b34a9f4a8a5
    resource: repo://examples/dma_memcopy_accel/dma_memcopy_device.hh
  - id: openwiki-source-e83e5f2aace17fea49729abb
    resource: repo://examples/dma_memcopy_accel/dma_memcopy.sv
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: dma_memcopy_accel

`dma_memcopy_accel` is AXION's DMA-side worked example — a `RTLDmaDevice`
leaf, as opposed to
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md)'s pure `RTLPioDevice`.
It is the one worked example in the repository that exercises
`Axi4MasterEngine`'s AXI-ID-based completion logic end to end against real
RTL (`fifo_pio_accel` has no DMA master port at all) — see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md) for the
engine itself.

## The DUT (`dma_memcopy.sv`)

`dma_memcopy` implements `NUM_CH` (default 4, `3` in the cocotb test)
independent copy channels behind a shared control/status AXI4 **slave**
port and a shared AXI4 **master** (DMA) port. Each channel's per-register
control/status map sits on a `0x20`-byte stride starting at `0x00`:

| Offset | Register | Access | Meaning |
| --- | --- | --- | --- |
| `+0x00` | `SRC` | RW | source address |
| `+0x08` | `DST` | RW | destination address |
| `+0x10` | `CTRL` | WO | write bit `0`=1 to start (ignored while busy) |
| `+0x18` | `STATUS` | RO | bit `0`=busy, bit `1`=done (reading clears done) |

Each channel performs one single-AXI4-beat (`DATA_WIDTH` bytes) src→dst copy
over the shared master port, **using its own channel index as its AXI ID**
(AWID/ARID). With more than one channel started concurrently, the DUT
genuinely has multiple outstanding read/write transactions in flight at
once, tagged by distinct IDs — this is what lets `Axi4MasterEngine`'s
per-ID-in-order/cross-ID-out-of-order completion logic actually get
exercised against real RTL rather than a mock.

AR/AW issuance among channels wanting to start a burst is arbitrated
fixed-priority (lowest channel index wins), one grant per cycle — that only
serializes *issuing* a burst; multiple channels can still have bursts
outstanding simultaneously (each waiting on its own R/B response), which is
the actual property under test. R/B responses are demultiplexed back to the
owning channel purely by ID, since a channel's AXI ID always equals its own
channel index.

## The leaf class (`DmaMemcopyAccel`)

`dma_memcopy_device.hh`/`.cc` define `DmaMemcopyAccel : RTLDmaDevice`,
implementing both `axion::Axi4SlavePins` (control/status port) and
`axion::Axi4MasterPins` (DMA port) directly against the
Verilator-generated `Vdma_memcopy_top`, with no dlopen/abstract-interface
indirection — the direct-link pattern (see
[AXION Architecture Overview](/openwiki/architecture/overview.md)). Every
`axiSlaveSet*`/`Get*` and `axiMasterSet*`/`Get*` override is a one-line
forward onto the corresponding `s_axi_*`/`m_axi_*` field of
`rtl_.top()` (an `axion::VerilatedRtlModel<Vdma_memcopy_top>` member);
`axiSetClk`/`axiSetRstN` set `clk_i`/`rst_ni` directly, and `axiEval()`
calls `rtl_.settle()`.

`DmaMemcopyAccel.py` (`examples/dma_memcopy_accel/DmaMemcopyAccel.py`) sets
`pio_size = 0x1000` and implements `generateDeviceTree()` (device-tree node
tagged `axion,dma-memcopy-accel`) for the FS-mode boot path.

## `dma_memcopy_top.sv`

The TOP harness wires an `axi4_pins_slave_port` (control/status) and an
`axi4_pins_master_port` (DMA) around the `dma_memcopy` DUT, the same pattern
`fifo_pio_top.sv` uses for its single slave port — see
[Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md)
for the shared `hw/axi4/` adapters both use.

## Running it

`examples/dma_memcopy_accel/configs/run_dma_memcopy.py` builds a minimal SE
(syscall-emulation)-mode `System` (`RiscvTimingSimpleCPU` + `SystemXBar` +
`DmaMemcopyAccel`) with `DMA_BASE = 0x10010000` and `mem_ranges` kept below
that base (the `SystemXBar` routes by address range, and an overlapping DRAM
range fails at `m5.instantiate()` — the same constraint
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md)'s config has).
Actually exercising it via `--binary` needs a compiled RISC-V SE-mode
binary driving the control/status registers, which this environment cannot
produce — see
[Verifying Without gem5 or a RISC-V Toolchain](/openwiki/workflows/testing-without-gem5.md).

`make -C examples/dma_memcopy_accel` (or `make verilate-dma` at the repo
root) verilates the RTL; `make run-dma-example ARGS='--binary <elf>'`
builds gem5 and runs it.

## The cocotb testbench

`examples/dma_memcopy_accel/cocotb/test_dma_memcopy.py` (run via `make
tb-dma`) is the standalone, no-gem5 verification: it drives the
control/status AXI4 slave port with `cocotbext-axi`'s `AxiMaster` — the same
approach `test_fifo_pio.py` uses — and plays gem5's role on the DMA AXI4
master port with an `AxiRam` memory model instead of hand-rolled
pending-transaction bookkeeping. `AxiRam` services `NUM_CH = 3` channels'
concurrent outstanding read/write bursts itself, each tagged by its own
channel's AXI ID, which is what actually exercises the RTL's ID-based R/B-
to-channel demux.

**Caveat, documented in the top-level `CLAUDE.md`:** `AxiRam` services
requests in arrival order, so this cocotb test proves
concurrent-outstanding-by-ID correctness against real RTL, not literal
out-of-order completion — that half (a same-ID-in-order,
cross-ID-out-of-order response arriving in a *different* order than issued)
is covered only by the mocked `CrossId*CompleteOutOfOrder*` gtests in
`src/axi/axi4_master_engine.test.cc` (see
[AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md)).
