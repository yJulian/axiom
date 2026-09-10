---
type: workflow
title: Building and Running AXION
description: Operational guide to the top-level Makefile — verilate-before-gem5 ordering, building gem5.opt, running each worked example, and what each make target does end to end.
tags: [build, makefile, verilator, scons, workflow]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-012f2c78e3b1446dfc35803f
    resource: repo://Makefile
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Building and Running AXION

The top-level `Makefile` (repo root) drives submodule setup, RTL
verilation, the gem5 build itself, and running each worked example. Run
`make help` for the live target summary.

## First-time setup

```bash
scripts/setup_env.sh   # git submodule update --init ext/gem5 + first mirror pass
```

Checks out the `ext/gem5` submodule and runs a pre-verilate mirror pass
(`scripts/build_gem5.sh`) — see
[Source Layout and the gem5 Mirror](/openwiki/architecture/source-layout-and-mirroring.md).
It does **not** verilate any example or build gem5 itself.

## Ordering: verilate before gem5

```bash
make verilate   # build examples/fifo_pio_accel's RTL (Verilator -> obj_dir/*.a)
make gem5       # re-mirror (now including the verilated .a) + scons build
                #   -> build/RISCV/gem5.opt at the repo root
```

Order matters the **first time**: the `src/` → `ext/gem5/src` mirror step
needs `examples/fifo_pio_accel/obj_dir/*.a` to already exist to pick it up.
`make gem5` depends on `verilate verilate-dma verilate-pcie
verilate-pcie-tlp` (so it verilates every example, not just `fifo_pio_accel`)
and then always re-runs the mirror itself via `scripts/build_gem5.sh`, so
once verilated at least once, `make gem5` alone is always safe to re-run.
`scons -C $(GEM5_DIR) build/RISCV/gem5.opt` places `build/` at the
directory scons was invoked *from* (this repo's root), not inside the
submodule — the same convention `gem5_cva6` documents and relies on.

## Per-example verilate targets

| Target | Builds RTL for |
| --- | --- |
| `make verilate` | `examples/fifo_pio_accel` |
| `make verilate-dma` | `examples/dma_memcopy_accel` |
| `make verilate-pcie` | `examples/pcie_template_accel` |
| `make verilate-pcie-tlp` | `examples/pcie_tlp_template_accel` |
| `make verilate-plugin` | the FIFO DUT as a plugin `.so` via `plugin/rtl_plugin.mk` (opt-in dlopen path — independent of `verilate`/`gem5` and of every other target) |

Each delegates to `$(MAKE) -C examples/<name>`.

## Running the worked examples

```bash
make run-fifo-example ARGS='--binary <path/to/riscv/elf>'
make run-dma-example ARGS='--binary <path/to/riscv/elf>'
```

Both depend on `gem5` and then invoke `$(GEM5_BUILD)
examples/<name>/configs/run_<name>.py $(ARGS)` — see
[fifo_pio_accel](/openwiki/examples/fifo-pio-accel.md) and
[dma_memcopy_accel](/openwiki/examples/dma-memcopy-accel.md) for what each
config script actually does and why this environment cannot itself produce
a RISC-V ELF to pass via `--binary`.

### PCIe examples

```bash
make pcie-test-elf              # build the bare-metal guest ELF from riscv_test/
make run-pcie-example            # bare-metal PCIe test on HiFive (builds the ELF itself)
make run-pcie-tlp-example        # same test, TLP-level RTL variant
make run-pcie-linux-example ARGS='--disk-image <img>'
```

`pcie-test-elf`/`pcie-tlp-test-elf` build the bare-metal guest test via
`$(MAKE) -C examples/<name>/riscv_test`, using the host's
`riscv64-linux-gnu-gcc`. `run-pcie-example` depends on `gem5 pcie-test-elf`
and runs `configs/run_pcie_template.py`; `run-pcie-tlp-example` is the
identical shape against `pcie_tlp_template_accel`. `run-pcie-linux-example`
depends only on `gem5` (no test-ELF build — the guest program comes from
the caller-supplied disk image) and runs
`configs/run_pcie_template_linux.py`. See
[pcie_template_accel](/openwiki/examples/pcie-template-accel.md) and
[PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md)
for what each of the three run modes actually proves.

### Plugin path

```bash
make verilate-plugin   # build the FIFO DUT as a .so via plugin/rtl_plugin.mk
make tb-plugin          # standalone plugin-ABI testbench, no gem5 needed
```

See [Plugin Path (dlopen ABI)](/openwiki/architecture/plugin-abi.md) and
[fifo_pio_accel_plugin](/openwiki/examples/fifo-pio-accel-plugin.md).

## cocotb testbenches (no gem5 needed)

```bash
make cocotb-env    # create .venv/ + install cocotb/cocotbext-axi
make tb             # FIFO/PIO
make tb-dma          # DMA memcopy
make tb-rtl3         # FleXNNgine2 rtl3 GEMM (RTL pulled from a sibling gem5_cva6 checkout)
make tb-pcie         # PCIe template
make tb-pcie-tlp     # TLP-level PCIe template
```

Each target depends on `cocotb-env` and runs `PATH="$(VENV_BIN):$PATH"
$(MAKE) -C examples/<name>/cocotb`. See
[Verifying Without gem5 or a RISC-V Toolchain](/openwiki/workflows/testing-without-gem5.md)
for what these actually verify and why they matter in an environment with
no RISC-V cross-toolchain.

## Cleaning up

```bash
make clean
```

Runs `clean` in every example directory (including `riscv_test/` and the
plugin directory), removes each cocotb testbench's `sim_build`/
`results.xml`, and removes the gem5-side mirrors
(`ext/gem5/src/{axi,pcie,cpu/rtl,dev/rtl,examples}`) — but not the
`ext/gem5` submodule checkout itself.

## Quick reference: what each Makefile target actually runs

- `make verilate*` → `$(MAKE) -C examples/<name>` (Verilator, produces
  `obj_dir/*.a`)
- `make gem5` → verilates every example, then `scripts/build_gem5.sh`
  (mirror), then `scons -C ext/gem5 build/RISCV/gem5.opt -j$(JOBS)`
- `make run-*-example` → depends on `gem5` (and, for PCIe bare-metal, the
  test-ELF target), then runs `$(GEM5_BUILD) <config>.py $(ARGS)`
- `make tb*` → depends on `cocotb-env`, then `$(MAKE) -C
  examples/<name>/cocotb` with the venv on `PATH`
- `make verilate-plugin` / `make tb-plugin` → entirely independent of
  `verilate`/`gem5`, using `plugin/rtl_plugin.mk`
