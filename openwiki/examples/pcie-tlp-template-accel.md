---
type: worked-example
title: "Worked Example: pcie_tlp_template_accel"
description: The same endpoint device as pcie_template_accel rebuilt on the TLP-level path (RTLPcieTlpDevice leaf), to directly compare abstraction levels — identical register/DMA/interrupt behavior with only DeviceID/VERSION differing, and an independent hand-built-TLP cocotb test.
tags: [pcie, tlp, axi4-stream, rtl-cosimulation, worked-example]
verified:
  - by: openwiki/0.5.1
    at: 2026-09-10T13:13:27.916Z
sources:
  - id: openwiki-source-d1fb77f2d6500afd5c92bcdc
    resource: repo://examples/pcie_tlp_template_accel/cocotb/test_pcie_tlp_template.py
  - id: openwiki-source-e3aa9507b12c2108148983d1
    resource: repo://examples/pcie_tlp_template_accel/configs/run_pcie_tlp_template.py
  - id: openwiki-source-232d78345af63cd74688c765
    resource: repo://examples/pcie_tlp_template_accel/pcie_tlp_template_accel.sv
  - id: openwiki-source-2d436106c5401b3615d327e5
    resource: repo://examples/pcie_tlp_template_accel/pcie_tlp_template_device.hh
  - id: openwiki-source-f521aa21c9ccf9aed53a9380
    resource: repo://examples/pcie_tlp_template_accel/PcieTlpTemplateAccel.py
generated: { by: "claude-code", at: "2026-09-10T13:13:27.916Z" }
---

# Worked Example: pcie_tlp_template_accel

`pcie_tlp_template_accel` is the worked example for
[the TLP-level PCIe path](/openwiki/architecture/pcie-tlp-integration.md) —
a `RTLPcieTlpDevice` leaf. It is deliberately the *same device* as
[pcie_template_accel](/openwiki/examples/pcie-template-accel.md): same
registers, same scratchpad RAM, same DMA engine, same interrupt. The pair
exists to compare the two abstraction levels on identical functionality,
not to compare two different devices — what differs is everything about
how a request arrives.

## The DUT (`pcie_tlp_template_accel.sv`)

Instead of an AXI4 slave port with an address and byte-enables, this DUT
receives raw PCIe transaction-layer packets on the completer-request
stream, parses the header itself (fmt, type, length, requester ID, tag,
byte enables, 64-bit address), and builds the completion header itself
(completer ID, status, byte count, lower address) on the completion
stream. Its own DMA works the same way in reverse, on the requester
streams. BAR0 offsets are the low bits of the TLP's address, which is
already BAR-relative — `src/pcie/pcie_tlp_engine.cc` translates it, the
same way the AXI4 path's `Axi4SlaveEngine` does.

Register map — identical layout to `pcie_template_accel`, with only
`VERSION` differing:

| Offset | Register | Access | Width | Meaning |
| --- | --- | --- | --- | --- |
| `0x0000` | `ID` | RO | 32b | `0x50434945` (`"PCIE"`) |
| `0x0008` | `VERSION` | RO | 32b | `0x00020000` (`2` = the TLP-level variant) |
| `0x0010` | `SCRATCH` | RW | 32b | |
| `0x0018` | `COUNTER` | RO | 32b | free-running cycle counter |
| `0x0020` | `DMA_SRC` | RW | 64b | |
| `0x0028` | `DMA_DST` | RW | 64b | |
| `0x0030` | `DMA_LEN` | RW | 32b | |
| `0x0038` | `CTRL` | RW | 32b | bit `0`=START (pulse), bit `1`=IRQ_EN, bit `2`=IRQ_FORCE (pulse) |
| `0x0040` | `STATUS` | RO | 32b | bit `0`=BUSY, bit `1`=DONE, bit `2`=IRQ_PENDING |
| `0x0048` | `IRQ_ACK` | WO | 32b | write 1 to bit `0` |
| `0x1000`–`0x1FFF` | scratchpad RAM | RW | — | 4 KiB |

Registers stay on the same 8-byte stride as the AXI4 variant, but for a
different mechanism: a TLP carries its payload as DWs with per-DW byte
enables, and keeping every register beat-aligned means a 64-bit access is
one beat and a 32-bit access is the low DW of one beat, with no DW-level
shifting anywhere in the file. See
[PCIe Path: TLP-Level Endpoint](/openwiki/architecture/pcie-tlp-integration.md)
for the header-format simplifications this relies on. The DUT hardcodes
`COMPLETER_ID = 16'h0008` (bus 0, dev 1, func 0) into every completion it
builds.

## The leaf class (`PcieTlpTemplateAccel`)

Implements `axion::PcieTlpCompleterPins` (CQ host→DUT, CC DUT→host),
`axion::PcieTlpRequesterPins` (RQ DUT→host, RC host→DUT), and the two
sideband pins (`rtlGetIrq()`, `isIdle()`), directly against the
Verilator-generated `Vpcie_tlp_template_top` — no dlopen indirection, the
usual direct-link pattern. Notably far shorter than the AXI4 examples'
pin accessors: 16 pins instead of 88, because a PCIe endpoint's four
streams carry addressing, length, and byte enables inside the packet
rather than on sideband wires — the parsing that buys has to happen
somewhere, and on this path it happens in the RTL.

`PcieTlpTemplateAccel.py` mirrors `PcieTemplateAccel.py`'s PCIe-identity
params closely, with two deliberate differences: `DeviceID = 0x0002`
(distinct from the AXI4-bridge variant's `0x0001`, so a guest can tell
which it's talking to when both are present) and a `completer_id = 0x0008`
param matching the DUT's hardcoded `COMPLETER_ID`. `VendorID = 0x1DE5`,
`ClassCode = 0xFF`, `BAR0 = PciMemBar(size="64KiB")`, and
`InterruptLine = 0x11`/`InterruptPin = 0x01` (the PLIC source is still
slot-derived, not taken from `InterruptLine`, exactly as in the AXI4
variant — see
[PCIe Path: AXI4-Bridge Endpoint](/openwiki/architecture/pcie-integration.md)).
As with the AXI4 variant there is no `generateDeviceTree()`, since a PCIe
endpoint is discovered by walking ECAM config space.

## Testing

- **`make tb-pcie-tlp`** — cocotb, no gem5. `test_pcie_tlp_template.py`
  hand-builds real PCIe transaction-layer packets, pushes them at the
  completer-request stream, and decodes the completion headers the RTL
  builds in reply; playing the host also means servicing the requester
  streams — answering the DUT's own MemRd TLPs with CplD, and applying its
  MemWr TLPs to a dict standing in for host memory. **The TLP encoding is
  written out longhand here rather than shared with
  `src/pcie/pcie_tlp_engine.cc` on purpose**: an independent second
  implementation of the header layout makes this a test of the RTL's own
  parser, rather than a test that two copies of one helper agree with each
  other.
- **`make run-pcie-tlp-example`** — the bare-metal end-to-end run,
  identical in shape to `run_pcie_template.py` but with
  `PcieTlpTemplateAccel` in the slot instead of `PcieTemplateAccel`. **The
  guest program is the same test as the AXI4 variant's**, and that is the
  headline result: enumeration, BAR programming, register access,
  scratchpad bursts, bus-master DMA, and INTx all behave identically from
  software, because the difference between the two paths is confined
  entirely to where the TLP encoding happens — in gem5 for one, in the RTL
  for the other.
