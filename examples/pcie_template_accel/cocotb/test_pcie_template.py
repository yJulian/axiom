# test_pcie_template.py
#
# cocotb testbench for pcie_template_top -- the standalone (no-gem5)
# verification of the PCIe template DUT. Drives the BAR0 window's AXI4
# slave port with an AxiMaster (playing the role gem5's RTLPciDevice
# plays once a BAR has been programmed, with the BAR-relative offsets it
# hands down) and plays host memory on the bus-master DMA port with an
# AxiRam model. Independent of gem5 -- run via `make tb-pcie` at the repo
# root, or `make -C examples/pcie_template_accel/cocotb` with the venv
# from scripts/setup_cocotb_env.sh on PATH.
#
# Nothing here knows about PCIe, and that is the point: config space,
# BARs and INTx are gem5's job (src/dev/rtl/rtl_pci_device.cc), exactly as
# they are a hard IP block's job on real silicon. What the RTL owes is
# ordinary AXI4 behavior, which is what gets checked below.

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, RisingEdge
from cocotbext.axi import AxiBus, AxiMaster, AxiRam

# BAR0 register map -- see pcie_template_accel.sv's header comment.
REG_ID = 0x00
REG_VERSION = 0x08
REG_SCRATCH = 0x10
REG_COUNTER = 0x18
REG_DMA_SRC = 0x20
REG_DMA_DST = 0x28
REG_DMA_LEN = 0x30
REG_CTRL = 0x38
REG_STATUS = 0x40
REG_IRQ_ACK = 0x48

RAM_BASE = 0x1000
RAM_SIZE = 0x1000

CTRL_START = 1 << 0
CTRL_IRQ_EN = 1 << 1
CTRL_IRQ_FORCE = 1 << 2

STATUS_BUSY = 1 << 0
STATUS_DONE = 1 << 1
STATUS_IRQ_PENDING = 1 << 2

ID_MAGIC = 0x50434945  # "PCIE"
ID_VERSION = 0x00010000

HOST_RAM_SIZE = 0x10000


def as_int(resp_data):
    return int.from_bytes(resp_data, "little")


async def reset_dut(dut):
    dut.rst_ni.value = 0
    for _ in range(10):
        await RisingEdge(dut.clk_i)
    dut.rst_ni.value = 1
    await RisingEdge(dut.clk_i)


async def bringup(dut):
    """Clock + reset + the two bus agents, shared by every test below."""
    cocotb.start_soon(Clock(dut.clk_i, 10, units="ns").start())
    await reset_dut(dut)
    bar0 = AxiMaster(AxiBus.from_prefix(dut, "s_axi"), dut.clk_i, dut.rst_ni,
                     reset_active_level=False)
    host = AxiRam(AxiBus.from_prefix(dut, "m_axi"), dut.clk_i, dut.rst_ni,
                  reset_active_level=False, size=HOST_RAM_SIZE)
    return bar0, host


async def rd32(bar0, offset):
    return as_int((await bar0.read(offset, 4)).data)


async def wr32(bar0, offset, value):
    await bar0.write(offset, int(value).to_bytes(4, "little"))


async def wr64(bar0, offset, value):
    await bar0.write(offset, int(value).to_bytes(8, "little"))


@cocotb.test()
async def identity_and_scratch(dut):
    """The bare "is anything alive behind BAR0" check: the read-only magic
    and version registers report themselves, and the scratch register
    round-trips a value."""
    bar0, _ = await bringup(dut)

    assert await rd32(bar0, REG_ID) == ID_MAGIC, "BAR0 ID magic mismatch"
    assert await rd32(bar0, REG_VERSION) == ID_VERSION

    for value in (0xDEADBEEF, 0x00000000, 0xFFFFFFFF, 0xC0FFEE00):
        await wr32(bar0, REG_SCRATCH, value)
        assert await rd32(bar0, REG_SCRATCH) == value, \
            f"scratch did not round-trip {value:#x}"


@cocotb.test()
async def counter_advances(dut):
    """The free-running counter proves the DUT's clock is genuinely
    ticking rather than only being stepped for the duration of a
    transaction -- the failure mode a clock-gated bridge can silently
    introduce."""
    bar0, _ = await bringup(dut)

    first = await rd32(bar0, REG_COUNTER)
    await ClockCycles(dut.clk_i, 50)
    second = await rd32(bar0, REG_COUNTER)
    assert second > first, f"counter did not advance ({first} -> {second})"


@cocotb.test()
async def bar_ram_burst_and_strobes(dut):
    """The RAM region behind BAR0, exercised the way a real driver would:
    one multi-beat INCR burst in and back out, then a sub-beat write to
    confirm WSTRB actually masks bytes instead of clobbering the whole
    word."""
    bar0, _ = await bringup(dut)

    payload = bytes((i * 7 + 3) & 0xFF for i in range(64))
    await bar0.write(RAM_BASE, payload)
    got = (await bar0.read(RAM_BASE, 64)).data
    assert got == payload, "64-byte multi-beat burst did not round-trip"

    # Byte strobes: overwrite 2 bytes in the middle of a beat and confirm
    # its other 6 bytes survived.
    await bar0.write(RAM_BASE + 8, b"\xaa\xbb")
    beat = (await bar0.read(RAM_BASE + 8, 8)).data
    assert beat[:2] == b"\xaa\xbb", "strobed bytes were not written"
    assert beat[2:] == payload[10:16], "WSTRB failed to mask the other bytes"

    # The far end of the 4 KiB window is addressable too.
    await bar0.write(RAM_BASE + RAM_SIZE - 8, b"\x01\x02\x03\x04\x05\x06\x07\x08")
    assert (await bar0.read(RAM_BASE + RAM_SIZE - 8, 8)).data == \
        b"\x01\x02\x03\x04\x05\x06\x07\x08"


async def run_dma(dut, bar0, src, dst, length, timeout=4000):
    await wr64(bar0, REG_DMA_SRC, src)
    await wr64(bar0, REG_DMA_DST, dst)
    await wr32(bar0, REG_DMA_LEN, length)
    assert as_int((await bar0.read(REG_DMA_SRC, 8)).data) == src, \
        "DMA_SRC did not read back"
    # CTRL is one register: START has to be OR'd into whatever else is
    # already set, or kicking off a transfer would silently clear IRQ_EN.
    # Read-modify-write, exactly as a driver would.
    ctrl = await rd32(bar0, REG_CTRL)
    await wr32(bar0, REG_CTRL, ctrl | CTRL_START)

    for _ in range(timeout):
        status = await rd32(bar0, REG_STATUS)
        if status & STATUS_DONE:
            return status
        await ClockCycles(dut.clk_i, 1)
    raise AssertionError("DMA never signalled DONE")


@cocotb.test()
async def bus_master_dma(dut):
    """Bus-master direction: the DUT reads and writes host memory itself
    over the AXI4 master port. Reads carry AXI ID 0 and writes ID 1 (see
    pcie_template_accel.sv), so both of Axi4MasterEngine's per-ID queues
    are on the path when this same DUT runs under gem5."""
    bar0, host = await bringup(dut)

    src, dst, length = 0x2000, 0x3000, 64
    payload = bytes((i * 11 + 5) & 0xFF for i in range(length))
    host.write(src, payload)

    status = await run_dma(dut, bar0, src, dst, length)
    assert status & STATUS_BUSY == 0, "device still busy after DONE"
    assert host.read(dst, length) == payload, "DMA copied the wrong bytes"

    # A trailing partial word must be masked by WSTRB, not rounded up:
    # the byte just past the transfer has to survive untouched.
    host.write(0x4000, bytes(range(16)))
    host.write(0x5000, b"\xff" * 16)
    await wr32(bar0, REG_IRQ_ACK, 1)
    await run_dma(dut, bar0, 0x4000, 0x5000, 12)
    assert host.read(0x5000, 12) == bytes(range(12))
    assert host.read(0x500C, 4) == b"\xff\xff\xff\xff", \
        "DMA wrote past the requested length"


@cocotb.test()
async def interrupt_assert_and_ack(dut):
    """The interrupt direction. irq_o only rises when IRQ_EN is set, and
    only clears when software acknowledges -- the same edge RTLPciDevice
    turns into PciDevice::intrPost()/intrClear()."""
    bar0, host = await bringup(dut)

    assert dut.irq_o.value == 0, "irq asserted out of reset"

    # Forced interrupt with IRQ_EN clear: pending latches, pin stays low.
    await wr32(bar0, REG_CTRL, CTRL_IRQ_FORCE)
    assert await rd32(bar0, REG_STATUS) & STATUS_IRQ_PENDING
    assert dut.irq_o.value == 0, "irq_o must stay low while IRQ_EN is clear"

    await wr32(bar0, REG_CTRL, CTRL_IRQ_EN)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 1, "enabling IRQ_EN should expose the pending irq"

    await wr32(bar0, REG_IRQ_ACK, 1)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 0, "IRQ_ACK did not deassert irq_o"
    assert await rd32(bar0, REG_STATUS) & STATUS_IRQ_PENDING == 0

    # A completing DMA raises it again, with IRQ_EN still set.
    host.write(0x6000, b"\x5a" * 8)
    await run_dma(dut, bar0, 0x6000, 0x7000, 8)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 1, "DMA completion did not raise irq_o"

    await wr32(bar0, REG_IRQ_ACK, 1)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 0
    assert await rd32(bar0, REG_STATUS) & STATUS_DONE == 0, \
        "IRQ_ACK should clear DONE as well"
