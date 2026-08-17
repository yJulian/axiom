// tb_dma_memcopy.cc
//
// Standalone Verilator testbench for dma_memcopy_top -- independent of
// gem5. Drives the control/status AXI4 slave port to kick off several
// copy channels, and plays the role gem5's memory system (via
// Axi4MasterEngine) would normally play on the DMA AXI4 master port:
// tracks each channel's outstanding read/write as it's issued, and lets
// the test explicitly choose *when* to complete each one -- deliberately
// out of issue order across channels/IDs.
//
// This is the DMA-side analog of tb_fifo_pio.cc: fifo_pio_accel has no
// DMA master port at all (a pure PIO/register slave), so nothing in this
// repo previously exercised more than one AXI4 transaction outstanding
// at once against real RTL. Each channel here uses its own channel index
// as its AXI ID, so starting three channels concurrently gives three
// genuinely concurrent outstanding DMA transactions, tagged by distinct
// IDs -- exactly the scenario Axi4MasterEngine's pickOldestEligibleRead()/
// Write() (src/axi/axi4_master_engine.cc) exists for. Completing them out
// of issue order and checking both the completion (done) flags and the
// actual copied data prove the real RTL correctly matches each
// out-of-order R/B response back to its owning channel by ID -- not just
// that the C++ engine's bookkeeping is correct in isolation (see
// src/axi/axi4_master_engine.test.cc's CrossId*CompleteOutOfOrder* tests
// for that half, against mocked pins).
//
// Built by `make -C examples/dma_memcopy_accel tb`.

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Vdma_memcopy_top.h"
#include "verilated.h"

namespace
{

constexpr uint64_t CH_STRIDE = 0x20;

uint64_t chSrcReg(unsigned ch) { return ch * CH_STRIDE + 0x00; }
uint64_t chDstReg(unsigned ch) { return ch * CH_STRIDE + 0x08; }
uint64_t chCtrlReg(unsigned ch) { return ch * CH_STRIDE + 0x10; }
uint64_t chStatusReg(unsigned ch) { return ch * CH_STRIDE + 0x18; }

struct Pending
{
    uint32_t id;
    uint64_t addr;
    uint64_t data = 0;
};

// Drives dma_memcopy_top's control port as the sole requester (mirroring
// tb_fifo_pio.cc's axiWrite/axiRead), and plays memory-system responder
// on the DMA master port: always ready to accept AW/AR/W (no backpressure
// modeled, matching Axi4MasterEngine's own always-ready AR/AW/W
// acceptance), and records each accepted read/write so the test can
// release its R/B response whenever it chooses via releaseRead/Write.
struct Sim
{
    Vdma_memcopy_top *top;
    std::vector<Pending> pendingReads;
    std::vector<Pending> pendingWrites;
    std::unordered_map<uint64_t, uint64_t> mem;

    void
    step()
    {
        top->m_axi_arready = 1;
        top->m_axi_awready = 1;
        top->m_axi_wready = 1;

        top->clk_i = 0;
        top->eval();

        if (top->m_axi_arvalid && top->m_axi_arready)
            pendingReads.push_back({top->m_axi_arid, top->m_axi_araddr});
        if (top->m_axi_awvalid && top->m_axi_awready &&
            top->m_axi_wvalid && top->m_axi_wready) {
            pendingWrites.push_back(
                {top->m_axi_awid, top->m_axi_awaddr, top->m_axi_wdata});
        }

        top->clk_i = 1;
        top->eval();
        top->clk_i = 0;
        top->eval();
    }

    void
    waitUntil(const std::function<bool()> &pred, unsigned maxCycles,
              const char *what)
    {
        for (unsigned i = 0; i < maxCycles; i++) {
            if (pred())
                return;
            step();
        }
        std::fprintf(stderr, "FAIL: timed out waiting for %s\n", what);
        std::exit(1);
    }

    // Completes channel `id`'s oldest outstanding read -- regardless of
    // when it was issued relative to any other channel's outstanding
    // read, i.e. the test picks the completion order, not this function.
    void
    releaseRead(uint32_t id)
    {
        auto it = std::find_if(pendingReads.begin(), pendingReads.end(),
                                [&](const Pending &p) { return p.id == id; });
        assert(it != pendingReads.end() && "no outstanding read for id");
        uint64_t addr = it->addr;
        pendingReads.erase(it);

        top->m_axi_rvalid = 1;
        top->m_axi_rid = id;
        top->m_axi_rdata = mem[addr];
        top->m_axi_rlast = 1;
        top->m_axi_rresp = 0;
        step();
        top->m_axi_rvalid = 0;
    }

    void
    releaseWrite(uint32_t id)
    {
        auto it = std::find_if(pendingWrites.begin(), pendingWrites.end(),
                                [&](const Pending &p) { return p.id == id; });
        assert(it != pendingWrites.end() && "no outstanding write for id");
        mem[it->addr] = it->data;
        pendingWrites.erase(it);

        top->m_axi_bvalid = 1;
        top->m_axi_bid = id;
        top->m_axi_bresp = 0;
        step();
        top->m_axi_bvalid = 0;
    }

    void
    ctrlWrite(uint64_t addr, uint64_t data, unsigned id)
    {
        top->s_axi_awid = id;
        top->s_axi_awaddr = addr;
        top->s_axi_awlen = 0;
        top->s_axi_awsize = 3;
        top->s_axi_awburst = 1; // INCR
        top->s_axi_awvalid = 1;
        while (!top->s_axi_awready)
            step();
        step(); // handshake edge
        top->s_axi_awvalid = 0;

        top->s_axi_wdata = data;
        top->s_axi_wstrb = 0xff;
        top->s_axi_wlast = 1;
        top->s_axi_wvalid = 1;
        while (!top->s_axi_wready)
            step();
        step(); // handshake edge
        top->s_axi_wvalid = 0;

        top->s_axi_bready = 1;
        while (!top->s_axi_bvalid)
            step();
        assert(top->s_axi_bresp == 0);
        step();
        top->s_axi_bready = 0;
    }

    uint64_t
    ctrlRead(uint64_t addr, unsigned id)
    {
        top->s_axi_arid = id;
        top->s_axi_araddr = addr;
        top->s_axi_arlen = 0;
        top->s_axi_arsize = 3;
        top->s_axi_arburst = 1; // INCR
        top->s_axi_arvalid = 1;
        while (!top->s_axi_arready)
            step();
        step(); // handshake edge
        top->s_axi_arvalid = 0;

        top->s_axi_rready = 1;
        while (!top->s_axi_rvalid)
            step();
        assert(top->s_axi_rresp == 0);
        uint64_t data = top->s_axi_rdata;
        step();
        top->s_axi_rready = 0;
        return data;
    }
};

} // namespace

int
main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    auto *top = new Vdma_memcopy_top;
    Sim sim{top};

    top->rst_ni = 0;
    for (int i = 0; i < 10; i++)
        sim.step();
    top->rst_ni = 1;
    sim.step();

    // Three channels, each copying a distinct source pattern to its own
    // destination address. Channel index doubles as AXI ID.
    const uint64_t src[3] = {0x1000, 0x1010, 0x1020};
    const uint64_t dst[3] = {0x9000, 0x9010, 0x9020};
    const uint64_t pattern[3] = {
        0x1111111111111111ULL, 0x2222222222222222ULL, 0x3333333333333333ULL
    };
    for (int ch = 0; ch < 3; ch++)
        sim.mem[src[ch]] = pattern[ch];

    for (int ch = 0; ch < 3; ch++) {
        sim.ctrlWrite(chSrcReg(ch), src[ch], /*id=*/ch);
        sim.ctrlWrite(chDstReg(ch), dst[ch], /*id=*/ch);
        sim.ctrlWrite(chCtrlReg(ch), 0x1, /*id=*/ch); // start
    }

    // All three channels should now have independently issued their AR
    // (round-robin/fixed-priority arbitration among them only serializes
    // *issuing*, not being outstanding) -- wait until all three reads are
    // pending, tagged by their own channel's AXI ID.
    sim.waitUntil([&] { return sim.pendingReads.size() == 3; }, 200,
                  "3 outstanding reads (one per channel)");
    for (uint32_t id : {0u, 1u, 2u}) {
        bool found = std::any_of(
            sim.pendingReads.begin(), sim.pendingReads.end(),
            [&](const Pending &p) { return p.id == id; });
        assert(found && "expected a pending read tagged with this channel's ID");
    }

    // Complete the reads in a different order than they were issued:
    // channel 2 (issued last) first, then channel 0 (issued first), then
    // channel 1. If the DUT matched R data to a channel by anything other
    // than RID -- e.g. by presentation order -- this would corrupt the
    // copy.
    sim.releaseRead(2);
    sim.releaseRead(0);
    sim.releaseRead(1);

    sim.waitUntil([&] { return sim.pendingWrites.size() == 3; }, 200,
                  "3 outstanding writes (one per channel)");

    // Same idea on the B channel: complete out of issue order.
    sim.releaseWrite(1);
    sim.releaseWrite(2);
    sim.releaseWrite(0);

    // Give the last B response a few cycles to land in ch_state before
    // polling STATUS below.
    for (int i = 0; i < 5; i++)
        sim.step();

    for (int ch = 0; ch < 3; ch++) {
        uint64_t status = sim.ctrlRead(chStatusReg(ch), /*id=*/ch);
        assert((status & 0x1) == 0 && "channel should no longer be busy");
        assert(((status >> 1) & 0x1) == 1 && "channel should be done");
    }

    for (int ch = 0; ch < 3; ch++) {
        assert(sim.mem[dst[ch]] == pattern[ch] &&
               "copied data must match its own channel's source pattern, "
               "even though completions arrived out of order");
    }

    // STATUS reads clear the done bit -- confirm that actually happened.
    uint64_t status0 = sim.ctrlRead(chStatusReg(0), /*id=*/0);
    assert(((status0 >> 1) & 0x1) == 0 &&
           "done should have been cleared by the earlier STATUS read");

    top->final();
    delete top;

    std::printf(
        "PASS: dma_memcopy_top completed 3 concurrent channels "
        "(distinct AXI IDs) with out-of-order R/B completion, all "
        "data landed at the correct destination\n");
    return 0;
}
