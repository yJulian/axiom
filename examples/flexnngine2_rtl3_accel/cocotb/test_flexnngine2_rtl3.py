# test_flexnngine2_rtl3.py
#
# cocotb testbench for flexnngine2_rtl3_top -- the standalone (no-gem5)
# verification of the rtl3 (scratchpad-fed) FleXNNgine2 GEMM accelerator
# ported over from gem5_cva6 (see ../README.md for what "ported" means: the
# RTL itself is untouched, only this harness is new). Drives the AXI4-Lite
# control/status port with an AxiLiteMaster (rtl3's port genuinely has no
# ID field, unlike dma_memcopy_top's control port -- see
# examples/dma_memcopy_accel/cocotb/test_dma_memcopy.py) and plays gem5's
# role on the DMA AXI4 master port with an AxiRam, same pattern as that
# test. Two things are checked against a plain-Python reference GEMM:
#
#   1. a single MESH_PE_ROW x MESH_PE_COL output tile, one job, K=TILE_K.
#   2. a 2-job ACCUM chain (K=2*TILE_K) -- rtl3's actual point: partial
#      sums are pinned in the PEs across jobs, never round-tripped through
#      memory, so this is the one behavior that would break if the chain
#      wiring (CONFIG.ACCUM, CTRL.CLEAR_DONE between jobs) were wrong even
#      though a single-job test already passed.
#
# Run via `make -C examples/flexnngine2_rtl3_accel/cocotb` (needs the venv
# from scripts/setup_cocotb_env.sh, and GEM5_CVA6 pointed at a gem5_cva6
# checkout if it isn't a sibling of this repo -- see the Makefile).

import random

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, RisingEdge
from cocotbext.axi import AxiBus, AxiLiteBus, AxiLiteMaster, AxiRam

# Register map -- ext/flexnngine2_adapter/rtl3/flexnngine2_rtl3_top.sv /
# include/Flexnngine2Rtl3Driver.h in gem5_cva6.
REG_CTRL, REG_STATUS, REG_CONFIG = 0x00, 0x04, 0x08
REG_SRC_A, REG_SRC_B, REG_DST_C = 0x0C, 0x10, 0x14
REG_NUM_CYCLES, REG_MESH_SIZE, REG_SPAD_DEPTH = 0x18, 0x1C, 0x20

CTRL_START, CTRL_CLEAR_DONE = 1 << 0, 1 << 1
STATUS_BUSY, STATUS_DONE = 1 << 0, 1 << 1
STATUS_TIMEOUT_ERR, STATUS_CFG_ERR = 1 << 2, 1 << 3
CFG_WS, CFG_ACCUM, CFG_WRITEBACK = 1 << 0, 1 << 1, 1 << 2

RAM_SIZE = 0x100000


async def reset_dut(dut):
    dut.i_rst_n.value = 0
    for _ in range(10):
        await RisingEdge(dut.i_clk)
    dut.i_rst_n.value = 1
    await RisingEdge(dut.i_clk)


async def reg_write(ctrl, addr, val):
    await ctrl.write(addr, (val & 0xFFFFFFFF).to_bytes(4, "little"))


async def reg_read(ctrl, addr):
    return int.from_bytes((await ctrl.read(addr, 4)).data, "little")


async def wait_done(ctrl, dut, max_cycles=500_000):
    for _ in range(max_cycles):
        status = await reg_read(ctrl, REG_STATUS)
        if status & STATUS_DONE:
            return status
        await ClockCycles(dut.i_clk, 1)
    assert False, "accelerator never signalled DONE"


async def run_job(ctrl, dut, a_addr, b_addr, c_addr, k, accum, writeback):
    await reg_write(ctrl, REG_SRC_A, a_addr)
    await reg_write(ctrl, REG_SRC_B, b_addr)
    await reg_write(ctrl, REG_DST_C, c_addr)
    await reg_write(ctrl, REG_NUM_CYCLES, k)
    cfg = (CFG_ACCUM if accum else 0) | (CFG_WRITEBACK if writeback else 0)
    await reg_write(ctrl, REG_CONFIG, cfg)
    await reg_write(ctrl, REG_CTRL, CTRL_START)

    status = await wait_done(ctrl, dut)
    assert not status & STATUS_TIMEOUT_ERR, "unexpected TIMEOUT_ERROR"
    assert not status & STATUS_CFG_ERR, "unexpected CONFIG_ERROR"
    await reg_write(ctrl, REG_CTRL, CTRL_CLEAR_DONE)


def gemm_ref(a, b, rows, cols, k):
    """Plain-Python reference: a is rows x k, b is k x cols, both int8."""
    c = [[0] * cols for _ in range(rows)]
    for r in range(rows):
        for cc in range(cols):
            c[r][cc] = sum(a[r][t] * b[t][cc] for t in range(k))
    return c


def stage_tile_a(ram, addr, a, rows, k):
    """SRC_A layout: entry t is A[:,t] (column-major)."""
    buf = bytearray(k * rows)
    for t in range(k):
        for r in range(rows):
            buf[t * rows + r] = a[r][t] & 0xFF
    ram.write(addr, bytes(buf))


def stage_tile_b(ram, addr, b, cols, k):
    """SRC_B layout: entry t is B[t,:] (row-major)."""
    buf = bytearray(k * cols)
    for t in range(k):
        for c in range(cols):
            buf[t * cols + c] = b[t][c] & 0xFF
    ram.write(addr, bytes(buf))


def read_tile_c(ram, addr, rows, cols):
    raw = ram.read(addr, rows * cols * 4)
    c = [[0] * cols for _ in range(rows)]
    for r in range(rows):
        for cc in range(cols):
            off = (r * cols + cc) * 4
            c[r][cc] = int.from_bytes(raw[off:off + 4], "little", signed=True)
    return c


@cocotb.test()
async def single_tile_gemm(dut):
    """One MESH_PE_ROW x MESH_PE_COL output tile, K = TILE_K, no ACCUM
    chaining -- the baseline case."""
    cocotb.start_soon(Clock(dut.i_clk, 10, units="ns").start())
    await reset_dut(dut)

    ctrl = AxiLiteMaster(AxiLiteBus.from_prefix(dut, "s_axi"), dut.i_clk,
                          dut.i_rst_n, reset_active_level=False)
    ram = AxiRam(AxiBus.from_prefix(dut, "m_axi"), dut.i_clk, dut.i_rst_n,
                 reset_active_level=False, size=RAM_SIZE)

    rows = await reg_read(ctrl, REG_MESH_SIZE) >> 16
    cols = await reg_read(ctrl, REG_MESH_SIZE) & 0xFFFF
    tile_k = await reg_read(ctrl, REG_SPAD_DEPTH)
    dut._log.info(f"mesh {rows}x{cols}, tile_k={tile_k}")

    rng = random.Random(0xC0FFEE)
    a = [[rng.randint(-128, 127) for _ in range(tile_k)] for _ in range(rows)]
    b = [[rng.randint(-128, 127) for _ in range(cols)] for _ in range(tile_k)]
    expect = gemm_ref(a, b, rows, cols, tile_k)

    a_addr, b_addr, c_addr = 0x1000, 0x2000, 0x3000
    ram.write(c_addr, bytes(rows * cols * 4))  # zero the destination first
    stage_tile_a(ram, a_addr, a, rows, tile_k)
    stage_tile_b(ram, b_addr, b, cols, tile_k)

    await run_job(ctrl, dut, a_addr, b_addr, c_addr, tile_k,
                  accum=False, writeback=True)

    got = read_tile_c(ram, c_addr, rows, cols)
    assert got == expect, f"single-tile GEMM mismatch: got {got} expect {expect}"


@cocotb.test()
async def accum_chain_gemm(dut):
    """Two jobs chained via CONFIG.ACCUM -- covers K = 2*TILE_K without ever
    round-tripping the partial sum through memory: only the second job has
    CONFIG.WRITEBACK set, so the first job's result exists only inside the
    PEs' accumulators until the second job's compute phase adds into it."""
    cocotb.start_soon(Clock(dut.i_clk, 10, units="ns").start())
    await reset_dut(dut)

    ctrl = AxiLiteMaster(AxiLiteBus.from_prefix(dut, "s_axi"), dut.i_clk,
                          dut.i_rst_n, reset_active_level=False)
    ram = AxiRam(AxiBus.from_prefix(dut, "m_axi"), dut.i_clk, dut.i_rst_n,
                 reset_active_level=False, size=RAM_SIZE)

    rows = await reg_read(ctrl, REG_MESH_SIZE) >> 16
    cols = await reg_read(ctrl, REG_MESH_SIZE) & 0xFFFF
    tile_k = await reg_read(ctrl, REG_SPAD_DEPTH)

    rng = random.Random(0xDECAF)
    a0 = [[rng.randint(-128, 127) for _ in range(tile_k)] for _ in range(rows)]
    b0 = [[rng.randint(-128, 127) for _ in range(cols)] for _ in range(tile_k)]
    a1 = [[rng.randint(-128, 127) for _ in range(tile_k)] for _ in range(rows)]
    b1 = [[rng.randint(-128, 127) for _ in range(cols)] for _ in range(tile_k)]
    a_full = [a0[r] + a1[r] for r in range(rows)]
    b_full = b0 + b1
    expect = gemm_ref(a_full, b_full, rows, cols, 2 * tile_k)

    a0_addr, b0_addr = 0x1000, 0x2000
    a1_addr, b1_addr = 0x3000, 0x4000
    c_addr = 0x5000
    ram.write(c_addr, bytes(rows * cols * 4))
    stage_tile_a(ram, a0_addr, a0, rows, tile_k)
    stage_tile_b(ram, b0_addr, b0, cols, tile_k)
    stage_tile_a(ram, a1_addr, a1, rows, tile_k)
    stage_tile_b(ram, b1_addr, b1, cols, tile_k)

    await run_job(ctrl, dut, a0_addr, b0_addr, c_addr, tile_k,
                  accum=False, writeback=False)
    await run_job(ctrl, dut, a1_addr, b1_addr, c_addr, tile_k,
                  accum=True, writeback=True)

    got = read_tile_c(ram, c_addr, rows, cols)
    assert got == expect, f"ACCUM chain GEMM mismatch: got {got} expect {expect}"
