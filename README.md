# AXION: AXI4-based X-module Integration & Operation Network

**AXION** is a gem5 framework designed for high-performance RTL co-simulation of CPU, PIO (Programmed I/O), and DMA components over the AXI4 protocol. It bridges full-system software execution in gem5 with detailed hardware implementations, enabling precise hardware-software co-design and architectural trade-off analysis.

---

## Key Features

* **AXI4-Native Integration:** Complete support for standard AXI4 bus transactions, ensuring accurate protocol modeling and interoperability.
* **RTL Co-Simulation:** Seamlessly interface custom RTL modules (e.g., compiled via Verilator) directly into gem5 memory hierarchies and interconnects.
* **Flexible Component Coupling:** Supports a wide range of hardware modules:
  * **CPU Cores & Accelerators**
  * **PIO (Programmed I/O) Control Interfaces**
  * **DMA (Direct Memory Access) Controllers**
* **High-Performance Execution:** Efficient inter-process communication and clock synchronization to minimize co-simulation overhead.
* **Cycle-Accurate Profiling:** Facilitates deep performance evaluation, memory latency measurement, and system-level bottleneck identification.

---

## System Overview

AXION serves as an integration layer inside gem5. It captures memory and register access events from gem5's event-driven simulator, translates them into AXI4 protocol requests, and routes them to target RTL components.

---

## Two ways to add RTL

**Direct-link (default):** write a small C++ leaf class (`RTLPioDevice`/`RTLDmaDevice`/`RTLBaseCpu`) that `#include`s your Verilator-generated `Vxxx_top.h` and a matching `.py` SimObject file, then link it straight into `gem5.opt`. See `examples/fifo_pio_accel/` for the worked example.

```bash
make verilate   # build examples/fifo_pio_accel's RTL
make gem5       # mirror src/ into gem5 + scons build -> build/RISCV/gem5.opt
make tb         # standalone AXI4 testbench, no gem5 needed
```

**Plugin (opt-in):** turn your own DUT `.sv` + a TOP wrapper (wired through `hw/axi4/axi4_pins.sv`, same as the direct-link path) into a `.so` via the reusable `plugin/rtl_plugin.mk` template, then point `RTLPioDevicePlugin`/`RTLDmaDevicePlugin`'s `rtl_library` param at it — no new C++ leaf class or `.py` file needed per model. See `examples/fifo_pio_accel_plugin/` for the same FIFO DUT built this way.

```bash
make verilate-plugin   # build the FIFO DUT into a .so via plugin/rtl_plugin.mk
make tb-plugin         # standalone plugin-ABI testbench, no gem5 needed
```

See `CLAUDE.md` for the architecture behind both paths (the stable C ABI, why it's a plain C interface rather than a dlopen'd C++ vtable, and how the two paths relate).

---

## AXI4 signal coverage

`AXI4_signals.md` tracks the full AXI4 pin-level signal set this bridge is
expected to generate correctly. `RTLPioDevice`/`RTLDmaDevice` (and their
plugin-path counterparts) have been audited and brought to completion:
AWLOCK/AWCACHE/AWPROT/AWQOS/AWREGION and their AR equivalents are now driven
end-to-end (SV interface, pins adapter, C++ pin contract, both engines, the
`fifo_pio_accel` example, and the plugin ABI); `Axi4SlaveEngine` now
propagates real BRESP/RRESP error codes from the RTL into the gem5 packet
instead of silently dropping them; `Axi4MasterEngine` now streams true
multi-beat read completions (RLAST only on the final beat, not every beat)
and rejects non-INCR bursts loudly instead of silently mis-executing them.

`RTLBaseCpu` (`src/cpu/rtl/`) has **not** been covered by this pass and is
an explicit follow-up -- its master/slave port roles are reversed relative
to `RTLDmaDevice`'s DMA port (the RTL core is the AXI4 master on both its
inst and data ports), so the same signal-completeness work needs to be
redone against that role split rather than assumed to carry over.

Two gaps found during the audit are **documented, not fixed**, because
gem5's `DmaDevice::dmaRead()`/`dmaWrite()` convenience API only takes a
completion `Event*` and never exposes packet/error status back to the
caller:

- `Axi4MasterEngine` always drives BRESP/RRESP as `OKAY` -- there is
  currently no path for a real gem5 memory-system error to reach the DMA
  master port's response, short of bypassing `dmaRead`/`dmaWrite` for raw
  packet-level DMA.
- `Axi4MasterEngine` `panic`s on FIXED/WRAP bursts rather than executing
  them -- `Backend::issueRead`/`issueWrite` model one linear memory access
  per burst (exactly INCR addressing); real FIXED/WRAP support needs
  per-beat addressing, the same category of rework as the point above.