# test_dma_memcopy.py
#
# cocotb testbench for dma_memcopy_top -- the standalone (no-gem5)
# verification of this DUT's control port *and* its DMA master port.
# Drives the control/status AXI4 slave port with an AxiMaster, same as
# test_fifo_pio.py, and plays gem5's role on the DMA AXI4 master port
# with an AxiRam memory model instead of hand-rolled pending-transaction
# bookkeeping -- AxiRam services the three channels' concurrent
# outstanding read/write bursts itself, each tagged by its own channel's
# AXI ID, which is what actually exercises the RTL's ID-based
# R/B-to-channel demux (see dma_memcopy.sv's header comment). Independent
# of gem5. Run via `make -C examples/dma_memcopy_accel/cocotb` (needs the
# venv from scripts/setup_cocotb_env.sh -- see that directory's Makefile,
# and examples/fifo_pio_accel/cocotb/Makefile for the same pattern, or
# just `make tb-dma` at the repo root).

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, RisingEdge
from cocotbext.axi import AxiBus, AxiMaster, AxiRam

CH_STRIDE = 0x20
NUM_CH = 3
RAM_SIZE = 0x10000


def src_reg(ch):
    return ch * CH_STRIDE + 0x00


def dst_reg(ch):
    return ch * CH_STRIDE + 0x08


def ctrl_reg(ch):
    return ch * CH_STRIDE + 0x10


def status_reg(ch):
    return ch * CH_STRIDE + 0x18


async def reset_dut(dut):
    dut.rst_ni.value = 0
    for _ in range(10):
        await RisingEdge(dut.clk_i)
    dut.rst_ni.value = 1
    await RisingEdge(dut.clk_i)


def as_u64(resp_data):
    return int.from_bytes(resp_data, "little")


@cocotb.test()
async def three_channel_concurrent_copy(dut):
    """Start three channels concurrently, each with its own channel index
    as its AXI ID (matching dma_memcopy.sv), let the AxiRam memory model
    service their genuinely-concurrent outstanding DMA reads/writes, and
    confirm each channel's data landed at its own destination -- not some
    other channel's, which is exactly what would go wrong if the DUT
    mismatched R/B responses to channels by anything other than ID."""
    cocotb.start_soon(Clock(dut.clk_i, 10, units="ns").start())
    await reset_dut(dut)

    ctrl = AxiMaster(AxiBus.from_prefix(dut, "s_axi"), dut.clk_i, dut.rst_ni,
                      reset_active_level=False)
    ram = AxiRam(AxiBus.from_prefix(dut, "m_axi"), dut.clk_i, dut.rst_ni,
                 reset_active_level=False, size=RAM_SIZE)

    src = [0x1000, 0x1010, 0x1020]
    dst = [0x9000, 0x9010, 0x9020]
    pattern = [0x1111111111111111, 0x2222222222222222, 0x3333333333333333]

    for ch in range(NUM_CH):
        ram.write(src[ch], pattern[ch].to_bytes(8, "little"))

    for ch in range(NUM_CH):
        await ctrl.write(src_reg(ch), src[ch].to_bytes(8, "little"), awid=ch)
        await ctrl.write(dst_reg(ch), dst[ch].to_bytes(8, "little"), awid=ch)
        await ctrl.write(ctrl_reg(ch), (1).to_bytes(8, "little"), awid=ch)

    # Poll each channel's STATUS until its done bit is set -- three
    # channels started back to back above, each with its own AXI ID, so
    # their DMA reads/writes to the shared AxiRam are genuinely
    # outstanding concurrently, not serialized by this loop.
    for ch in range(NUM_CH):
        status = 0
        for _ in range(2000):
            status = as_u64((await ctrl.read(status_reg(ch), 8, arid=ch)).data)
            if status & 0x2:
                break
            await ClockCycles(dut.clk_i, 1)
        else:
            assert False, f"channel {ch} never finished"
        assert status & 0x1 == 0, "channel should no longer be busy"

    for ch in range(NUM_CH):
        got = as_u64(ram.read(dst[ch], 8))
        assert got == pattern[ch], (
            f"channel {ch}'s data must match its own source pattern, "
            "even with three channels' DMA bursts genuinely concurrent"
        )

    # STATUS reads clear the done bit -- confirm that actually happened.
    status0 = as_u64((await ctrl.read(status_reg(0), 8, arid=0)).data)
    assert (status0 >> 1) & 0x1 == 0, \
        "done should have been cleared by the earlier STATUS read"
