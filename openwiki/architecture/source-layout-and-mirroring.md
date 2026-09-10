---
type: architecture-component
title: Source Layout and the gem5 Mirror
description: Why AXION's own sources under src/{axi,pcie,cpu/rtl,dev/rtl} and examples/ must never be edited under ext/gem5/src, the cp -rs mirroring trick that makes gem5's build see them, and the shared hw/ SystemVerilog layer.
tags: [build-system, gem5, submodule, mirroring, systemverilog]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-8c8e70a48936c50bb4fd4eed
    resource: repo://hw/axi4/axi4_pins.sv
  - id: openwiki-source-673da0b12134d79981dc2b62
    resource: repo://hw/axi4/axi4_pkg.sv
  - id: openwiki-source-4388e4f3c747be1bd93709fb
    resource: repo://scripts/build_gem5.sh
  - id: openwiki-source-4c452e03a7d9866e4f9185f9
    resource: repo://scripts/setup_env.sh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Source Layout and the gem5 Mirror

`ext/gem5` is a git submodule (pinned, shallow). AXION's own C++ sources
live at the repo root under `src/` (and the worked examples under
`examples/`) — **never** edit anything under `ext/gem5/src/axi/`,
`ext/gem5/src/pcie/`, `ext/gem5/src/cpu/rtl/`, `ext/gem5/src/dev/rtl/`, or
`ext/gem5/src/examples/` directly. Those five paths are mirrors, regenerated
by `scripts/build_gem5.sh`; edits made under a mirror are overwritten (or
simply invisible to git) the next time it re-runs.

## Why a mirror at all

gem5's own `src/SConscript` walks the source tree with `followlinks=False`,
so a plain symlinked directory (`ln -s src/axi ext/gem5/src/axi`) would be
completely invisible to the build — SCons would never see any file inside
it. `scripts/build_gem5.sh` instead uses `cp -rs`, which creates a *real*
directory tree with each individual *file* symlinked back to this repo. A
real directory containing symlinked files is exactly what the
`followlinks=False` walk does see, while edits still land in this repo's own
`src/` (the true source) because each file is a symlink back to it — editing
through the mirror edits the real file, but nothing should be edited through
the mirror by convention, since the mirror itself is fully regenerated on
every re-run (`mirror()` in `build_gem5.sh` does `rm -rf "$dst"` before
`cp -rs`, so a deleted/renamed source file doesn't linger as an orphaned
mirror entry).

## Same relative path as gem5's own tree

Each subdirectory mirrors at the *same relative path* it would occupy inside
gem5's own tree: `src/axi` → `ext/gem5/src/axi`, `src/pcie` →
`ext/gem5/src/pcie`, `src/cpu/rtl` → `ext/gem5/src/cpu/rtl`, `src/dev/rtl` →
`ext/gem5/src/dev/rtl`, `examples` → `ext/gem5/src/examples`. This matters
because AXION's own headers use gem5-style `#include` paths (e.g.
`#include "axi/verilated_model.hh"`) resolved against gem5's `src/` as the
include root — the same convention every native gem5 source file uses. None
of these five paths (`axi/`, `pcie/`, `cpu/rtl/`, `dev/rtl/`, `examples/`)
exist in stock gem5, so there is no collision with anything gem5 itself
ships. This also matches how the sibling `gem5_cva6` project mirrors its own
custom sources (`gem5/src/cpu/rtl/`, not some wrapped or prefixed location) —
see [Prior Art](/openwiki/reference/prior-art.md).

## `plugin/` is never mirrored

`plugin/` (the opt-in plugin path's Makefile template + generic shim, see
[Plugin Path](/openwiki/architecture/plugin-abi.md)) is a sixth top-level
source area, but unlike the five above it is **never** mirrored into
`ext/gem5/src` at all — it's build tooling that produces a `.so` outside
gem5's own build entirely, not something gem5's `SConscript` needs to see.

## Ordering: verilate before mirror

`scripts/build_gem5.sh` must run *after* `make -C examples/fifo_pio_accel`
(or any other example) has produced its `obj_dir/*.a`, so those generated
artifacts get mirrored in too. `scripts/setup_env.sh` performs the
one-shot environment setup (`git submodule update --init ext/gem5`, then a
pre-verilate mirror pass) but explicitly does not verilate any example or
build gem5 itself — the top-level `Makefile`'s `gem5` target
([Building and Running AXION](/openwiki/workflows/build-and-verilate.md))
handles the full `verilate` → `build_gem5.sh` → `scons` sequence, and
re-running the mirror step alone is always safe once at least one `make
verilate*` has run at some point.

## The shared `hw/` SystemVerilog layer

`hw/` holds the SystemVerilog reused by both the direct-link and plugin
paths — it is not part of the mirroring mechanism above (SystemVerilog isn't
part of gem5's own C++ build), but is the shared hardware-side counterpart
to the source-layout discipline: one canonical copy of the AXI4 pin adapters
rather than one per example.

- `hw/axi4/axi4_pkg.sv` — shared typedefs and default field widths (e.g.
  `AXI_ID_WIDTH_DEFAULT = 4`, `AXI_ADDR_WIDTH_DEFAULT = 64`,
  `AXI_DATA_WIDTH_DEFAULT = 64`) used by `axi4_if.sv` and `axi4_pins.sv`;
  individual `axi4_if` instances may override these per instantiation.
- `hw/axi4/axi4_if.sv` — the actual 5-channel AXI4 interface a DUT connects
  to.
- `hw/axi4/axi4_pins.sv` — flat-port adapters wrapping a clean `axi4_if`
  handle around the scalar port list Verilator's generated C++ model
  actually exposes. Two flavors named after the DUT's own role:
  `axi4_pins_slave_port` (DUT is the AXI4 slave, e.g. a PIO/register port
  driven by gem5 as sole requester — flat `s_axi_*` pins) and
  `axi4_pins_master_port` (DUT is the AXI4 master, e.g. a DMA requester —
  flat `m_axi_*` pins).
- `hw/pcie/` mirrors the same idea one protocol layer up for the TLP-level
  PCIe path — see
  [PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md).

A **TOP** module (e.g. `examples/fifo_pio_accel/fifo_pio_top.sv`) instantiates
one or more of these pin adapters plus the DUT, and wires the adapter's
`aif` interface port straight into the DUT's own `axi4_if` port.
`plugin/rtl_plugin.mk`'s build recipe likewise prepends these same three
`hw/axi4/` files ahead of a caller's own DUT/TOP sources, so the plugin path
verilates the identical adapter layer instead of a second copy.
