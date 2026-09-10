# Files

- [AXI4 Signal Coverage](axi4-signal-coverage.md) - Tracks the full pin-level AXI4 signal set AXION's bridge is expected to generate correctly — what is audited and complete, what remains for RTLBaseCpu, and the two documented-not-fixed gaps in Axi4MasterEngine.
- [Prior Art: gem5_cva6 and gem5-verilator-ghdl](prior-art.md) - The two sibling on-machine projects AXION was designed by mining — gem5_cva6's dlopen'd AccelInterface/RtlAccelerator/dma_master_engine.cc, and gem5-verilator-ghdl's earlier build-system gotchas — and which specific AXION pieces model which.
- [RTL Device Base Classes](rtl-device-classes.md) - Concrete reference for AXION's five leaf-facing abstract base classes (RTLBaseCpu, RTLPioDevice, RTLDmaDevice, RTLPciDevice, RTLPcieTlpDevice) — gem5 base, engines/ports owned, SimObject params, idle-gating, and RTLBaseCpu's no-worked-example scope note.
