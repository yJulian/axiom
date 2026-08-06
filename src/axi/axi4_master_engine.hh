/*
 * Axi4MasterEngine: services an Axi4MasterPins pin set driven by a DUT
 * (the DUT is the AXI4 master; this engine plays the responder role gem5's
 * memory system occupies). Used by RTLDmaDevice's DMA port and
 * RTLBaseCpu's inst/data ports.
 *
 * This is the one place genuine AXI4 ID-based out-of-order semantics are
 * implemented: transactions sharing an ID complete in issue order;
 * transactions with different IDs may complete in any order, tracked by a
 * global issue-order sequence number. The pickOldestEligible() selection
 * rule is modeled directly on gem5_cva6's src/accel/dma_master_engine.cc
 * (same-ID-in-order, cross-ID-out-of-order DMA completion arbitration).
 */

#ifndef __AXI_AXI4_MASTER_ENGINE_HH__
#define __AXI_AXI4_MASTER_ENGINE_HH__

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include "axi/axi4_types.hh"

namespace gem5
{
namespace axion
{

class Axi4MasterEngine
{
  public:
    /**
     * The seam this engine uses to actually move data through gem5's
     * memory system, and to be told when a previously-issued transaction
     * completes. RTLDmaDevice implements this via dmaRead()/dmaWrite();
     * RTLBaseCpu via its RequestPort's sendTimingReq().
     */
    class Backend
    {
      public:
        virtual ~Backend() = default;
        virtual void issueRead(uint64_t seq, Addr addr, unsigned size) = 0;
        virtual void issueWrite(uint64_t seq, Addr addr, unsigned size,
                                 const uint8_t *data) = 0;
    };

    Axi4MasterEngine(Axi4MasterPins &pins, Backend &backend);

    /** Backend callbacks: a previously issued transaction has completed. */
    void completeRead(uint64_t seq, const uint8_t *data, unsigned size);
    void completeWrite(uint64_t seq);

    /**
     * Advance the state machine by exactly one clock cycle. Pass
     * `driveClock = false` when multiple engines share one underlying
     * Verilated model (e.g. RTLBaseCpu's inst/data ports both fed by a
     * single RTL core) and another engine already owns toggling that
     * model's actual clk pin this cycle -- this engine's own combinational
     * drive/sample/eval still runs, it just skips the rising-edge toggle
     * so the shared model's registers don't advance twice per cycle.
     */
    void tick(bool driveClock = true);

  private:
    // AxLOCK/AxCACHE/AxPROT/AxQOS/AxREGION sampled off the DUT-driven address
    // channel for pin-accuracy. Nothing downstream consumes them yet --
    // Backend::issueRead/issueWrite models a single linear gem5 memory
    // access per burst, with no cache/QoS/region-aware access path -- so
    // these are captured-but-currently-unused, same spirit as AWID/ARID
    // being captured for echo-back even before any reordering logic needed
    // them.
    struct AxAttrs
    {
        uint8_t lock = 0;
        uint8_t cache = 0;
        uint8_t prot = 0;
        uint8_t qos = 0;
        uint8_t region = 0;
    };

    struct ReadXact
    {
        uint64_t seq;
        AxiId id;
        Addr addr;
        unsigned size;
        unsigned beatBytes;
        bool dataReady = false;
        std::vector<uint8_t> data;
        unsigned beatsSent = 0;
        AxAttrs attrs;
    };

    struct WriteXact
    {
        uint64_t seq;
        AxiId id;
        Addr addr;
        std::vector<uint8_t> data;
        bool respReady = false;
        AxAttrs attrs;
    };

    Axi4MasterPins &pins_;
    Backend &backend_;
    uint64_t nextSeq_ = 0;

    // Issue-order queue per ID, oldest-first; the front of each deque is
    // the only transaction for that ID allowed to complete next.
    std::unordered_map<AxiId, std::deque<uint64_t>> readOrder_;
    std::unordered_map<AxiId, std::deque<uint64_t>> writeOrder_;
    std::unordered_map<uint64_t, ReadXact> reads_;
    std::unordered_map<uint64_t, WriteXact> writes_;

    // Address-channel acceptance state (one AW or AR beat sequence
    // accepted at a time at the pin level -- the real overlap happens
    // between issuing a transaction and its completion arriving back,
    // which is fully concurrent across IDs).
    bool awAcceptedThisBurst_ = false;
    bool arAcceptedThisBurst_ = false;
    uint64_t currentWriteSeq_ = UINT64_MAX;

    // Currently-presenting-on-R / currently-presenting-on-B transaction.
    uint64_t rInFlightSeq_ = UINT64_MAX;
    uint64_t bInFlightSeq_ = UINT64_MAX;

    void sampleAr();
    void sampleAwAndW();
    void driveR();
    void driveB();

    /** Oldest ready read that is also the oldest outstanding for its ID. */
    uint64_t pickOldestEligibleRead() const;
    uint64_t pickOldestEligibleWrite() const;
};

} // namespace axion
} // namespace gem5

#endif // __AXI_AXI4_MASTER_ENGINE_HH__
