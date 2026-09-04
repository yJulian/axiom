# flexnngine2_rtl3_accel

`rtl3` is the scratchpad-fed FleXNNgine2 DMA GEMM accelerator from the
sibling `gem5_cva6` repo (`ext/flexnngine2_adapter/rtl3` there) — a two-tile
(A/B) systolic-array GEMM engine with three distinct AXI IDs on one
physical `m_axi` master port and a chained-ACCUM mode that pins partial
sums in the PEs across jobs instead of round-tripping them through memory.
See that repo's `ext/flexnngine2_adapter/rtl3/README.md` for the full
architecture. Its own sources are **not** vendored into this repo — every
Makefile/SConscript here pulls them from a sibling `gem5_cva6` checkout
(`GEM5_CVA6` variable, default `/home/julian/gem5_cva6`).

## Two ways to exercise it here

**`cocotb/`** drives `flexnngine2_rtl3_top.sv` directly as `TOPLEVEL`, no
AXION wrapper involved — proves the RTL itself is correct under AXION's
Verilator + cocotb + cocotbext-axi stack, independent of gem5. Both tests
pass (`single_tile_gemm`, `accum_chain_gemm`, the latter covering rtl3's
ACCUM-chain feature). Run via `make tb-rtl3` at the repo root.

**A real gem5 `RTLDmaDevice` leaf** (`Flexnngine2Rtl3Accel`, this
directory's `.sv`/`.hh`/`.cc`/`.py`) — the direct-link pattern, same as
`dma_memcopy_accel`. `flexnngine2_rtl3_axion_top.sv` wraps the DUT with a
full-AXI4 pin set (padding the missing `lock/cache/prot/qos/region` fields
on both ports, and a small ID-latch for the control port, which is genuine
AXI4-Lite — see that file's header for the exact field-by-field mapping).
`configs/run_flexnngine2_rtl3.py` builds a full-system, bare-metal
(`RiscvBareMetal`, not SE mode) `RiscvTimingSimpleCPU` system and reuses
**gem5_cva6's own precompiled test ELF**
(`scratch/flexnngine2_rtl3/flexnngine2_rtl3_test.elf`) unmodified — full-system
because that ELF signals completion via a `tohost` write, not an OS
syscall, so SE mode (what `run_fifo_pio.py`/`run_dma_memcopy.py` use) isn't
the right fit here; this sidesteps AXION's "no RISC-V toolchain in this
sandbox" limitation entirely by borrowing an already-built binary.

```bash
make -C examples/flexnngine2_rtl3_accel        # verilate the wrapper
scripts/build_gem5.sh && scons -C ext/gem5 build/RISCV/gem5.opt -j$(nproc)
./build/RISCV/gem5.opt examples/flexnngine2_rtl3_accel/configs/run_flexnngine2_rtl3.py
```

## Two real bugs found in AXION's `RTLDmaDevice` along the way

Nobody had run `RTLDmaDevice` against a real gem5 CPU before this (per
`dma_memcopy_accel`'s own docstrings — `run_dma_memcopy.py` was always
`cocotb`-tested only, never actually simulated end-to-end, for the same
toolchain reason as above). Getting rtl3 running here found two genuine
framework bugs, both now fixed in `src/dev/rtl/rtl_dma_device.{hh,cc}` /
`RTLDmaDevice.py` / `src/axi/axi4_master_engine.hh`:

1. **No idle clock gating** — `RTLDmaDevice::tick()` rescheduled itself
   unconditionally every cycle, for the entire simulation, regardless of
   whether the device had any work at all. Fixed by porting gem5_cva6's
   `RtlAccelerator`/`AccelInterface::is_idle()` pattern: an optional
   `isIdle()` hook a leaf can override (default `false`, so existing
   leaves are unaffected unless they opt in), gated by a new
   `idle_gate_cycles` param (default 16, mirrors gem5_cva6's default) and
   hysteresis counter. `Flexnngine2Rtl3Accel::isIdle()` reads the DUT's own
   `dma_busy_o` pin (now exposed on the wrapper) — the *DUT's* busy state,
   not just "no gem5-side AXI traffic in flight", because the DUT can be
   busy computing autonomously with zero AXI activity on either port (e.g.
   mid-GEMM, between the operand reads finishing and the result write
   starting) — gating on gem5-side idleness alone would freeze the clock
   right there and nothing would ever un-gate it again.
2. **Double clock-edge, a real deadlock** — `RTLDmaDevice::tick()` called
   `slaveEngine.tick()` *and* `masterEngine.tick()`, both toggling the one
   shared Verilated model's clock pin (`Axi4MasterEngine::tick()` has a
   `driveClock` parameter for exactly this shared-model case, but the call
   site never passed `false`). Two rising edges per device cycle
   double-advanced every registered FSM in the DUT, silently skipping any
   single-cycle handshake window — concretely, rtl3's axion-hdl-generated
   AXI4-Lite register block asserts `ARREADY` for exactly one cycle
   (`state == READ_ADDR`), which the doubled edge always skipped, so the
   control port's read engine got stuck in `ArHandshake` forever. This
   looked exactly like a hang from the gem5 side (confirmed via a
   temporary `Axi4SlaveEngine` pin trace before the fix, and by the
   register block's generated FSM source after). Fixed with
   `masterEngine.tick(false)`.

Both were verified with the exact reproduction: without fix 2, the very
first `STATUS` register read after `START` never returns, and the guest
spins forever. With both fixes, MMIO reads/writes complete immediately and
`isIdle()`-driven gating kicks in during the guest's unrelated (non-MMIO)
work, exactly as gem5_cva6's own accelerator does.

## Open issue: the GEMM job itself never signals DONE

With both fixes in place, the guest's `wait_done()` polling loop runs (each
`STATUS` read completes correctly, `BUSY` reads back `1`), but `DONE` never
sets. A temporary hierarchical debug tap (`dbg_state_o` on the wrapper,
`Flexnngine2Rtl3Accel::debugEngineState()` in the leaf — kept in place,
unused by default) shows **both** operand-load FSMs (`flexnngine2_rtl3_
load_engine`'s `a_state`/`b_state`) parked in `LS_WAIT` indefinitely, even
though `Axi4MasterEngine` reports both 8-beat AR/R bursts (A tile id=0, B
tile id=2) fully delivered (`RLAST` seen, `beatsSent == totalBeats`).
Something between the AXI4 R channel and the AXI-stream `taxi_axi_dma_rd`
hands to `load_engine` isn't registering completion under gem5's real DMA
timing, despite the *same* DUT passing `accum_chain_gemm` cleanly in
`cocotb/` (which drives the master port with `cocotbext-axi`'s `AxiRam`
instead of `Axi4MasterEngine`) — so this looks like a timing-sensitive
interaction specific to `Axi4MasterEngine`'s delivery pattern (it presents
each outstanding read's 8 beats as one uninterrupted burst rather than
interleaving between the two concurrently-outstanding IDs; `taxi_axi_
crossbar_rd`/`taxi_axi_dma_rd`'s expectations around that haven't been
root-caused yet). Not investigated further this session — worth picking up
with a waveform (`--trace-fst` is already wired into both Makefiles) rather
than more `fprintf` guessing.
