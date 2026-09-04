/*
 * PcieTlpCompleterEngine / PcieTlpRequesterEngine: the TLP-level
 * counterparts to Axi4SlaveEngine / Axi4MasterEngine. They translate
 * between gem5 PacketPtrs and genuine PCIe transaction-layer packets on
 * the four AXI4-Stream channels a hard IP block exposes (see
 * hw/pcie/pcie_tlp_if.sv), so a DUT can do its own header parsing and
 * building.
 *
 * Same per-cycle shape as the AXI4 engines: drive combinational inputs ->
 * eval -> sample completed handshakes -> toggle clk 0->1->0 -> advance
 * state, with `driveClock = false` on every engine but one when several
 * share a Verilated model.
 */

#ifndef __PCIE_PCIE_TLP_ENGINE_HH__
#define __PCIE_PCIE_TLP_ENGINE_HH__

#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <vector>

#include "mem/packet.hh"
#include "pcie/pcie_tlp_types.hh"

namespace gem5
{
namespace axion
{

/**
 * Drives the completer streams: turns one gem5 PacketPtr at a time into a
 * MemRd or MemWr TLP on CQ and, for reads, decodes the CplD that comes
 * back on CC.
 *
 * A MemWr is *posted*: PCIe returns no completion for it, so the packet is
 * answered as soon as the last request beat is accepted. That is a real
 * protocol difference from the AXI4 path, where every write waits for a B
 * response.
 */
class PcieTlpCompleterEngine
{
  public:
    PcieTlpCompleterEngine(PcieTlpCompleterPins &pins, uint16_t requesterId);

    /**
     * Queue a packet for translation into a TLP. `tlpAddr` is the address
     * placed in the header -- BAR-relative, so the DUT sees fixed offsets
     * regardless of where enumeration mapped the BAR. `onDone` fires once
     * the packet has been turned into a response.
     *
     * Returns false if a transaction is already in flight; the caller
     * should hold the packet and retry.
     */
    bool issue(PacketPtr pkt, Addr tlpAddr,
               const std::function<void()> &onDone);

    bool busy() const { return state_ != State::Idle; }

    void tick(bool driveClock = true);

  private:
    enum class State
    {
        Idle,
        ReqHdr0,
        ReqHdr1,
        ReqData,
        CplHdr0,
        CplHdr1,
        CplData,
    };

    PcieTlpCompleterPins &pins_;
    const uint16_t requesterId_;

    State state_ = State::Idle;
    PacketPtr pkt_ = nullptr;
    std::function<void()> onDone_;
    std::function<void()> pendingDone_;

    Addr addr_ = 0;
    uint16_t lengthDw_ = 0;
    uint8_t firstBe_ = 0;
    uint8_t lastBe_ = 0;
    uint8_t tag_ = 0;
    uint8_t nextTag_ = 0;
    unsigned beatsDone_ = 0;
    unsigned cplDwDone_ = 0;

    // Status decoded from the completion header. Held rather than applied
    // on the spot: Packet::setBadAddress()/setBadCommand() assert the
    // packet is already a response, and makeResponse() only happens once
    // the last completion beat has landed.
    TlpCplStatus cplStatus_ = TlpCplStatus::Success;

    void applyCplStatus();

    void driveInputs();
    void sampleAndAdvance();
    uint64_t writeBeat(unsigned index) const;
    void captureCplBeat(uint64_t data);
};

/**
 * Services the requester streams: decodes the DUT's own MemRd/MemWr TLPs
 * on RQ, hands them to a Backend (gem5's DMA machinery), and returns CplD
 * completions on RC.
 *
 * Reads are tracked per tag. Completions are emitted in the order the
 * backend finishes them, which across different tags may differ from issue
 * order -- that is exactly what PCIe permits, and the same property
 * Axi4MasterEngine implements per AXI ID. A tag may not be reused while
 * outstanding, and reusing one is a fatal error rather than silently
 * mismatched data.
 */
class PcieTlpRequesterEngine
{
  public:
    class Backend
    {
      public:
        virtual ~Backend() = default;
        virtual void issueRead(uint64_t seq, Addr addr, unsigned size) = 0;
        virtual void issueWrite(uint64_t seq, Addr addr, unsigned size,
                                const uint8_t *data) = 0;
    };

    PcieTlpRequesterEngine(PcieTlpRequesterPins &pins, Backend &backend,
                           uint16_t completerId);

    /** Backend callbacks: a previously issued transaction has completed. */
    void completeRead(uint64_t seq, const uint8_t *data, unsigned size);
    void completeWrite(uint64_t seq);

    void tick(bool driveClock = true);

    /** True when nothing is outstanding and nothing is waiting to be sent. */
    bool
    idle() const
    {
        return reads_.empty() && postedWrites_ == 0 && cplQueue_.empty();
    }

  private:
    enum class RqState
    {
        Hdr0,
        Hdr1,
        Data,
    };

    enum class RcState
    {
        Idle,
        Hdr0,
        Hdr1,
        Data,
    };

    struct PendingRead
    {
        uint64_t seq;
        uint8_t tag;
        uint16_t requesterId;
        Addr addr;
        unsigned size;
    };

    struct Completion
    {
        uint8_t tag;
        uint16_t requesterId;
        Addr addr;
        std::vector<uint8_t> data;
    };

    PcieTlpRequesterPins &pins_;
    Backend &backend_;
    const uint16_t completerId_;

    RqState rqState_ = RqState::Hdr0;
    RcState rcState_ = RcState::Idle;

    // Header fields of the RQ packet currently being decoded.
    TlpFmt reqFmt_ = TlpFmt::Hdr4NoData;
    uint16_t reqLengthDw_ = 0;
    uint16_t reqRequesterId_ = 0;
    uint8_t reqTag_ = 0;
    Addr reqAddr_ = 0;
    std::vector<uint8_t> reqData_;

    uint64_t nextSeq_ = 0;
    unsigned postedWrites_ = 0;
    std::unordered_map<uint64_t, PendingRead> reads_;
    std::deque<Completion> cplQueue_;
    unsigned cplBeatsDone_ = 0;

    void driveInputs();
    void sampleAndAdvance();
    void startRequest();
};

} // namespace axion
} // namespace gem5

#endif // __PCIE_PCIE_TLP_ENGINE_HH__
