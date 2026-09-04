# test_pcie_tlp_template.py
#
# cocotb testbench for pcie_tlp_template_top -- the standalone (no-gem5)
# verification of the TLP-level PCIe DUT. Where test_pcie_template.py drives
# an AXI4 slave port with cocotbext-axi's AxiMaster, this one hand-builds
# real PCIe transaction-layer packets, pushes them at the completer-request
# stream, and decodes the completion headers the RTL builds in reply.
# Playing the host also means servicing the requester streams: answering the
# DUT's own MemRd TLPs with CplD, and applying its MemWr TLPs to a dict
# standing in for host memory.
#
# The TLP encoding is written out longhand here rather than shared with
# src/pcie/pcie_tlp_engine.cc on purpose: an independent second
# implementation of the header layout is what makes this a test of the RTL's
# parser, rather than a test that two copies of one helper agree.
#
# See hw/pcie/pcie_tlp_pkg.sv for the layout and for the one packing
# simplification (4-DW request headers only; completion headers padded to
# 4 DW so payload stays beat-aligned), and hw/pcie/pcie_tlp_if.sv for why
# these streams carry no TKEEP.
#
# Run via `make tb-pcie-tlp` at the repo root.

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, ReadOnly, RisingEdge

# --- TLP header fields (PCIe base spec; see hw/pcie/pcie_tlp_pkg.sv) ---
FMT_4DW_NODATA = 0b001
FMT_4DW_DATA = 0b011
FMT_3DW_DATA = 0b010
TYPE_MEM = 0b00000
TYPE_CPL = 0b01010
CPL_SUCCESS = 0b000

HOST_ID = 0x0100  # our requester ID; the DUT must echo it in completions
DUT_ID = 0x0008  # bus 0, dev 1, func 0 -- matches the DUT's COMPLETER_ID

# --- BAR0 register map: see ../pcie_tlp_template_accel.sv ---
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
BAR_RAM = 0x1000

CTRL_START = 1 << 0
CTRL_IRQ_EN = 1 << 1
CTRL_IRQ_FORCE = 1 << 2

STATUS_BUSY = 1 << 0
STATUS_DONE = 1 << 1
STATUS_IRQ_PENDING = 1 << 2

ID_MAGIC = 0x50434945  # "PCIE"
ID_VERSION = 0x00020000

MASK32 = 0xFFFFFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF


def dw0(fmt, ttype, length_dw):
    return ((fmt & 0x7) << 29) | ((ttype & 0x1F) << 24) | (length_dw & 0x3FF)


def req_dw1(requester_id, tag, last_be, first_be):
    return (((requester_id & 0xFFFF) << 16) | ((tag & 0xFF) << 8)
            | ((last_be & 0xF) << 4) | (first_be & 0xF))


def beat(lo_dw, hi_dw):
    """Two DWs into one 64-bit stream beat: DW N low, DW N+1 high."""
    return ((hi_dw & MASK32) << 32) | (lo_dw & MASK32)


def addr_beats(addr):
    """The 4-DW header's address pair: DW2 = addr[63:32], DW3 = addr[31:2]."""
    return beat(addr >> 32, addr & 0xFFFFFFFC)


class TlpHost:
    """Plays the PCIe host: drives CQ, collects CC, and services the DUT's
    own RQ/RC traffic against a dict of 8-byte words standing in for host
    memory."""

    def __init__(self, dut):
        self.dut = dut
        self.mem = {}
        self.tag = 0x40

    # -- low-level stream helpers -------------------------------------
    async def _send(self, prefix, beats):
        tdata = getattr(self.dut, f"{prefix}_tdata")
        tlast = getattr(self.dut, f"{prefix}_tlast")
        tvalid = getattr(self.dut, f"{prefix}_tvalid")
        tready = getattr(self.dut, f"{prefix}_tready")
        for i, value in enumerate(beats):
            tdata.value = value
            tlast.value = 1 if i == len(beats) - 1 else 0
            tvalid.value = 1
            while True:
                await ReadOnly()
                accepted = bool(tready.value)
                await RisingEdge(self.dut.clk_i)
                if accepted:
                    break
        tvalid.value = 0
        tlast.value = 0

    async def _recv(self, prefix):
        """Collect one whole packet (up to and including tlast)."""
        tdata = getattr(self.dut, f"{prefix}_tdata")
        tlast = getattr(self.dut, f"{prefix}_tlast")
        tvalid = getattr(self.dut, f"{prefix}_tvalid")
        tready = getattr(self.dut, f"{prefix}_tready")
        tready.value = 1
        beats = []
        while True:
            await ReadOnly()
            valid = bool(tvalid.value)
            if valid:
                beats.append(int(tdata.value))
                last = bool(tlast.value)
            await RisingEdge(self.dut.clk_i)
            if valid and last:
                break
        tready.value = 0
        return beats

    # -- completer side: our requests into the DUT's BAR ---------------
    async def write(self, addr, data, length_dw, first_be=0xF):
        """MemWr TLP. `data` is a list of 64-bit beats."""
        last_be = 0x0 if length_dw == 1 else 0xF
        header = [
            beat(dw0(FMT_4DW_DATA, TYPE_MEM, length_dw),
                 req_dw1(HOST_ID, self.tag, last_be, first_be)),
            addr_beats(addr),
        ]
        self.tag = (self.tag + 1) & 0xFF
        await self._send("cq", header + data)

    async def read(self, addr, length_dw):
        """MemRd TLP, then decode the CplD the DUT sends back. Returns
        (payload beats, tag echoed, byte count reported)."""
        tag = self.tag
        self.tag = (self.tag + 1) & 0xFF
        last_be = 0x0 if length_dw == 1 else 0xF
        header = [
            beat(dw0(FMT_4DW_NODATA, TYPE_MEM, length_dw),
                 req_dw1(HOST_ID, tag, last_be, 0xF)),
            addr_beats(addr),
        ]

        collector = cocotb.start_soon(self._recv("cc"))
        await self._send("cq", header)
        beats = await collector

        assert len(beats) >= 3, f"completion too short: {beats}"
        cpl_dw0 = beats[0] & MASK32
        cpl_dw1 = beats[0] >> 32
        cpl_dw2 = beats[1] & MASK32

        fmt = (cpl_dw0 >> 29) & 0x7
        ttype = (cpl_dw0 >> 24) & 0x1F
        assert (fmt, ttype) == (FMT_3DW_DATA, TYPE_CPL), \
            f"expected a CplD, got fmt={fmt:#05b} type={ttype:#07b}"
        assert (cpl_dw0 & 0x3FF) == length_dw, "completion length mismatch"

        status = (cpl_dw1 >> 13) & 0x7
        assert status == CPL_SUCCESS, f"completion status {status}"
        byte_count = cpl_dw1 & 0xFFF

        assert (cpl_dw2 >> 16) == HOST_ID, \
            "completion did not carry our requester ID back"
        assert ((cpl_dw2 >> 8) & 0xFF) == tag, \
            "completion did not echo our tag"
        assert (cpl_dw2 & 0x7F) == (addr & 0x7F), \
            "completion lower-address field is wrong"

        return beats[2:], tag, byte_count

    # -- convenience wrappers around the register map -------------------
    async def rd32(self, offset):
        payload, _, _ = await self.read(offset, 1)
        return payload[0] & MASK32

    async def rd64(self, offset):
        payload, _, _ = await self.read(offset, 2)
        return payload[0] & MASK64

    async def wr32(self, offset, value):
        await self.write(offset, [value & MASK32], 1)

    async def wr64(self, offset, value):
        await self.write(offset, [value & MASK64], 2)

    # -- requester side: service the DUT's own DMA ----------------------
    async def serve_requester(self):
        """Forever: decode the DUT's RQ packets, apply MemWr to self.mem and
        answer MemRd with a CplD on RC. This is the job
        PcieTlpEngine's requester half does under gem5."""
        while True:
            beats = await self._recv("rq")
            hdr0 = beats[0] & MASK32
            hdr1 = beats[0] >> 32
            addr = ((beats[1] & MASK32) << 32) | ((beats[1] >> 32) & MASK32)
            fmt = (hdr0 >> 29) & 0x7
            length_dw = hdr0 & 0x3FF
            tag = (hdr1 >> 8) & 0xFF
            requester = (hdr1 >> 16) & 0xFFFF

            if fmt & 0b010:  # has data -> MemWr
                self.mem[addr] = beats[2] & MASK64
            else:  # MemRd -> answer with a CplD
                value = self.mem.get(addr, 0)
                cpl = [
                    beat(dw0(FMT_3DW_DATA, TYPE_CPL, length_dw),
                         (DUT_ID << 16) | (CPL_SUCCESS << 13)
                         | ((length_dw << 2) & 0xFFF)),
                    beat((requester << 16) | (tag << 8) | (addr & 0x7F), 0),
                    value,
                ]
                await self._send("rc", cpl)


async def bringup(dut):
    cocotb.start_soon(Clock(dut.clk_i, 10, units="ns").start())
    dut.rst_ni.value = 0
    dut.cq_tvalid.value = 0
    dut.cq_tlast.value = 0
    dut.cc_tready.value = 0
    dut.rq_tready.value = 0
    dut.rc_tvalid.value = 0
    dut.rc_tlast.value = 0
    for _ in range(10):
        await RisingEdge(dut.clk_i)
    dut.rst_ni.value = 1
    await RisingEdge(dut.clk_i)
    return TlpHost(dut)


@cocotb.test()
async def identity_and_scratch(dut):
    """The RTL parses a MemRd header and builds a well-formed CplD: right
    fmt/type, right length, success status, and our requester ID, tag and
    lower address echoed back. That last part is what the AXI4 variant of
    this device never has to get right."""
    host = await bringup(dut)

    assert await host.rd32(REG_ID) == ID_MAGIC
    assert await host.rd32(REG_VERSION) == ID_VERSION

    for value in (0xDEADBEEF, 0x00000000, 0xFFFFFFFF, 0xC0FFEE00):
        await host.wr32(REG_SCRATCH, value)
        assert await host.rd32(REG_SCRATCH) == value, \
            f"scratch did not round-trip {value:#x}"


@cocotb.test()
async def completion_byte_count(dut):
    """A completion's byte-count field has to reflect the request's length,
    not the DUT's internal bus width."""
    host = await bringup(dut)

    _, _, bc1 = await host.read(REG_ID, 1)
    assert bc1 == 4, f"1-DW read reported byte count {bc1}, expected 4"

    _, _, bc2 = await host.read(REG_DMA_SRC, 2)
    assert bc2 == 8, f"2-DW read reported byte count {bc2}, expected 8"


@cocotb.test()
async def counter_advances(dut):
    """The free-running counter proves the DUT's clock is genuinely
    ticking, not just being stepped for the duration of a transaction."""
    host = await bringup(dut)

    first = await host.rd32(REG_COUNTER)
    await ClockCycles(dut.clk_i, 50)
    second = await host.rd32(REG_COUNTER)
    assert second > first, f"counter did not advance ({first} -> {second})"


@cocotb.test()
async def bar_ram_and_byte_enables(dut):
    """The scratchpad RAM, and the header's first-DW byte enables actually
    masking bytes rather than the whole DW being written."""
    host = await bringup(dut)

    for i in range(8):
        await host.wr64(BAR_RAM + i * 8, 0x1000000000000000 + i)
    for i in range(8):
        assert await host.rd64(BAR_RAM + i * 8) == 0x1000000000000000 + i, \
            f"RAM word {i} did not round-trip"

    # Rewrite only the low two bytes of one DW; the other six bytes of that
    # 8-byte word must survive.
    await host.write(BAR_RAM, [0x0000BBAA], 1, first_be=0x3)
    got = await host.rd64(BAR_RAM)
    assert got & 0xFFFF == 0xBBAA, "byte-enabled bytes were not written"
    assert got >> 16 == 0x1000000000000000 >> 16, \
        "first-DW byte enables failed to mask the rest of the word"


async def run_dma(dut, host, src, dst, length, timeout=4000):
    await host.wr64(REG_DMA_SRC, src)
    await host.wr64(REG_DMA_DST, dst)
    await host.wr32(REG_DMA_LEN, length)
    assert await host.rd64(REG_DMA_SRC) == src, "DMA_SRC did not read back"

    # CTRL is one register: OR START in so IRQ_EN survives.
    ctrl = await host.rd32(REG_CTRL)
    await host.wr32(REG_CTRL, ctrl | CTRL_START)

    for _ in range(timeout):
        status = await host.rd32(REG_STATUS)
        if status & STATUS_DONE:
            return status
        await ClockCycles(dut.clk_i, 1)
    raise AssertionError("DMA never signalled DONE")


@cocotb.test()
async def bus_master_dma(dut):
    """The requester direction: the DUT emits its own MemRd/MemWr TLPs,
    parses the CplD that comes back, and copies host memory with them."""
    host = await bringup(dut)
    cocotb.start_soon(host.serve_requester())

    src, dst = 0x2000, 0x3000
    host.mem[src] = 0xA5A5A5A5DEADBEEF
    status = await run_dma(dut, host, src, dst, 8)

    assert status & STATUS_BUSY == 0, "device still busy after DONE"
    assert host.mem.get(dst) == 0xA5A5A5A5DEADBEEF, \
        "DMA did not copy the source word to the destination"

    # A second, multi-word transfer over the same path.
    await host.wr32(REG_IRQ_ACK, 1)
    for i in range(4):
        host.mem[0x4000 + i * 8] = 0x1111111100000000 + i
    await run_dma(dut, host, 0x4000, 0x5000, 32)
    for i in range(4):
        assert host.mem.get(0x5000 + i * 8) == 0x1111111100000000 + i, \
            f"multi-word DMA word {i} landed wrong"


@cocotb.test()
async def interrupt_assert_and_ack(dut):
    """The interrupt pin, which sits outside the TLP streams entirely --
    gem5's RTLPcieTlpDevice turns its edges into INTx assert/deassert."""
    host = await bringup(dut)
    cocotb.start_soon(host.serve_requester())

    assert dut.irq_o.value == 0, "irq asserted out of reset"

    await host.wr32(REG_CTRL, CTRL_IRQ_FORCE)
    assert await host.rd32(REG_STATUS) & STATUS_IRQ_PENDING
    assert dut.irq_o.value == 0, "irq_o must stay low while IRQ_EN is clear"

    await host.wr32(REG_CTRL, CTRL_IRQ_EN)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 1, "enabling IRQ_EN should expose the pending irq"

    await host.wr32(REG_IRQ_ACK, 1)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 0, "IRQ_ACK did not deassert irq_o"

    # A completing DMA raises it again, with IRQ_EN still set.
    host.mem[0x6000] = 0x5A5A5A5A5A5A5A5A
    await run_dma(dut, host, 0x6000, 0x7000, 8)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 1, "DMA completion did not raise irq_o"

    await host.wr32(REG_IRQ_ACK, 1)
    await ClockCycles(dut.clk_i, 2)
    assert dut.irq_o.value == 0
    assert await host.rd32(REG_STATUS) & STATUS_DONE == 0, \
        "IRQ_ACK should clear DONE as well"
