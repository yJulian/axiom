---
type: architecture-component
title: Plugin Path (dlopen ABI)
description: The opt-in runtime-generic RTL loading path — the plain-C axi4_plugin_abi.h contract, RTLPioDevicePlugin/RTLDmaDevicePlugin's dlsym-and-cache mechanics, the rtl_plugin.mk build template, and the generic rtl_plugin_shim.cc.
tags: [dlopen, plugin, abi, verilator, rtl-cosimulation]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-ac4f8bb4c3d9c10799aa5570
    resource: repo://plugin/rtl_plugin_shim.cc
  - id: openwiki-source-04540febcb699b52a71803e7
    resource: repo://plugin/rtl_plugin.mk
  - id: openwiki-source-a7c7c64f3e97af2930c064c0
    resource: repo://src/axi/axi4_plugin_abi.h
  - id: openwiki-source-79faf18a4f34b95559882432
    resource: repo://src/dev/rtl/rtl_pio_device_plugin.cc
  - id: openwiki-source-9a75863b12fdcab9b49aed72
    resource: repo://src/dev/rtl/rtl_pio_device_plugin.hh
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Plugin Path (dlopen ABI)

`RTLPioDevicePlugin` / `RTLDmaDevicePlugin` (`src/dev/rtl/rtl_{pio,dma}_device_plugin.{hh,cc}`)
are concrete subclasses of `RTLPioDevice`/`RTLDmaDevice` that satisfy the
same `Axi4SlavePins`/`Axi4MasterPins` contract as any direct-link leaf, but
by `dlopen`ing a `.so` at construction and driving it through a small,
ABI-stable C interface (`src/axi/axi4_plugin_abi.h`) instead of binding to a
`Vxxx_top.h` at compile time. This is an **opt-in, additive** path — it
changes nothing about the [direct-link default](/openwiki/architecture/overview.md).

**Motivation.** Every DUT is wired through the same `hw/axi4/axi4_pins.sv`
adapter, so the flat `s_axi_*`/`m_axi_*` pin names are identical across every
possible model — only the generated Verilator class name differs. That makes
a runtime-generic loading path feasible without losing per-pin fidelity: a
new RTL model can be plugged in via a `rtl_library` param alone, with no new
C++ leaf class or `.py` `SimObject` file.

## The ABI (`src/axi/axi4_plugin_abi.h`)

Plain C, not C++ — `dlopen` crosses a shared-library boundary between two
independently compiled binaries, and C++ vtable/ABI layout isn't guaranteed
stable across that boundary the way a plain C struct/function ABI is. This
is the one deliberate deviation from `gem5_cva6`'s `AccelInterface` (see
[Prior Art](/openwiki/reference/prior-art.md)), which does `dlopen` a C++
vtable and relies on both sides being built with a matching compiler/stdlib.

The ABI defines:

- An opaque `AxionRtlInstance` handle (one `VerilatedContext` + one top-module
  instance inside the `.so`).
- Four plain-C-struct types — `AxionAxi4SlaveInputs`/`Outputs` and
  `AxionAxi4MasterInputs`/`Outputs` — with fields named field-for-field after
  `axi4_pins.sv`'s flat port list, so the mapping in the shim and in the
  `dlopen`'d gem5 classes is mechanical.
- Lifecycle/clock functions: `axion_rtl_create`/`destroy`, `axion_rtl_set_clk`/
  `set_rst_n`, `axion_rtl_eval` (settles combinational logic without
  advancing the clock, mirroring `VerilatedRtlModel<TopT>::settle()`).
- Capability queries: `axion_rtl_has_slave_port()`/`has_master_port()` — the
  `dlopen`'d leaf class `fatal()`s at construction if the role it needs isn't
  reported, rather than silently reading zeroed structs.
- Batched pin access: `axion_rtl_slave_drive`/`slave_sample` and
  `axion_rtl_master_drive`/`master_sample`, each pair costing exactly two ABI
  crossings per tick (plus `axion_rtl_eval`) regardless of how many
  individual pins changed.
- `axion_rtl_abi_version()`, checked against `AXION_RTL_PLUGIN_ABI_VERSION`
  (currently `2`) — a `.so` reporting a mismatched version is rejected at
  load time rather than driven with stale struct-layout assumptions.

Every `Set*`/`Get*` pin access on the leaf class's C++ side is cached
locally into a POD struct (`in_`/`out_` members) and only actually crosses
the ABI boundary, batched, inside `axiEval()` — one `*_drive()` + one
`axion_rtl_eval()` + one `*_sample()` per port role per `axiEval()` call,
instead of one crossing per individual pin.

## `RTLPioDevicePlugin` / `RTLDmaDevicePlugin`

At construction, `loadPlugin(p.rtl_library)` `dlopen`s the `.so` named by the
`rtl_library` param (`RTLD_NOW | RTLD_LOCAL`) — matching `gem5_cva6`'s
`RtlAccelerator` precedent of resolving at construction, not deferred to
`init()`. Every `axion_rtl_*` symbol is resolved via `dlsym()` into a cached
member function pointer (`resolveSymbol<FnT>()`), and `fatal()`s on any
missing symbol. After resolving, the ABI version is checked and the required
port-role capability (`axion_rtl_has_slave_port()` for
`RTLPioDevicePlugin`) is verified — both `fatal_if()` on failure. `axiEval()`
then implements the batched pattern: `slaveDrive_(rtl_, &in_)` →
`eval_(rtl_)` → `slaveSample_(rtl_, &out_)`. Every `axiSlaveSet*`/`Get*`
override is a one-line read/write of `in_`/`out_`. The destructor calls
`destroy_(rtl_)` then `dlclose(libHandle_)`.

## `plugin/rtl_plugin.mk`

A reusable, `include`-able Makefile template that locates its own directory
via `$(dir $(lastword $(MAKEFILE_LIST)))`, so it works regardless of the
caller's nesting depth. A caller sets `TOP_MODULE` (required, e.g.
`fifo_pio_top`) and `PLUGIN_SOURCES` (required — the caller's own DUT + TOP
wrapper `.sv` files only; the template prepends `hw/axi4`'s three shared
files: `axi4_pkg.sv`, `axi4_if.sv`, `axi4_pins.sv`), plus optional
`PLUGIN_HAS_SLAVE` (default 1), `PLUGIN_HAS_MASTER` (default 0), `OUT_DIR`
(default `obj_dir_plugin`), and `SO_NAME` (default
`lib$(TOP_MODULE)_plugin.so`), and gets the `plugin-so`/`plugin-clean`
targets.

The build recipe follows the proven Verilator-to-`.so` pattern from
`~/gem5_cva6/accelerator/Makefile`: `verilator --cc` (not `--build`, since a
*shared* object needs its own link step rather than Verilator's own
executable-producing `--build` link), then `make -f Vxxx.mk` for the static
archive of Verilated model objects, then a manual `g++ -shared -fPIC` link of
`plugin/rtl_plugin_shim.cc` + the Verilator runtime (`verilated.cpp` **and**
`verilated_threads.cpp` — omitting the latter links but leaves an undefined
`VlThreadPool` symbol at `dlopen` time) + that archive into the final `.so`.

## `plugin/rtl_plugin_shim.cc`

The one piece of generated-per-model C++ that remains, but as a single
non-templated file compiled repeatedly with different `-D`/`-include` flags
(`AXION_TOP_CLASS`, `AXION_PLUGIN_HAS_{SLAVE,MASTER}`) — nothing to
hand-edit per model. `AxionRtlInstance` wraps a `VerilatedContext` and a
`std::unique_ptr<AXION_TOP_CLASS>` (the `-DAXION_TOP_CLASS=Vxxx_top` macro
selects the concrete Verilator class); `axion_rtl_create`/`destroy`/`set_clk`
(`top->clk_i`)/`set_rst_n` (`top->rst_ni`)/`eval` are thin wrappers, and
`axion_rtl_has_slave_port()`/`has_master_port()` just return the compile-time
`AXION_PLUGIN_HAS_{SLAVE,MASTER}` macros. Field access is the same
one-line-per-pin mapping already hand-written in `fifo_pio_device.cc`
(the direct-link leaf), written once generically here instead of once per
DUT.

## Verifying without gem5

See
[Verifying Without gem5 or a RISC-V Toolchain](/openwiki/workflows/testing-without-gem5.md)
and [fifo_pio_accel_plugin](/openwiki/examples/fifo-pio-accel-plugin.md) for
the standalone `make tb-plugin` testbench that proves the ABI usable
generically by driving the same `fifo_pio_accel` DUT purely through
`axi4_plugin_abi.h`, with no `Vfifo_pio_top.h` and no compile-time knowledge
of the DUT.
