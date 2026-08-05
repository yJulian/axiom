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