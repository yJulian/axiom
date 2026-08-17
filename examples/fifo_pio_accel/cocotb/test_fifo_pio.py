# test_fifo_pio.py
#
# cocotb testbench for fifo_pio_top -- the standalone (no-gem5)
# verification of this DUT's AXI4 protocol logic, via cocotb +
# cocotbext-axi's AxiMaster instead of hand-rolled beat-by-beat C++.
# Independent of gem5. Run via `make -C examples/fifo_pio_accel/cocotb`
# (needs the venv from scripts/setup_cocotb_env.sh on PATH -- see that
# directory's Makefile, or just `make tb` at the repo root).

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge
from cocotbext.axi import AxiBus, AxiMaster

STATUS_ADDR = 0x00
FIFO_DATA_ADDR = 0x08


async def reset_dut(dut):
    dut.rst_ni.value = 0
    for _ in range(10):
        await RisingEdge(dut.clk_i)
    dut.rst_ni.value = 1
    await RisingEdge(dut.clk_i)


def as_u64(resp_data):
    return int.from_bytes(resp_data, "little")


@cocotb.test()
async def push_pop_roundtrip(dut):
    """Push one value over a full AXI4 write burst, pop it back over a
    full AXI4 read burst, and check it round-trips -- proving the write
    burst -> RTL state -> read burst path works end to end. Distinct
    AWID/ARID exercise the ID field itself (echoed back on B/R), not just
    the data path."""
    cocotb.start_soon(Clock(dut.clk_i, 10, units="ns").start())
    await reset_dut(dut)

    axi = AxiMaster(AxiBus.from_prefix(dut, "s_axi"), dut.clk_i, dut.rst_ni,
                     reset_active_level=False)

    status = as_u64((await axi.read(STATUS_ADDR, 8, arid=0)).data)
    assert status & 0x1 == 1, "expected empty on reset"
    assert (status >> 1) & 0x1 == 0, "expected not-full on reset"

    pushed = 0xDEADBEEFCAFEF00D
    await axi.write(FIFO_DATA_ADDR, pushed.to_bytes(8, "little"), awid=5)

    status = as_u64((await axi.read(STATUS_ADDR, 8, arid=1)).data)
    assert status & 0x1 == 0, "expected not-empty after push"

    popped = as_u64((await axi.read(FIFO_DATA_ADDR, 8, arid=7)).data)
    assert popped == pushed, "popped value must match what was pushed"

    status = as_u64((await axi.read(STATUS_ADDR, 8, arid=2)).data)
    assert status & 0x1 == 1, "expected empty again after pop"
