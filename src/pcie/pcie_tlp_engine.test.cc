/*
 * Unit tests for PcieTlpCompleterEngine / PcieTlpRequesterEngine. Like the
 * AXI4 engine tests these are gem5-Packet-coupled by design, so they run as
 * gem5 gtests (`scons build/RISCV/unittests.opt`) rather than through the
 * RTL-free `make tb-pcie-tlp` path, which exercises the RTL's own parser
 * and bypasses these engines entirely.
 *
 * The mock pins below are a loopback: whatever the completer engine drives
 * onto CQ is captured, and the test then hands back the completion beats a
 * device would have produced. That keeps these tests about the *engine's*
 * header encoding and decoding -- the RTL's half of the same protocol is
 * covered by the cocotb testbench, written against an independent
 * implementation of the layout.
 */

#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <vector>

#include "base/gtest/cur_tick_fake.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "pcie/pcie_tlp_engine.hh"

namespace gem5
{
namespace pcie_tlp_engine_test
{

GTestTickHandler tickHandler;

constexpr uint16_t kRequesterId = 0x0100;
constexpr uint16_t kCompleterId = 0x0008;

// Every handshake completes on the cycle it is asserted; there is no real
// RTL timing to model here, only the engines' own state machines.
//
// Beats are consumed on the *rising clock edge*, not when tready is
// driven. That distinction matters: tick() drives inputs, evals, samples,
// and only then toggles the clock -- so a mock that popped its queue
// inside ccSetTReady() would hand the engine the following beat to sample,
// exactly one packet ahead of reality.
class MockCompleterPins : public axion::PcieTlpCompleterPins
{
  public:
    void
    tlpSetClk(uint8_t val) override
    {
        if (val && ccReady && !ccBeats.empty())
            ccBeats.pop_front();
    }
    void tlpSetRstN(uint8_t) override {}
    void tlpEval() override {}

    void
    cqSetTData(uint64_t data) override
    {
        cqData = data;
    }
    void cqSetTLast(uint8_t val) override { cqLast = val; }
    void
    cqSetTValid(uint8_t val) override
    {
        // driveInputs() calls cqSetTData then cqSetTValid exactly once per
        // tick, and ready is always high here, so recording on every
        // asserted call is exactly one record per accepted beat.
        if (val) {
            cqBeats.push_back(cqData);
            cqLasts.push_back(cqLast);
        }
    }
    uint8_t cqGetTReady() override { return 1; }

    uint64_t
    ccGetTData() override
    {
        return ccBeats.empty() ? 0 : ccBeats.front();
    }
    uint8_t
    ccGetTLast() override
    {
        return ccBeats.size() == 1 ? 1 : 0;
    }
    uint8_t ccGetTValid() override { return !ccBeats.empty(); }
    void ccSetTReady(uint8_t val) override { ccReady = val; }

    // Beats the engine drove onto CQ. cqSetTData runs before cqSetTValid in
    // driveInputs(), so the recorded data is the beat's own.
    std::vector<uint64_t> cqBeats;
    std::vector<uint8_t> cqLasts;
    // Completion beats the test scripts for the engine to consume.
    std::deque<uint64_t> ccBeats;

  private:
    uint64_t cqData = 0;
    uint8_t cqLast = 0;
    uint8_t ccReady = 0;
};

class MockRequesterPins : public axion::PcieTlpRequesterPins
{
  public:
    void
    tlpSetClk(uint8_t val) override
    {
        if (val && rqReady && !rqBeats.empty())
            rqBeats.pop_front();
    }
    void tlpSetRstN(uint8_t) override {}
    void tlpEval() override {}

    uint64_t
    rqGetTData() override
    {
        return rqBeats.empty() ? 0 : rqBeats.front();
    }
    uint8_t
    rqGetTLast() override
    {
        return rqBeats.size() == 1 ? 1 : 0;
    }
    uint8_t rqGetTValid() override { return !rqBeats.empty(); }
    void rqSetTReady(uint8_t val) override { rqReady = val; }

    void rcSetTData(uint64_t data) override { rcData = data; }
    void rcSetTLast(uint8_t val) override { rcLast = val; }
    void
    rcSetTValid(uint8_t val) override
    {
        if (val) {
            rcBeats.push_back(rcData);
            rcLasts.push_back(rcLast);
        }
    }
    uint8_t rcGetTReady() override { return 1; }

    std::deque<uint64_t> rqBeats;
    std::vector<uint64_t> rcBeats;
    std::vector<uint8_t> rcLasts;

  private:
    uint64_t rcData = 0;
    uint8_t rcLast = 0;
    uint8_t rqReady = 0;
};

class RecordingBackend : public axion::PcieTlpRequesterEngine::Backend
{
  public:
    struct Op
    {
        bool isWrite;
        uint64_t seq;
        Addr addr;
        unsigned size;
        std::vector<uint8_t> data;
    };

    void
    issueRead(uint64_t seq, Addr addr, unsigned size) override
    {
        ops.push_back({false, seq, addr, size, {}});
    }

    void
    issueWrite(uint64_t seq, Addr addr, unsigned size,
               const uint8_t *data) override
    {
        ops.push_back({true, seq, addr, size,
                       std::vector<uint8_t>(data, data + size)});
    }

    std::vector<Op> ops;
};

PacketPtr
makeReadPkt(Addr addr, unsigned size)
{
    RequestPtr req = std::make_shared<Request>(addr, size, 0,
                                                Request::funcRequestorId);
    PacketPtr pkt = Packet::createRead(req);
    pkt->allocate();
    return pkt;
}

PacketPtr
makeWritePkt(Addr addr, const std::vector<uint8_t> &bytes)
{
    RequestPtr req = std::make_shared<Request>(addr, bytes.size(), 0,
                                                Request::funcRequestorId);
    PacketPtr pkt = Packet::createWrite(req);
    pkt->allocate();
    std::memcpy(pkt->getPtr<uint8_t>(), bytes.data(), bytes.size());
    return pkt;
}

/** Drive the engine until it goes idle, or give up. */
void
runCompleter(axion::PcieTlpCompleterEngine &engine, unsigned limit = 32)
{
    for (unsigned i = 0; i < limit && engine.busy(); i++)
        engine.tick();
}

// --- Completer engine ------------------------------------------------

TEST(PcieTlpCompleterEngineTest, WriteEmits4DwMemWrHeaderAndIsPosted)
{
    MockCompleterPins pins;
    axion::PcieTlpCompleterEngine engine(pins, kRequesterId);

    PacketPtr pkt = makeWritePkt(0x1000, {0x11, 0x22, 0x33, 0x44});
    bool done = false;
    ASSERT_TRUE(engine.issue(pkt, 0x40, [&done] { done = true; }));
    runCompleter(engine);

    ASSERT_EQ(pins.cqBeats.size(), 3u);

    const uint32_t dw0 = axion::tlp::beatLowDw(pins.cqBeats[0]);
    EXPECT_EQ(axion::tlp::dw0Fmt(dw0), axion::TlpFmt::Hdr4Data);
    EXPECT_EQ(axion::tlp::dw0Type(dw0), axion::TlpType::Mem);
    EXPECT_EQ(axion::tlp::dw0LengthDw(dw0), 1u);

    const uint32_t dw1 = axion::tlp::beatHighDw(pins.cqBeats[0]);
    EXPECT_EQ(axion::tlp::reqDw1RequesterId(dw1), kRequesterId);
    EXPECT_EQ(axion::tlp::reqDw1FirstBe(dw1), 0xfu);
    // A single-DW request must report zero last-DW byte enables.
    EXPECT_EQ(axion::tlp::reqDw1LastBe(dw1), 0u);

    // DW2 = address[63:32], DW3 = address[31:2] << 2.
    EXPECT_EQ(axion::tlp::beatLowDw(pins.cqBeats[1]), 0u);
    EXPECT_EQ(axion::tlp::beatHighDw(pins.cqBeats[1]), 0x40u);

    EXPECT_EQ(axion::tlp::beatLowDw(pins.cqBeats[2]), 0x44332211u);
    EXPECT_EQ(pins.cqLasts[2], 1u);

    // Posted: answered without any completion being fed back.
    EXPECT_TRUE(done);
    EXPECT_TRUE(pkt->isResponse());
    delete pkt;
}

TEST(PcieTlpCompleterEngineTest, ReadEmitsMemRdAndCapturesCompletionPayload)
{
    MockCompleterPins pins;
    axion::PcieTlpCompleterEngine engine(pins, kRequesterId);

    PacketPtr pkt = makeReadPkt(0x2000, 8);
    bool done = false;
    ASSERT_TRUE(engine.issue(pkt, 0x20, [&done] { done = true; }));

    // Two header beats go out first; run just far enough to emit them.
    engine.tick();
    engine.tick();
    ASSERT_GE(pins.cqBeats.size(), 2u);

    const uint32_t dw0 = axion::tlp::beatLowDw(pins.cqBeats[0]);
    EXPECT_EQ(axion::tlp::dw0Fmt(dw0), axion::TlpFmt::Hdr4NoData);
    EXPECT_EQ(axion::tlp::dw0LengthDw(dw0), 2u);
    // A read's request TLP has no payload, so its last header beat is last.
    EXPECT_EQ(pins.cqLasts[1], 1u);

    const uint8_t tag =
        axion::tlp::reqDw1Tag(axion::tlp::beatHighDw(pins.cqBeats[0]));

    pins.ccBeats.push_back(axion::tlp::beat(
        axion::tlp::makeDw0(axion::TlpFmt::Hdr3Data, axion::TlpType::Cpl, 2),
        axion::tlp::makeCplDw1(kCompleterId, axion::TlpCplStatus::Success,
                               8)));
    pins.ccBeats.push_back(axion::tlp::beat(
        axion::tlp::makeCplDw2(kRequesterId, tag, 0x20), 0));
    pins.ccBeats.push_back(0xDEADBEEFCAFEF00Dull);

    runCompleter(engine);

    EXPECT_TRUE(done);
    ASSERT_TRUE(pkt->isResponse());
    uint64_t got = 0;
    std::memcpy(&got, pkt->getConstPtr<uint8_t>(), 8);
    EXPECT_EQ(got, 0xDEADBEEFCAFEF00Dull);
    delete pkt;
}

TEST(PcieTlpCompleterEngineTest, UnsupportedRequestStatusMarksBadAddress)
{
    MockCompleterPins pins;
    axion::PcieTlpCompleterEngine engine(pins, kRequesterId);

    PacketPtr pkt = makeReadPkt(0x3000, 4);
    ASSERT_TRUE(engine.issue(pkt, 0x8, [] {}));
    engine.tick();
    engine.tick();

    const uint8_t tag =
        axion::tlp::reqDw1Tag(axion::tlp::beatHighDw(pins.cqBeats[0]));
    pins.ccBeats.push_back(axion::tlp::beat(
        axion::tlp::makeDw0(axion::TlpFmt::Hdr3Data, axion::TlpType::Cpl, 1),
        axion::tlp::makeCplDw1(kCompleterId,
                               axion::TlpCplStatus::UnsupportedRequest, 4)));
    pins.ccBeats.push_back(axion::tlp::beat(
        axion::tlp::makeCplDw2(kRequesterId, tag, 0x8), 0));
    pins.ccBeats.push_back(0);

    runCompleter(engine);

    // Packet::setBadAddress() rewrites the command, and only works on a
    // packet that is already a response -- so this also pins down that the
    // engine applies the status *after* makeResponse().
    EXPECT_EQ(pkt->cmd, MemCmd::BadAddressError);
    delete pkt;
}

// --- Requester engine ------------------------------------------------

TEST(PcieTlpRequesterEngineTest, DecodesMemWrIntoABackendWrite)
{
    MockRequesterPins pins;
    RecordingBackend backend;
    axion::PcieTlpRequesterEngine engine(pins, backend, kCompleterId);

    pins.rqBeats.push_back(axion::tlp::beat(
        axion::tlp::makeDw0(axion::TlpFmt::Hdr4Data, axion::TlpType::Mem, 2),
        axion::tlp::makeReqDw1(kCompleterId, 0x11, 0xf, 0xf)));
    pins.rqBeats.push_back(axion::tlp::beat(0, 0x80001000));
    pins.rqBeats.push_back(0x0102030405060708ull);

    for (int i = 0; i < 4; i++) {
        engine.tick();
    }

    ASSERT_EQ(backend.ops.size(), 1u);
    EXPECT_TRUE(backend.ops[0].isWrite);
    EXPECT_EQ(backend.ops[0].addr, 0x80001000u);
    EXPECT_EQ(backend.ops[0].size, 8u);
    uint64_t got = 0;
    std::memcpy(&got, backend.ops[0].data.data(), 8);
    EXPECT_EQ(got, 0x0102030405060708ull);
}

TEST(PcieTlpRequesterEngineTest, MemRdIsAnsweredWithATaggedCplD)
{
    MockRequesterPins pins;
    RecordingBackend backend;
    axion::PcieTlpRequesterEngine engine(pins, backend, kCompleterId);

    pins.rqBeats.push_back(axion::tlp::beat(
        axion::tlp::makeDw0(axion::TlpFmt::Hdr4NoData, axion::TlpType::Mem,
                            2),
        axion::tlp::makeReqDw1(0x0200, 0x33, 0xf, 0xf)));
    pins.rqBeats.push_back(axion::tlp::beat(0, 0x80002000));

    for (int i = 0; i < 3; i++) {
        engine.tick();
    }

    ASSERT_EQ(backend.ops.size(), 1u);
    EXPECT_FALSE(backend.ops[0].isWrite);
    EXPECT_EQ(backend.ops[0].addr, 0x80002000u);

    const uint64_t payload = 0xAABBCCDDEEFF0011ull;
    engine.completeRead(backend.ops[0].seq,
                        reinterpret_cast<const uint8_t *>(&payload), 8);

    for (int i = 0; i < 4; i++) {
        engine.tick();
    }

    ASSERT_EQ(pins.rcBeats.size(), 3u);
    const uint32_t dw0 = axion::tlp::beatLowDw(pins.rcBeats[0]);
    EXPECT_EQ(axion::tlp::dw0Fmt(dw0), axion::TlpFmt::Hdr3Data);
    EXPECT_EQ(axion::tlp::dw0Type(dw0), axion::TlpType::Cpl);
    EXPECT_EQ(axion::tlp::dw0LengthDw(dw0), 2u);

    const uint32_t dw1 = axion::tlp::beatHighDw(pins.rcBeats[0]);
    EXPECT_EQ(axion::tlp::cplDw1Status(dw1), axion::TlpCplStatus::Success);
    EXPECT_EQ(axion::tlp::cplDw1ByteCount(dw1), 8u);

    const uint32_t dw2 = axion::tlp::beatLowDw(pins.rcBeats[1]);
    EXPECT_EQ(axion::tlp::cplDw2Tag(dw2), 0x33u);
    EXPECT_EQ(dw2 >> 16, 0x0200u);

    EXPECT_EQ(pins.rcBeats[2], payload);
    EXPECT_EQ(pins.rcLasts[2], 1u);
    EXPECT_TRUE(engine.idle());
}

TEST(PcieTlpRequesterEngineTest, DifferentTagsMayCompleteOutOfIssueOrder)
{
    MockRequesterPins pins;
    RecordingBackend backend;
    axion::PcieTlpRequesterEngine engine(pins, backend, kCompleterId);

    // Two reads, tags 0x01 then 0x02.
    for (uint8_t tag : {uint8_t(0x01), uint8_t(0x02)}) {
        pins.rqBeats.push_back(axion::tlp::beat(
            axion::tlp::makeDw0(axion::TlpFmt::Hdr4NoData,
                                axion::TlpType::Mem, 1),
            axion::tlp::makeReqDw1(0x0200, tag, 0x0, 0xf)));
        pins.rqBeats.push_back(
            axion::tlp::beat(0, 0x80003000 + (tag * 0x100)));
    }

    for (int i = 0; i < 6; i++) {
        engine.tick();
    }
    ASSERT_EQ(backend.ops.size(), 2u);

    // Complete the *second* one first -- legal across distinct tags, and
    // the completion stream must reflect that order.
    const uint32_t second = 0x22222222;
    const uint32_t first = 0x11111111;
    engine.completeRead(backend.ops[1].seq,
                        reinterpret_cast<const uint8_t *>(&second), 4);
    for (int i = 0; i < 4; i++) {
        engine.tick();
    }
    engine.completeRead(backend.ops[0].seq,
                        reinterpret_cast<const uint8_t *>(&first), 4);
    for (int i = 0; i < 4; i++) {
        engine.tick();
    }

    ASSERT_EQ(pins.rcBeats.size(), 6u);
    EXPECT_EQ(axion::tlp::cplDw2Tag(axion::tlp::beatLowDw(pins.rcBeats[1])),
              0x02u);
    EXPECT_EQ(axion::tlp::beatLowDw(pins.rcBeats[2]), second);
    EXPECT_EQ(axion::tlp::cplDw2Tag(axion::tlp::beatLowDw(pins.rcBeats[4])),
              0x01u);
    EXPECT_EQ(axion::tlp::beatLowDw(pins.rcBeats[5]), first);
    EXPECT_TRUE(engine.idle());
}

} // namespace pcie_tlp_engine_test
} // namespace gem5
