---
type: worked-example
title: "Worked Example: fifo_pio_accel"
description: AXION's simplest complete worked example — an RTLPioDevice leaf wrapping an N-deep FIFO register block, its TOP harness, SE-mode and FS-mode Linux boot configs, and the cocotb testbench proving full multi-beat AXI4 bursts with distinct AWID/ARID.
tags: [pio, axi4, rtl-cosimulation, cocotb, worked-example, fifo]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-a17c2ccf8a906224ae848ec4
    resource: repo://examples/fifo_pio_accel/cocotb/test_fifo_pio.py
  - id: openwiki-source-4d78b98434106cb7ca17d454
    resource: repo://examples/fifo_pio_accel/configs/run_fifo_pio_linux.py
  - id: openwiki-source-2d50113fd50be92f03dc17c9
    resource: repo://examples/fifo_pio_accel/configs/run_fifo_pio.py
  - id: openwiki-source-be63dcd089c9f6bf9c0a0970
    resource: repo://examples/fifo_pio_accel/fifo_pio_accel.sv
  - id: openwiki-source-cf852e4e7a2929f70c2a7172
    resource: repo://examples/fifo_pio_accel/fifo_pio_device.hh
  - id: openwiki-source-856879cace807f5848e31870
    resource: repo://examples/fifo_pio_accel/FifoPioAccel.py
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: fifo_pio_accel

`fifo_pio_accel` is AXION's simplest complete worked example: a
`RTLPioDevice` leaf (see
[AXION Architecture Overview](/openwiki/architecture/overview.md)) wrapping
an N-deep FIFO exposed purely as a PIO/MMIO register interface over full
AXI4. It has no DMA master port — that side is exercised by
[dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md) instead — so
it is also the reference point `fifo_pio_accel_plugin` reuses verbatim as
its DUT for [the plugin path](/openwiki/examples/fifo-pio-accel-plugin.md).

## The DUT (`fifo_pio_accel.sv`)

Adapted from `gem5_cva6`'s `accelerator/fifo_accel.sv` (see
[Prior Art](/openwiki/reference/prior-art.md)), with the AXI4 master/DMA
half dropped and the register interface upgraded from AXI4-Lite to full
AXI4 — still single-beat per burst, since this particular DUT doesn't
itself need multi-beat bursts, though the `axi4_if`/`axi4_pins` plumbing
around it supports them. Register map (8-byte aligned, `DATA_WIDTH`-wide):

| Offset | Register | Access | Meaning |
| --- | --- | --- | --- |
| `0x00` | `STATUS` | RO | bit `[0]`=empty, bit `[1]`=full, bits `[N_BITS+1:2]`=count |
| `0x08` | `FIFO_DATA` | RW | write = push, read = pop |

## `fifo_pio_top.sv`

The TOP module Verilator elaborates for this example: connects the
pre-built AXI4 pin adapter (`hw/axi4/axi4_pins.sv`'s
`axi4_pins_slave_port`) to the DUT over a clean `axi4_if` handle. Its own
module boundary is flat scalar ports (`s_axi_awid`, `s_axi_awaddr`, ...) —
exactly what `FifoPioAccel`'s C++ pin-accessor overrides talk to via the
Verilator-generated `Vfifo_pio_top.h`.

## The leaf class (`FifoPioAccel`)

`fifo_pio_device.hh`/`.cc` define `FifoPioAccel : RTLPioDevice`, implementing
`axion::Axi4SlavePins` directly against `Vfifo_pio_top` — no dlopen/
abstract-interface indirection, the direct-link pattern every pin accessor
forwards one line onto the matching `s_axi_*` field of an
`axion::VerilatedRtlModel<Vfifo_pio_top>` member. `FifoPioAccel.py` sets
`pio_size = 0x1000` and implements `generateDeviceTree()` (device-tree node
tagged `axion,fifo-pio-accel`) for FS-mode boot.

## Running it

Two configs, one SE-mode and one FS-mode:

- `configs/run_fifo_pio.py` builds a minimal SE-mode `System`
  (`RiscvTimingSimpleCPU` + `SystemXBar` + `FifoPioAccel` at
  `FIFO_BASE = 0x10010000`) — correct integration wiring, but running it
  via `--binary` needs a compiled RISC-V SE-mode ELF performing
  loads/stores against the MMIO range, which this environment has no
  cross-toolchain to produce (`mem_ranges` must stay below `FIFO_BASE`
  since `SystemXBar` routes by address range and an overlapping DRAM range
  fails at `m5.instantiate()`).
- `configs/run_fifo_pio_linux.py` boots RISC-V Linux (Ubuntu 24.04) under
  gem5's `RiscvBoard` with `FifoPioAccel` wired in as an extra off-chip PIO
  device, proving it's reachable from genuine Linux userspace
  (`/dev/mem` + `mmap()`) rather than only SE mode's `Process.map()`
  shortcut. No RISC-V KVM CPU model exists in gem5, and KVM can't
  cross-virtualize RISC-V on an x86 host anyway, so this uses a
  `SimpleSwitchableProcessor` to fast-forward boot under `ATOMIC` and
  switches to a real `TIMING` core exactly when the disk image's own
  `after_boot.sh` signals (hypercall 2), so only the actual FIFO/PIO
  interaction runs under detailed timing. It reuses gem5's packaged
  `riscv-ubuntu-24.04-boot` FS workload resource but swaps in a
  caller-supplied disk image with a compiled control program injected onto
  it (see `riscv_test/fifo_pio_linux_ctl.c`).

`make verilate` builds the RTL; `make run-fifo-example ARGS='--binary
<elf>'` builds gem5 and runs the SE-mode config.

## The cocotb testbench

`examples/fifo_pio_accel/cocotb/test_fifo_pio.py` (run via `make tb`) is
the standalone, no-gem5 verification of the DUT's AXI4 protocol logic, via
`cocotbext-axi`'s `AxiMaster` instead of hand-rolled beat-by-beat C++. Its
one test, `push_pop_roundtrip`:

1. Resets the DUT, then reads `STATUS` (ARID `0`) and asserts `empty=1,
   full=0`.
2. Writes `0xDEADBEEFCAFEF00D` to `FIFO_DATA` with AWID `5`.
3. Reads `STATUS` again (ARID `1`) and asserts not-empty.
4. Reads `FIFO_DATA` back (ARID `7`) and asserts the popped value matches
   what was pushed.
5. Reads `STATUS` once more (ARID `2`) and asserts empty again.

Each transaction uses a distinct AWID/ARID specifically to exercise the ID
field itself (echoed back on B/R), not just the data path — proving the
write burst → RTL state → read burst path works end to end. This test
exercises a full multi-beat-capable AW/W/B write burst and AR/R read burst
through `Axi4SlaveEngine`'s protocol, even though this particular DUT only
ever needs one beat per transaction. `tb_fifo_pio_plugin.cc` (see
[fifo_pio_accel_plugin](/openwiki/examples/fifo-pio-accel-plugin.md)) is
the plugin-ABI analog of this exact same test.
