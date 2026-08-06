/*
 * Unit tests for Axi4SlaveEngine: gem5-Packet-coupled by design (it turns
 * PacketPtrs into AXI4 bursts and back), so these run as gem5 gtests
 * (`scons build/RISCV/unittests.opt`) rather than through the RTL-free
 * `make tb` path, which bypasses this engine entirely by driving Verilator
 * pins directly.
 */

#include <gtest/gtest.h>

#include <functional>
#include <memory>

#include "axi/axi4_slave_engine.hh"
#include "base/gtest/cur_tick_fake.hh"
#include "mem/packet.hh"
#include "mem/request.hh"

namespace gem5
{
namespace axi4_slave_engine_test
{

GTestTickHandler tickHandler;

// Every handshake completes on the first cycle it's asserted -- there's no
// real RTL timing to model here, just the engine's own state-machine and
// response-code logic. bResp_/rRespSequence_ let each test script the
// response the "RTL" would have returned.
class MockSlavePins : public axion::Axi4SlavePins
{
  public:
    void axiSetClk(uint8_t) override {}
    void axiSetRstN(uint8_t) override {}
    void axiEval() override {}

    void axiSlaveSetAwId(axion::AxiId id) override { awId_ = id; }
    void axiSlaveSetAwAddr(Addr addr) override { awAddr_ = addr; }
    void axiSlaveSetAwLen(uint8_t len) override { awLen_ = len; }
    void axiSlaveSetAwSize(uint8_t size) override { awSize_ = size; }
    void axiSlaveSetAwBurst(uint8_t burst) override { awBurst_ = burst; }
    void axiSlaveSetAwLock(uint8_t lock) override { awLock_ = lock; }
    void axiSlaveSetAwCache(uint8_t cache) override { awCache_ = cache; }
    void axiSlaveSetAwProt(uint8_t prot) override { awProt_ = prot; }
    void axiSlaveSetAwQos(uint8_t qos) override { awQos_ = qos; }
    void axiSlaveSetAwRegion(uint8_t region) override { awRegion_ = region; }
    void axiSlaveSetAwValid(uint8_t val) override { awValid_ = val; }
    uint8_t axiSlaveGetAwReady() override { return 1; }

    void axiSlaveSetWData(uint64_t) override {}
    void axiSlaveSetWStrb(uint64_t) override {}
    void axiSlaveSetWLast(uint8_t) override {}
    void axiSlaveSetWValid(uint8_t) override {}
    uint8_t axiSlaveGetWReady() override { return 1; }

    axion::AxiId axiSlaveGetBId() override { return 0; }
    uint8_t axiSlaveGetBResp() override { return bResp; }
    uint8_t axiSlaveGetBValid() override { return 1; }
    void axiSlaveSetBReady(uint8_t) override {}

    void axiSlaveSetArId(axion::AxiId id) override { arId_ = id; }
    void axiSlaveSetArAddr(Addr addr) override { arAddr_ = addr; }
    void axiSlaveSetArLen(uint8_t len) override { arLen_ = len; }
    void axiSlaveSetArSize(uint8_t size) override { arSize_ = size; }
    void axiSlaveSetArBurst(uint8_t burst) override { arBurst_ = burst; }
    void axiSlaveSetArLock(uint8_t lock) override { arLock_ = lock; }
    void axiSlaveSetArCache(uint8_t cache) override { arCache_ = cache; }
    void axiSlaveSetArProt(uint8_t prot) override { arProt_ = prot; }
    void axiSlaveSetArQos(uint8_t qos) override { arQos_ = qos; }
    void axiSlaveSetArRegion(uint8_t region) override { arRegion_ = region; }
    void axiSlaveSetArValid(uint8_t val) override { arValid_ = val; }
    uint8_t axiSlaveGetArReady() override { return 1; }

    axion::AxiId axiSlaveGetRId() override { return 0; }
    uint64_t axiSlaveGetRData() override { return 0; }
    uint8_t
    axiSlaveGetRResp() override
    {
        uint8_t resp = rRespSequence.empty() ? 0 : rRespSequence.front();
        if (rRespSequence.size() > 1)
            rRespSequence.erase(rRespSequence.begin());
        return resp;
    }
    uint8_t axiSlaveGetRLast() override { return 0; }
    uint8_t axiSlaveGetRValid() override { return 1; }
    void axiSlaveSetRReady(uint8_t) override {}

    // Scripted responses.
    uint8_t bResp = 0;
    std::vector<uint8_t> rRespSequence{0};

    // Last-driven AW/AR channel values, for the pin-driving assertions.
    axion::AxiId awId_ = 0, arId_ = 0;
    Addr awAddr_ = 0, arAddr_ = 0;
    uint8_t awLen_ = 0, awSize_ = 0, awBurst_ = 0;
    uint8_t awLock_ = 0, awCache_ = 0, awProt_ = 0, awQos_ = 0, awRegion_ = 0;
    uint8_t awValid_ = 0;
    uint8_t arLen_ = 0, arSize_ = 0, arBurst_ = 0;
    uint8_t arLock_ = 0, arCache_ = 0, arProt_ = 0, arQos_ = 0, arRegion_ = 0;
    uint8_t arValid_ = 0;
};

PacketPtr
makeReadPkt(Addr addr, unsigned size, Request::Flags flags = 0)
{
    RequestPtr req = std::make_shared<Request>(addr, size, flags,
                                                Request::funcRequestorId);
    PacketPtr pkt = Packet::createRead(req);
    pkt->allocate();
    return pkt;
}

PacketPtr
makeWritePkt(Addr addr, unsigned size, Request::Flags flags = 0)
{
    RequestPtr req = std::make_shared<Request>(addr, size, flags,
                                                Request::funcRequestorId);
    PacketPtr pkt = Packet::createWrite(req);
    pkt->allocate();
    std::vector<uint8_t> data(size, 0xab);
    pkt->setData(data.data());
    return pkt;
}

// Runs the engine's tick() loop until the transaction completes (onDone
// fires) or a cycle budget is exhausted, to avoid ever hanging the test.
void
runToCompletion(axion::Axi4SlaveEngine &engine, PacketPtr pkt)
{
    bool done = false;
    ASSERT_TRUE(engine.issue(pkt, [&done] { done = true; }));
    for (int i = 0; i < 64 && !done; i++)
        engine.tick();
    ASSERT_TRUE(done);
}

TEST(Axi4SlaveEngineTest, OkayWriteIsNotError)
{
    MockSlavePins pins;
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeWritePkt(0x100, 8);
    runToCompletion(engine, pkt);
    EXPECT_FALSE(pkt->isError());
    delete pkt;
}

TEST(Axi4SlaveEngineTest, OkayReadIsNotError)
{
    MockSlavePins pins;
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeReadPkt(0x100, 8);
    runToCompletion(engine, pkt);
    EXPECT_FALSE(pkt->isError());
    delete pkt;
}

TEST(Axi4SlaveEngineTest, SlvErrOnWriteSetsBadCommand)
{
    MockSlavePins pins;
    pins.bResp = static_cast<uint8_t>(axion::AxiResp::SlvErr);
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeWritePkt(0x100, 8);
    runToCompletion(engine, pkt);
    EXPECT_TRUE(pkt->isError());
    EXPECT_EQ(pkt->cmd, MemCmd::WriteError);
    delete pkt;
}

TEST(Axi4SlaveEngineTest, DecErrOnNonFinalReadBeatIsWorstAcrossBurst)
{
    MockSlavePins pins;
    // 24 bytes / 8-byte beats = 3 beats; DECERR on the middle beat must
    // still mark the whole transfer erroneous once the last beat lands.
    pins.rRespSequence = {static_cast<uint8_t>(axion::AxiResp::Okay),
                           static_cast<uint8_t>(axion::AxiResp::DecErr),
                           static_cast<uint8_t>(axion::AxiResp::Okay)};
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeReadPkt(0x100, 24);
    runToCompletion(engine, pkt);
    EXPECT_TRUE(pkt->isError());
    EXPECT_EQ(pkt->cmd, MemCmd::BadAddressError);
    delete pkt;
}

TEST(Axi4SlaveEngineTest, DrivesAwAttrsFromRequestFlags)
{
    MockSlavePins pins;
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeWritePkt(0x100, 8,
                                  Request::LOCKED_RMW | Request::UNCACHEABLE |
                                      Request::PRIVILEGED);
    pkt->qosValue(5);
    runToCompletion(engine, pkt);
    EXPECT_EQ(pins.awLock_, 1);
    EXPECT_EQ(pins.awCache_, 0x0); // uncacheable -> Device Non-bufferable
    EXPECT_EQ(pins.awProt_ & 0x1, 1); // privileged
    EXPECT_EQ(pins.awProt_ & 0x2, 0x2); // non-secure (SECURE flag unset)
    EXPECT_EQ(pins.awQos_, 5);
    delete pkt;
}

TEST(Axi4SlaveEngineTest, DrivesArAttrsFromRequestFlags)
{
    MockSlavePins pins;
    axion::Axi4SlaveEngine engine(pins);
    PacketPtr pkt = makeReadPkt(0x100, 8, Request::SECURE);
    pkt->qosValue(3);
    runToCompletion(engine, pkt);
    EXPECT_EQ(pins.arLock_, 0);
    EXPECT_EQ(pins.arCache_, 0x3); // cacheable path -> Normal Non-cache Buf.
    EXPECT_EQ(pins.arProt_ & 0x2, 0); // secure
    EXPECT_EQ(pins.arQos_, 3);
    delete pkt;
}

} // namespace axi4_slave_engine_test
} // namespace gem5
