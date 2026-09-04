#include "pcie/pcie_tlp_engine.hh"

#include <algorithm>
#include <cstring>

#include "base/logging.hh"

namespace gem5
{
namespace axion
{

namespace
{

/**
 * Byte enables for the first and last DW of a request, per PCIe base spec
 * 2.2.5. `addr` must be DW-aligned -- every access AXION issues here comes
 * from a register-device PIO packet or from a DMA burst, both of which
 * are; anything else is a caller bug, caught at the call site.
 */
void
computeByteEnables(Addr addr, unsigned size, uint16_t lengthDw,
                    uint8_t &firstBe, uint8_t &lastBe)
{
    const unsigned firstBytes = std::min<unsigned>(size, 4);
    firstBe = static_cast<uint8_t>((1u << firstBytes) - 1);

    if (lengthDw == 1) {
        // A single-DW request must report zero last-DW byte enables.
        lastBe = 0;
        return;
    }

    const unsigned tail = (addr + size) & 3;
    lastBe = tail ? static_cast<uint8_t>((1u << tail) - 1) : 0xf;
}

} // namespace

// ---------------------------------------------------------------------
// Completer engine
// ---------------------------------------------------------------------

PcieTlpCompleterEngine::PcieTlpCompleterEngine(PcieTlpCompleterPins &pins,
                                                uint16_t requesterId)
    : pins_(pins), requesterId_(requesterId)
{}

bool
PcieTlpCompleterEngine::issue(PacketPtr pkt, Addr tlpAddr,
                               const std::function<void()> &onDone)
{
    if (state_ != State::Idle)
        return false;

    panic_if(tlpAddr & 0x3,
             "PcieTlpCompleterEngine: unaligned TLP address %#x; requests "
             "must start on a DW boundary", tlpAddr);
    panic_if(pkt->getSize() == 0, "PcieTlpCompleterEngine: zero-size packet");

    pkt_ = pkt;
    onDone_ = onDone;
    addr_ = tlpAddr;
    lengthDw_ = static_cast<uint16_t>((pkt->getSize() + 3) / 4);
    computeByteEnables(tlpAddr, pkt->getSize(), lengthDw_, firstBe_, lastBe_);
    tag_ = nextTag_++;
    beatsDone_ = 0;
    cplDwDone_ = 0;
    cplStatus_ = TlpCplStatus::Success;
    state_ = State::ReqHdr0;
    return true;
}

uint64_t
PcieTlpCompleterEngine::writeBeat(unsigned index) const
{
    // Beat `index` of the payload carries DWs 2*index and 2*index+1, i.e.
    // bytes [8*index, 8*index+8) of the packet, short-read at the end.
    const unsigned off = index * 8;
    const unsigned n = std::min<unsigned>(8, pkt_->getSize() - off);
    uint64_t data = 0;
    std::memcpy(&data, pkt_->getConstPtr<uint8_t>() + off, n);
    return data;
}

void
PcieTlpCompleterEngine::captureCplBeat(uint64_t data)
{
    // A completion beat carries up to two payload DWs; copy however many
    // of them the packet still has room for.
    const unsigned off = cplDwDone_ * 4;
    if (off >= pkt_->getSize())
        return;
    const unsigned n = std::min<unsigned>(8, pkt_->getSize() - off);
    std::memcpy(pkt_->getPtr<uint8_t>() + off, &data, n);
}

void
PcieTlpCompleterEngine::applyCplStatus()
{
    // Map the statuses a device can plausibly return onto gem5's packet
    // error flags, same as the AXI4 path does with SLVERR/DECERR. Both
    // setters require the packet to already be a response.
    switch (cplStatus_) {
      case TlpCplStatus::Success:
        break;
      case TlpCplStatus::UnsupportedRequest:
        pkt_->setBadAddress();
        break;
      default:
        pkt_->setBadCommand();
        break;
    }
}

void
PcieTlpCompleterEngine::driveInputs()
{
    switch (state_) {
      case State::Idle:
        pins_.cqSetTValid(0);
        pins_.cqSetTLast(0);
        pins_.ccSetTReady(0);
        break;

      case State::ReqHdr0: {
        const TlpFmt fmt = pkt_->isWrite() ? TlpFmt::Hdr4Data
                                            : TlpFmt::Hdr4NoData;
        pins_.cqSetTData(tlp::beat(
            tlp::makeDw0(fmt, TlpType::Mem, lengthDw_),
            tlp::makeReqDw1(requesterId_, tag_, lastBe_, firstBe_)));
        pins_.cqSetTLast(0);
        pins_.cqSetTValid(1);
        pins_.ccSetTReady(0);
        break;
      }

      case State::ReqHdr1:
        // DW2 = address[63:32], DW3 = address[31:2] << 2.
        pins_.cqSetTData(tlp::beat(
            static_cast<uint32_t>(addr_ >> 32),
            static_cast<uint32_t>(addr_) & 0xfffffffcu));
        // A read's request ends here; a write's payload follows.
        pins_.cqSetTLast(pkt_->isRead() ? 1 : 0);
        pins_.cqSetTValid(1);
        break;

      case State::ReqData: {
        const unsigned totalBeats = (lengthDw_ + 1) / 2;
        pins_.cqSetTData(writeBeat(beatsDone_));
        pins_.cqSetTLast(beatsDone_ == totalBeats - 1 ? 1 : 0);
        pins_.cqSetTValid(1);
        break;
      }

      case State::CplHdr0:
      case State::CplHdr1:
      case State::CplData:
        pins_.cqSetTValid(0);
        pins_.cqSetTLast(0);
        pins_.ccSetTReady(1);
        break;
    }
}

void
PcieTlpCompleterEngine::sampleAndAdvance()
{
    switch (state_) {
      case State::Idle:
        break;

      case State::ReqHdr0:
        if (pins_.cqGetTReady())
            state_ = State::ReqHdr1;
        break;

      case State::ReqHdr1:
        if (pins_.cqGetTReady()) {
            if (pkt_->isRead()) {
                state_ = State::CplHdr0;
            } else {
                state_ = State::ReqData;
            }
        }
        break;

      case State::ReqData: {
        if (!pins_.cqGetTReady())
            break;
        const unsigned totalBeats = (lengthDw_ + 1) / 2;
        if (++beatsDone_ >= totalBeats) {
            // Memory writes are posted: no completion is coming, so the
            // packet is done the moment the last beat is accepted.
            pkt_->makeResponse();
            pendingDone_ = onDone_;
            pkt_ = nullptr;
            onDone_ = nullptr;
            state_ = State::Idle;
        }
        break;
      }

      case State::CplHdr0:
        if (pins_.ccGetTValid()) {
            const uint32_t dw0 = tlp::beatLowDw(pins_.ccGetTData());
            const uint32_t dw1 = tlp::beatHighDw(pins_.ccGetTData());
            panic_if(tlp::dw0Type(dw0) != TlpType::Cpl,
                     "PcieTlpCompleterEngine: expected a completion, got "
                     "type %#x", static_cast<unsigned>(tlp::dw0Type(dw0)));
            cplStatus_ = tlp::cplDw1Status(dw1);
            state_ = State::CplHdr1;
        }
        break;

      case State::CplHdr1:
        if (pins_.ccGetTValid()) {
            const uint32_t dw2 = tlp::beatLowDw(pins_.ccGetTData());
            panic_if(tlp::cplDw2Tag(dw2) != tag_,
                     "PcieTlpCompleterEngine: completion carries tag %#x, "
                     "expected %#x", tlp::cplDw2Tag(dw2), tag_);
            state_ = State::CplData;
        }
        break;

      case State::CplData:
        if (pins_.ccGetTValid()) {
            captureCplBeat(pins_.ccGetTData());
            cplDwDone_ += 2;
            if (pins_.ccGetTLast() || cplDwDone_ >= lengthDw_) {
                pkt_->makeResponse();
                applyCplStatus();
                pendingDone_ = onDone_;
                pkt_ = nullptr;
                onDone_ = nullptr;
                state_ = State::Idle;
            }
        }
        break;
    }
}

void
PcieTlpCompleterEngine::tick(bool driveClock)
{
    pins_.tlpSetClk(0);
    driveInputs();
    pins_.tlpEval();

    sampleAndAdvance();

    if (driveClock) {
        pins_.tlpSetClk(1);
        pins_.tlpEval();
        pins_.tlpSetClk(0);
    }

    if (pendingDone_) {
        auto done = pendingDone_;
        pendingDone_ = nullptr;
        done();
    }
}

// ---------------------------------------------------------------------
// Requester engine
// ---------------------------------------------------------------------

PcieTlpRequesterEngine::PcieTlpRequesterEngine(PcieTlpRequesterPins &pins,
                                                Backend &backend,
                                                uint16_t completerId)
    : pins_(pins), backend_(backend), completerId_(completerId)
{}

void
PcieTlpRequesterEngine::completeRead(uint64_t seq, const uint8_t *data,
                                      unsigned size)
{
    auto it = reads_.find(seq);
    panic_if(it == reads_.end(),
             "PcieTlpRequesterEngine: completion for unknown seq %llu", seq);

    Completion cpl;
    cpl.tag = it->second.tag;
    cpl.requesterId = it->second.requesterId;
    cpl.addr = it->second.addr;
    cpl.data.assign(data, data + size);
    reads_.erase(it);

    // Queued in backend-completion order, which across different tags may
    // differ from issue order -- exactly what PCIe allows.
    cplQueue_.push_back(std::move(cpl));
}

void
PcieTlpRequesterEngine::completeWrite(uint64_t seq)
{
    // Memory writes are posted; nothing goes back on RC. The count only
    // exists so idle() does not claim quiescence with a write still in
    // gem5's memory system.
    panic_if(postedWrites_ == 0,
             "PcieTlpRequesterEngine: write completion with none pending");
    postedWrites_--;
}

void
PcieTlpRequesterEngine::startRequest()
{
    const unsigned size = reqLengthDw_ * 4;

    if (tlp::fmtHasData(reqFmt_)) {
        postedWrites_++;
        backend_.issueWrite(nextSeq_++, reqAddr_, size, reqData_.data());
        return;
    }

    const uint64_t seq = nextSeq_++;
    for (const auto &kv : reads_) {
        panic_if(kv.second.tag == reqTag_,
                 "PcieTlpRequesterEngine: tag %#x reused while still "
                 "outstanding", reqTag_);
    }
    reads_[seq] = PendingRead{seq, reqTag_, reqRequesterId_, reqAddr_, size};
    backend_.issueRead(seq, reqAddr_, size);
}

void
PcieTlpRequesterEngine::driveInputs()
{
    pins_.rqSetTReady(1);

    if (rcState_ == RcState::Idle && !cplQueue_.empty()) {
        rcState_ = RcState::Hdr0;
        cplBeatsDone_ = 0;
    }

    if (rcState_ == RcState::Idle) {
        pins_.rcSetTValid(0);
        pins_.rcSetTLast(0);
        return;
    }

    const Completion &cpl = cplQueue_.front();
    const uint16_t lengthDw = static_cast<uint16_t>(cpl.data.size() / 4);
    const unsigned totalBeats = (lengthDw + 1) / 2;

    switch (rcState_) {
      case RcState::Hdr0:
        pins_.rcSetTData(tlp::beat(
            tlp::makeDw0(TlpFmt::Hdr3Data, TlpType::Cpl, lengthDw),
            tlp::makeCplDw1(completerId_, TlpCplStatus::Success,
                            static_cast<uint16_t>(cpl.data.size()))));
        pins_.rcSetTLast(0);
        pins_.rcSetTValid(1);
        break;

      case RcState::Hdr1:
        // DW3 is the pad that keeps payload beat-aligned; see
        // hw/pcie/pcie_tlp_pkg.sv.
        pins_.rcSetTData(tlp::beat(
            tlp::makeCplDw2(cpl.requesterId, cpl.tag,
                            static_cast<uint8_t>(cpl.addr & 0x7f)),
            0));
        pins_.rcSetTLast(0);
        pins_.rcSetTValid(1);
        break;

      case RcState::Data: {
        const unsigned off = cplBeatsDone_ * 8;
        const unsigned n = std::min<unsigned>(
            8, static_cast<unsigned>(cpl.data.size()) - off);
        uint64_t data = 0;
        std::memcpy(&data, cpl.data.data() + off, n);
        pins_.rcSetTData(data);
        pins_.rcSetTLast(cplBeatsDone_ == totalBeats - 1 ? 1 : 0);
        pins_.rcSetTValid(1);
        break;
      }

      case RcState::Idle:
        break;
    }
}

void
PcieTlpRequesterEngine::sampleAndAdvance()
{
    // --- RQ: decode the DUT's outgoing request ---
    if (pins_.rqGetTValid()) {
        const uint64_t data = pins_.rqGetTData();
        switch (rqState_) {
          case RqState::Hdr0: {
            const uint32_t dw0 = tlp::beatLowDw(data);
            const uint32_t dw1 = tlp::beatHighDw(data);
            reqFmt_ = tlp::dw0Fmt(dw0);
            panic_if(!tlp::fmtIs4Dw(reqFmt_),
                     "PcieTlpRequesterEngine: 3-DW request headers are not "
                     "supported on this path (see hw/pcie/pcie_tlp_pkg.sv)");
            panic_if(tlp::dw0Type(dw0) != TlpType::Mem,
                     "PcieTlpRequesterEngine: only memory requests are "
                     "supported, got type %#x",
                     static_cast<unsigned>(tlp::dw0Type(dw0)));
            reqLengthDw_ = tlp::dw0LengthDw(dw0);
            reqRequesterId_ = tlp::reqDw1RequesterId(dw1);
            reqTag_ = tlp::reqDw1Tag(dw1);
            reqData_.clear();
            rqState_ = RqState::Hdr1;
            break;
          }

          case RqState::Hdr1:
            reqAddr_ = (static_cast<Addr>(tlp::beatLowDw(data)) << 32) |
                       tlp::beatHighDw(data);
            if (tlp::fmtHasData(reqFmt_)) {
                rqState_ = RqState::Data;
            } else {
                startRequest();
                rqState_ = RqState::Hdr0;
            }
            break;

          case RqState::Data: {
            const unsigned want = reqLengthDw_ * 4;
            const unsigned have = static_cast<unsigned>(reqData_.size());
            const unsigned n = std::min<unsigned>(8, want - have);
            const auto *bytes = reinterpret_cast<const uint8_t *>(&data);
            reqData_.insert(reqData_.end(), bytes, bytes + n);
            if (pins_.rqGetTLast() || reqData_.size() >= want) {
                startRequest();
                rqState_ = RqState::Hdr0;
            }
            break;
          }
        }
    }

    // --- RC: advance the completion currently being sent ---
    if (rcState_ != RcState::Idle && pins_.rcGetTReady()) {
        const Completion &cpl = cplQueue_.front();
        const uint16_t lengthDw = static_cast<uint16_t>(cpl.data.size() / 4);
        const unsigned totalBeats = (lengthDw + 1) / 2;

        switch (rcState_) {
          case RcState::Hdr0:
            rcState_ = RcState::Hdr1;
            break;
          case RcState::Hdr1:
            rcState_ = RcState::Data;
            break;
          case RcState::Data:
            if (++cplBeatsDone_ >= totalBeats) {
                cplQueue_.pop_front();
                rcState_ = RcState::Idle;
            }
            break;
          case RcState::Idle:
            break;
        }
    }
}

void
PcieTlpRequesterEngine::tick(bool driveClock)
{
    pins_.tlpSetClk(0);
    driveInputs();
    pins_.tlpEval();

    sampleAndAdvance();

    if (driveClock) {
        pins_.tlpSetClk(1);
        pins_.tlpEval();
        pins_.tlpSetClk(0);
    }
}

} // namespace axion
} // namespace gem5
