# Files

- [Building and Running AXION](build-and-verilate.md) - Operational guide to the top-level Makefile — verilate-before-gem5 ordering, building gem5.opt, running each worked example, and what each make target does end to end.
- [Verifying Without gem5 or a RISC-V Toolchain](testing-without-gem5.md) - How to validate AXION's RTL and protocol logic independent of gem5 and without a RISC-V cross-toolchain — the cocotb testbenches, the cocotb-env venv setup, the plugin-path standalone testbench, and the gtest suites covering engine logic and out-of-order-by-ID behavior.
