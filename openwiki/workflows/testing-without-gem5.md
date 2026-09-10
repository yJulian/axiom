---
type: workflow
title: Verifying Without gem5 or a RISC-V Toolchain
description: How to validate AXION's RTL and protocol logic independent of gem5 and without a RISC-V cross-toolchain — the cocotb testbenches, the cocotb-env venv setup, the plugin-path standalone testbench, and the gtest suites covering engine logic and out-of-order-by-ID behavior.
tags: [testing, cocotb, gtest, verification, workflow]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-5861b69d3b5a156bb50383fe
    resource: repo://examples/dma_memcopy_accel/cocotb/test_dma_memcopy.py
  - id: openwiki-source-a17c2ccf8a906224ae848ec4
    resource: repo://examples/fifo_pio_accel/cocotb/test_fifo_pio.py
  - id: openwiki-source-012f2c78e3b1446dfc35803f
    resource: repo://Makefile
  - id: openwiki-source-5c746dc0c45cb3d297594cf7
    resource: repo://requirements-cocotb.txt
  - id: openwiki-source-368d628dfce956e4a2a12cbf
    resource: repo://scripts/setup_cocotb_env.sh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Verifying Without gem5 or a RISC-V Toolchain

Every worked example's `configs/run_*.py` is correct integration wiring,
but actually running it needs a compiled RISC-V binary that exercises the
accelerator's MMIO range or drives PCIe enumeration — and this environment
has no RISC-V cross-toolchain to produce one. AXION instead provides two
independent verification paths that need neither gem5 nor that toolchain:
cocotb testbenches driving real Verilator RTL directly, and gtest suites
covering the bridge engines' own logic against mocked pin implementations.

## cocotb testbenches

Each `examples/*/cocotb/` directory drives its example's Verilator RTL
directly via cocotb + [cocotbext-axi](https://github.com/alexforencich/cocotbext-axi),
completely independent of gem5:

| `make` target | Directory | What it drives |
| --- | --- | --- |
| `make tb` | `examples/fifo_pio_accel/cocotb` | `fifo_pio_top`'s AXI4 slave pins directly via `AxiMaster` — full AW/W/B write burst, AR/R read burst, distinct AWID/ARID values, push/pop round-trip. See [fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md). |
| `make tb-dma` | `examples/dma_memcopy_accel/cocotb` | The control/status port the same way, plus gem5's role on the DMA AXI4 master port via an `AxiRam` memory model — three channels started concurrently, each tagged by its own channel index as AXI ID. See [dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md). |
| `make tb-rtl3` | `examples/flexnngine2_rtl3_accel/cocotb` | `flexnngine2_rtl3_top.sv` directly, RTL pulled from a sibling `gem5_cva6` checkout — proves the RTL correct independent of both gem5 and the AXION wrapper. See [flexnngine2_rtl3_accel](/openwiki/examples/flexnngine2-rtl3-accel.md). |
| `make tb-pcie` | `examples/pcie_template_accel/cocotb` | The BAR0 AXI4 slave port with `AxiMaster` and the DMA master port with `AxiRam` — registers, burst + strobes, DMA, interrupt assert/ack. See [pcie_template_accel](/openwiki/examples/pcie-template-accel.md). |
| `make tb-pcie-tlp` | `examples/pcie_tlp_template_accel/cocotb` | Hand-built real TLPs against the completer-request stream, decoding the completion headers the RTL produces. See [pcie_tlp_template_accel](/openwiki/examples/pcie-tlp-template-accel.md). |
| `make tb-plugin` | `examples/fifo_pio_accel_plugin` | The same FIFO DUT built as a `.so`, driven purely through `src/axi/axi4_plugin_abi.h` with no Verilator headers at all. See [fifo_pio_accel_plugin](/openwiki/examples/fifo-pio-accel-plugin.md). |

**Important caveat, documented for `tb-dma`:** `cocotbext-axi`'s `AxiRam`
services requests in arrival order, so that test proves
concurrent-outstanding-by-ID correctness against real RTL, not literal
out-of-order completion — that half is covered only by the mocked
gtests below.

## Setting up the cocotb venv

```bash
make cocotb-env
```

Runs `scripts/setup_cocotb_env.sh`, which creates (or reuses) a Python
venv at `.venv/` and installs `requirements-cocotb.txt` (`cocotb==2.0.1`,
`cocotbext-axi==0.1.28`) into it — never system-wide. This script is
independent of `scripts/setup_env.sh` (the gem5 submodule/mirror step);
it has nothing to do with gem5 or Verilator's own build, only the Python
side of `examples/*/cocotb/`. Every `make tb*` target depends on
`cocotb-env` and runs `PATH="$(abspath .venv/bin):$PATH" $(MAKE) -C
examples/<name>/cocotb`, so `make tb*` alone (with no prior manual setup)
is always sufficient. Each `examples/*/cocotb/` directory has its own
`Makefile` (cocotb's own `Makefile.verilator`, the same
`VERILOG_SOURCES`/`-Wno-*` pattern as the RTL-only `Makefile` one level up
that `make verilate`/`verilate-dma` etc. use).

## gtest suites (mocked pins, no RTL at all)

Where the cocotb testbenches verify real RTL against a strict AXI4
requester, the gtest suites (built via `scons build/RISCV/unittests.opt`)
verify the bridge engines' *own* protocol logic against mocked pin
implementations — no Verilator model involved:

- `src/axi/axi4_master_engine.test.cc` / `axi4_slave_engine.test.cc` cover
  `Axi4MasterEngine`/`Axi4SlaveEngine` directly. Notably,
  `CrossIdReadsCompleteOutOfOrderSameIdStaysInOrder` and
  `CrossIdWritesCompleteOutOfOrderSameIdStaysInOrder` are the **only**
  tests in the repository that exercise literal out-of-order completion
  across AXI IDs — the property `tb-dma`'s `AxiRam`-backed test cannot
  prove because `AxiRam` services requests in arrival order. See
  [AXI4 Bridge Engines](/openwiki/architecture/axi4-bridge-engines.md).
- `src/pcie/pcie_tlp_engine.test.cc` (`PcieTlpEngineTest`) covers
  `PcieTlpCompleterEngine`/`PcieTlpRequesterEngine`'s header encode/decode,
  posted-write behavior, completion-status mapping, and out-of-order
  completion across PCIe tags. See
  [PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md).

## Summary: what covers what

| Property | Proven by |
| --- | --- |
| Real RTL speaks correct AXI4 protocol | cocotb testbenches (`tb`, `tb-dma`, `tb-pcie`, `tb-pcie-tlp`, `tb-rtl3`) |
| Concurrent-outstanding-by-ID against real RTL | `tb-dma` (arrival-order servicing only) |
| Literal cross-ID out-of-order completion | `axi4_master_engine.test.cc`'s `CrossId*CompleteOutOfOrder*` gtests (mocked) |
| Plugin ABI usable with zero compile-time DUT knowledge | `tb-plugin` |
| PCIe TLP engine header logic in isolation | `pcie_tlp_engine.test.cc` (mocked) + `tb-pcie-tlp` (real RTL) |
