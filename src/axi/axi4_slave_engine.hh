/*
 * Axi4SlaveEngine: drives an Axi4SlavePins pin set from gem5 PacketPtrs,
 * one AXI4 burst at a time. Used by RTLPioDevice (its only port) and
 * RTLDmaDevice (its control/status register port).
 */

#ifndef __AXI_AXI4_SLAVE_ENGINE_HH__
#define __AXI_AXI4_SLAVE_ENGINE_HH__

#include <functional>

#include "axi/axi4_types.hh"
#include "mem/packet.hh"

namespace gem5
{
namespace axion
{

/**
 * Full AXI4 slave-side protocol engine: multi-beat AW/W/B write bursts and
 * AR/R read bursts, honoring awlen/arlen so packets larger than one beat
 * (fixed at 8 bytes) split into the right number of beats. There is
 * exactly one requester on a slave port (gem5), so there is nothing to
 * reorder -- this drives one transaction to completion before accepting
 * the next; the queueing/retry behavior toward gem5's interconnect lives
 * in the owning device's port class, not here.
 */
class Axi4SlaveEngine
{
  public:
    explicit Axi4SlaveEngine(Axi4SlavePins &pins);

    /**
     * Queue a packet for translation into an AXI4 burst. `onDone` is
     * invoked once the write response (B) or last read beat (R, last)
     * has been observed and the packet turned into a response via
     * makeResponse(). Returns false if a transaction is already in
     * flight -- the caller should hold the packet and retry.
     */
    bool issue(PacketPtr pkt, const std::function<void()> &onDone);

    bool busy() const { return state_ != State::Idle; }

    /** Advance the state machine by exactly one clock cycle. */
    void tick();

  private:
    enum class State
    {
        Idle,
        AwHandshake,
        WBeats,
        BHandshake,
        ArHandshake,
        RBeats,
    };

    Axi4SlavePins &pins_;
    State state_ = State::Idle;
    PacketPtr pkt_ = nullptr;
    std::function<void()> onDone_;

    static constexpr unsigned beatBytes_ = 8;
    Addr addr_ = 0;
    AxiId id_ = 0;
    unsigned totalBeats_ = 0;
    unsigned beatsDone_ = 0;

    // Worst RRESP seen across all beats of the in-flight read (AxiResp's
    // values are already ordered by severity: Okay < ExOkay < SlvErr <
    // DecErr), applied to the gem5 packet once the last beat completes.
    uint8_t worstResp_ = 0;

    void driveInputs();
    void sampleAndAdvance();

    // Deferred to after the clock's rising edge so the completion
    // callback never runs from inside a combinational eval.
    std::function<void()> pendingDone_;
};

} // namespace axion
} // namespace gem5

#endif // __AXI_AXI4_SLAVE_ENGINE_HH__
