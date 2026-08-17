/*
 * Unit tests for Axi4MasterEngine: no DMA-capable RTL example exists in
 * this repo (only the PIO-only fifo_pio_accel worked example), so this
 * engine's multi-beat read streaming and non-INCR burst guard can only be
 * exercised against a mock Axi4MasterPins + mock Backend, as gem5 gtests
 * (`scons build/RISCV/unittests.opt`).
 *
 * CrossId{Reads,Writes}CompleteOutOfOrderSameIdStaysInOrder below are the
 * ones that actually exercise pickOldestEligibleRead()/Write() -- the
 * per-ID-in-order/cross-ID-out-of-order selection rule described in this
 * engine's header comment -- which every other test here leaves untouched
 * (single outstanding transaction at a time). See also
 * examples/dma_memcopy_accel/cocotb/test_dma_memcopy.py, which proves
 * three concurrent channels (distinct AXI IDs) against real Verilated RTL
 * instead of these mocks -- though its AxiRam responder completes
 * requests in arrival order, so explicit *out-of-order* completion
 * against real RTL (as opposed to concurrent-outstanding correctness) is
 * only covered here, against the mocks below.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "axi/axi4_master_engine.hh"
#include "base/gtest/logging.hh"

namespace gem5
{
namespace axi4_master_engine_test
{

// Plain member-variable pins, driven/read directly by the test in place of
// a real Verilated DUT -- the DUT is the AXI4 master here, so the *Get*
// accessors return whatever the test scripted as "driven by the DUT", and
// the *Set* accessors record what the engine drove back.
class MockMasterPins : public axion::Axi4MasterPins
{
  public:
    void axiSetClk(uint8_t) override {}
    void axiSetRstN(uint8_t) override {}
    void axiEval() override {}

    axion::AxiId axiMasterGetAwId() override { return awId; }
    Addr axiMasterGetAwAddr() override { return awAddr; }
    uint8_t axiMasterGetAwLen() override { return awLen; }
    uint8_t axiMasterGetAwSize() override { return awSize; }
    uint8_t axiMasterGetAwBurst() override { return awBurst; }
    uint8_t axiMasterGetAwLock() override { return 0; }
    uint8_t axiMasterGetAwCache() override { return 0; }
    uint8_t axiMasterGetAwProt() override { return 0; }
    uint8_t axiMasterGetAwQos() override { return 0; }
    uint8_t axiMasterGetAwRegion() override { return 0; }
    uint8_t axiMasterGetAwValid() override { return awValid; }
    void axiMasterSetAwReady(uint8_t val) override { awReady = val; }

    uint64_t axiMasterGetWData() override { return wData; }
    uint64_t axiMasterGetWStrb() override { return wStrb; }
    uint8_t axiMasterGetWLast() override { return wLast; }
    uint8_t axiMasterGetWValid() override { return wValid; }
    void axiMasterSetWReady(uint8_t val) override { wReady = val; }

    void axiMasterSetBId(axion::AxiId id) override { bId = id; }
    void axiMasterSetBResp(uint8_t resp) override { bResp = resp; }
    void axiMasterSetBValid(uint8_t val) override { bValid = val; }
    uint8_t axiMasterGetBReady() override { return 1; }

    axion::AxiId axiMasterGetArId() override { return arId; }
    Addr axiMasterGetArAddr() override { return arAddr; }
    uint8_t axiMasterGetArLen() override { return arLen; }
    uint8_t axiMasterGetArSize() override { return arSize; }
    uint8_t axiMasterGetArBurst() override { return arBurst; }
    uint8_t axiMasterGetArLock() override { return 0; }
    uint8_t axiMasterGetArCache() override { return 0; }
    uint8_t axiMasterGetArProt() override { return 0; }
    uint8_t axiMasterGetArQos() override { return 0; }
    uint8_t axiMasterGetArRegion() override { return 0; }
    uint8_t axiMasterGetArValid() override { return arValid; }
    void axiMasterSetArReady(uint8_t val) override { arReady = val; }

    void axiMasterSetRId(axion::AxiId id) override { rId = id; }
    void axiMasterSetRData(uint64_t data) override { rData = data; }
    void axiMasterSetRResp(uint8_t resp) override { rResp = resp; }
    void axiMasterSetRLast(uint8_t val) override { rLast = val; }
    void axiMasterSetRValid(uint8_t val) override { rValid = val; }
    uint8_t axiMasterGetRReady() override { return rReady; }

    // AR/AW stimulus, set by the test to look like a DUT-driven request.
    axion::AxiId awId = 0, arId = 0;
    Addr awAddr = 0, arAddr = 0;
    uint8_t awLen = 0, awSize = 3, awBurst = 1; // INCR
    uint8_t awValid = 0, awReady = 0;
    uint8_t arLen = 0, arSize = 3, arBurst = 1; // INCR
    uint8_t arValid = 0, arReady = 0;

    uint64_t wData = 0, wStrb = 0xff;
    uint8_t wLast = 0, wValid = 0, wReady = 0;

    axion::AxiId bId = 0;
    uint8_t bResp = 0, bValid = 0;

    // R channel: what the engine drove, and what the test's "DUT" presents
    // as RREADY (for backpressure).
    axion::AxiId rId = 0;
    uint64_t rData = 0;
    uint8_t rResp = 0, rLast = 0, rValid = 0;
    uint8_t rReady = 1;
};

class MockBackend : public axion::Axi4MasterEngine::Backend
{
  public:
    struct ReadCall
    {
        uint64_t seq;
        Addr addr;
        unsigned size;
    };

    struct WriteCall
    {
        uint64_t seq;
        Addr addr;
        unsigned size;
    };

    std::vector<ReadCall> reads;
    std::vector<WriteCall> writes;

    void
    issueRead(uint64_t seq, Addr addr, unsigned size) override
    {
        reads.push_back({seq, addr, size});
    }

    void
    issueWrite(uint64_t seq, Addr addr, unsigned size,
               const uint8_t *) override
    {
        writes.push_back({seq, addr, size});
    }
};

TEST(Axi4MasterEngineTest, MultiBeatReadStreamsRlastOnlyOnFinalBeat)
{
    MockMasterPins pins;
    MockBackend backend;
    axion::Axi4MasterEngine engine(pins, backend);

    // ARLEN=2 (3 beats), ARSIZE=3 (8 bytes/beat) -> 24-byte read.
    pins.arId = 7;
    pins.arAddr = 0x1000;
    pins.arLen = 2;
    pins.arSize = 3;
    pins.arBurst = 1; // INCR
    pins.arValid = 1;
    engine.tick();
    pins.arValid = 0;

    ASSERT_EQ(backend.reads.size(), 1u);
    EXPECT_EQ(backend.reads[0].addr, 0x1000u);
    EXPECT_EQ(backend.reads[0].size, 24u);

    uint8_t data[24];
    for (int i = 0; i < 24; i++)
        data[i] = static_cast<uint8_t>(i);
    engine.completeRead(backend.reads[0].seq, data, 24);

    uint64_t beat0, beat1, beat2;
    std::memcpy(&beat0, data, 8);
    std::memcpy(&beat1, data + 8, 8);
    std::memcpy(&beat2, data + 16, 8);

    // Beat 0 presented.
    engine.tick();
    EXPECT_EQ(pins.rValid, 1);
    EXPECT_EQ(pins.rLast, 0);
    EXPECT_EQ(pins.rId, 7u);
    EXPECT_EQ(pins.rData, beat0);

    // Backpressure: RREADY low must re-present the same beat, not advance.
    pins.rReady = 0;
    engine.tick();
    EXPECT_EQ(pins.rValid, 1);
    EXPECT_EQ(pins.rLast, 0);
    EXPECT_EQ(pins.rData, beat0);

    // RREADY back up: beat 0 handshakes, engine advances to beat 1.
    pins.rReady = 1;
    engine.tick();
    EXPECT_EQ(pins.rLast, 0);
    EXPECT_EQ(pins.rData, beat1);

    // Beat 1 handshakes -> beat 2, the final beat (RLAST=1).
    engine.tick();
    EXPECT_EQ(pins.rLast, 1);
    EXPECT_EQ(pins.rData, beat2);
}

// gem5 gtest binaries link the mock Logger (base/gtest/logging_mock.cc,
// pulled in by default via GTest()'s "gtest lib" tag) where panic_if()
// throws GTestException instead of calling exit() -- specifically so a
// panic can be asserted on without a fork-based ASSERT_DEATH, which would
// require this target to opt out of that shared default (skip_lib=True)
// and re-list every transitive logging dependency by hand, the way
// base/logging.test.cc does to test real process-death semantics. That
// depth isn't needed here: EXPECT_THROW is enough to confirm the guard
// fires.
TEST(Axi4MasterEngineTest, FixedArBurstThrows)
{
    MockMasterPins pins;
    MockBackend backend;
    axion::Axi4MasterEngine engine(pins, backend);

    pins.arAddr = 0x1000;
    pins.arLen = 0;
    pins.arSize = 3;
    pins.arBurst = 0; // FIXED
    pins.arValid = 1;

    EXPECT_THROW(engine.tick(), GTestException);
}

TEST(Axi4MasterEngineTest, WrapAwBurstThrows)
{
    MockMasterPins pins;
    MockBackend backend;
    axion::Axi4MasterEngine engine(pins, backend);

    pins.awAddr = 0x1000;
    pins.awLen = 0;
    pins.awSize = 3;
    pins.awBurst = 2; // WRAP
    pins.awValid = 1;

    EXPECT_THROW(engine.tick(), GTestException);
}

// Three single-beat reads issued back to back: id=1 (seq0), id=2 (seq1),
// id=1 again (seq2). Completing them out of issue order (seq1, then seq2,
// then seq0) must still present id=2's transaction on R before id=1's
// earlier-issued seq0 -- proving cross-ID out-of-order -- while id=1's
// seq2 must not appear until seq0 (the older transaction sharing its ID)
// has been presented, even though seq2 became ready first -- proving
// same-ID in-order.
TEST(Axi4MasterEngineTest, CrossIdReadsCompleteOutOfOrderSameIdStaysInOrder)
{
    MockMasterPins pins;
    MockBackend backend;
    axion::Axi4MasterEngine engine(pins, backend);

    pins.arSize = 3; // 8 bytes/beat
    pins.arLen = 0;  // single beat
    pins.arBurst = 1; // INCR

    pins.arId = 1;
    pins.arAddr = 0x1000;
    pins.arValid = 1;
    engine.tick(); // issues seq0 (id 1)

    pins.arId = 2;
    pins.arAddr = 0x2000;
    engine.tick(); // issues seq1 (id 2)

    pins.arId = 1;
    pins.arAddr = 0x3000;
    engine.tick(); // issues seq2 (id 1, second outstanding)
    pins.arValid = 0;

    ASSERT_EQ(backend.reads.size(), 3u);
    uint64_t seq0 = backend.reads[0].seq;
    uint64_t seq1 = backend.reads[1].seq;
    uint64_t seq2 = backend.reads[2].seq;

    uint8_t dataA[8], dataB[8], dataC[8];
    std::memset(dataA, 0xAA, sizeof(dataA));
    std::memset(dataB, 0xBB, sizeof(dataB));
    std::memset(dataC, 0xCC, sizeof(dataC));

    // id 2 (seq1) becomes ready first -- id 1's front (seq0) is still
    // outstanding.
    engine.completeRead(seq1, dataB, 8);
    engine.tick();
    EXPECT_EQ(pins.rValid, 1);
    EXPECT_EQ(pins.rId, 2u) << "cross-ID: a later-issued but ready "
                                "transaction on a different ID must not "
                                "wait for an unrelated ID's older one";
    EXPECT_EQ(std::memcmp(&pins.rData, dataB, 8), 0);

    engine.tick(); // seq1 handshakes (rReady==1) and is popped
    EXPECT_EQ(pins.rValid, 0) << "nothing else is ready yet";

    // id 1's second transaction (seq2) becomes ready, but seq0 -- the
    // older transaction sharing its ID -- has not.
    engine.completeRead(seq2, dataC, 8);
    engine.tick();
    EXPECT_EQ(pins.rValid, 0) << "same-ID: seq2 must not present before "
                                  "seq0, its own ID's older outstanding "
                                  "transaction, even though seq2 is ready";

    engine.completeRead(seq0, dataA, 8);
    engine.tick();
    EXPECT_EQ(pins.rId, 1u);
    EXPECT_EQ(std::memcmp(&pins.rData, dataA, 8), 0);

    engine.tick(); // seq0 handshakes and is popped; seq2 (already ready)
                    // is immediately presented next, same-ID, in order.
    EXPECT_EQ(pins.rValid, 1);
    EXPECT_EQ(pins.rId, 1u);
    EXPECT_EQ(std::memcmp(&pins.rData, dataC, 8), 0);
}

// Write-side mirror of the read test above, using the B channel /
// pickOldestEligibleWrite() instead of R / pickOldestEligibleRead().
TEST(Axi4MasterEngineTest, CrossIdWritesCompleteOutOfOrderSameIdStaysInOrder)
{
    MockMasterPins pins;
    MockBackend backend;
    axion::Axi4MasterEngine engine(pins, backend);

    pins.awSize = 3;
    pins.awLen = 0;
    pins.awBurst = 1; // INCR
    pins.wStrb = 0xff;
    pins.wLast = 1;

    pins.awId = 1;
    pins.awAddr = 0x1000;
    pins.wData = 0xAAAAAAAAAAAAAAAAull;
    pins.awValid = 1;
    pins.wValid = 1;
    engine.tick(); // issues seq0 (id 1)

    pins.awId = 2;
    pins.awAddr = 0x2000;
    pins.wData = 0xBBBBBBBBBBBBBBBBull;
    engine.tick(); // issues seq1 (id 2)

    pins.awId = 1;
    pins.awAddr = 0x3000;
    pins.wData = 0xCCCCCCCCCCCCCCCCull;
    engine.tick(); // issues seq2 (id 1, second outstanding)
    pins.awValid = 0;
    pins.wValid = 0;

    ASSERT_EQ(backend.writes.size(), 3u);
    uint64_t seq0 = backend.writes[0].seq;
    uint64_t seq1 = backend.writes[1].seq;
    uint64_t seq2 = backend.writes[2].seq;

    // id 2 (seq1) completes first -- id 1's front (seq0) has not.
    engine.completeWrite(seq1);
    engine.tick();
    EXPECT_EQ(pins.bValid, 1);
    EXPECT_EQ(pins.bId, 2u) << "cross-ID: id 2's write must not wait for "
                                "id 1's older, still-incomplete write";

    engine.tick(); // BREADY is hardwired 1 in the mock: seq1 pops here.
    EXPECT_EQ(pins.bValid, 0);

    // id 1's second write (seq2) completes, but seq0 has not.
    engine.completeWrite(seq2);
    engine.tick();
    EXPECT_EQ(pins.bValid, 0) << "same-ID: seq2 must not present on B "
                                  "before seq0, its own ID's older write";

    engine.completeWrite(seq0);
    engine.tick();
    EXPECT_EQ(pins.bId, 1u);

    engine.tick(); // seq0 pops; seq2 (already complete) presents next.
    EXPECT_EQ(pins.bValid, 1);
    EXPECT_EQ(pins.bId, 1u);
}

} // namespace axi4_master_engine_test
} // namespace gem5
