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