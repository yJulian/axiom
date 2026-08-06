#include "axi/axi4_slave_engine.hh"

#include <algorithm>
#include <cstring>

#include "mem/request.hh"

namespace gem5
{
namespace axion
{

Axi4SlaveEngine::Axi4SlaveEngine(Axi4SlavePins &pins) : pins_(pins)
{}

bool
Axi4SlaveEngine::issue(PacketPtr pkt, const std::function<void()> &onDone)
{
    if (state_ != State::Idle)
        return false;

    pkt_ = pkt;
    onDone_ = onDone;
    addr_ = pkt->getAddr();
    id_ = 0;
    totalBeats_ = (pkt->getSize() + beatBytes_ - 1) / beatBytes_;
    if (totalBeats_ == 0)
        totalBeats_ = 1;
    beatsDone_ = 0;
    worstResp_ = 0;
    state_ = pkt->isWrite() ? State::AwHandshake : State::ArHandshake;
    return true;
}

void
Axi4SlaveEngine::driveInputs()
{
    switch (state_) {
      case State::Idle:
        pins_.axiSlaveSetAwValid(0);
        pins_.axiSlaveSetWValid(0);
        pins_.axiSlaveSetBReady(0);
        pins_.axiSlaveSetArValid(0);
        pins_.axiSlaveSetRReady(0);
        break;

      case State::AwHandshake: {
        pins_.axiSlaveSetAwId(id_);
        pins_.axiSlaveSetAwAddr(addr_);
        pins_.axiSlaveSetAwLen(static_cast<uint8_t>(totalBeats_ - 1));
        pins_.axiSlaveSetAwSize(3); // 1 << 3 == 8 bytes/beat
        pins_.axiSlaveSetAwBurst(static_cast<uint8_t>(AxiBurst::Incr));
        const RequestPtr &req = pkt_->req;
        pins_.axiSlaveSetAwLock(req->isLockedRMW() ? 1 : 0);
        pins_.axiSlaveSetAwCache(req->isUncacheable() ? 0x0 : 0x3);
        pins_.axiSlaveSetAwProt((req->isPriv() ? 1 : 0) |
                                 (req->isSecure() ? 0 : 2) |
                                 (req->isInstFetch() ? 4 : 0));
        pins_.axiSlaveSetAwQos(pkt_->qosValue());
        pins_.axiSlaveSetAwRegion(0); // no gem5 equivalent; single region
        pins_.axiSlaveSetAwValid(1);
        pins_.axiSlaveSetWValid(0);
        break;
      }

      case State::WBeats: {
        pins_.axiSlaveSetAwValid(0);
        unsigned off = beatsDone_ * beatBytes_;
        unsigned n = std::min<unsigned>(beatBytes_, pkt_->getSize() - off);
        uint64_t data = 0;
        std::memcpy(&data, pkt_->getConstPtr<uint8_t>() + off, n);
        pins_.axiSlaveSetWData(data);
        pins_.axiSlaveSetWStrb(n == beatBytes_ ? 0xffu : ((1u << n) - 1));
        pins_.axiSlaveSetWLast(beatsDone_ == totalBeats_ - 1 ? 1 : 0);
        pins_.axiSlaveSetWValid(1);
        break;
      }

      case State::BHandshake:
        pins_.axiSlaveSetWValid(0);
        pins_.axiSlaveSetBReady(1);
        break;

      case State::ArHandshake: {
        pins_.axiSlaveSetArId(id_);
        pins_.axiSlaveSetArAddr(addr_);
        pins_.axiSlaveSetArLen(static_cast<uint8_t>(totalBeats_ - 1));
        pins_.axiSlaveSetArSize(3);
        pins_.axiSlaveSetArBurst(static_cast<uint8_t>(AxiBurst::Incr));
        const RequestPtr &req = pkt_->req;
        pins_.axiSlaveSetArLock(req->isLockedRMW() ? 1 : 0);
        pins_.axiSlaveSetArCache(req->isUncacheable() ? 0x0 : 0x3);
        pins_.axiSlaveSetArProt((req->isPriv() ? 1 : 0) |
                                 (req->isSecure() ? 0 : 2) |
                                 (req->isInstFetch() ? 4 : 0));
        pins_.axiSlaveSetArQos(pkt_->qosValue());
        pins_.axiSlaveSetArRegion(0); // no gem5 equivalent; single region
        pins_.axiSlaveSetArValid(1);
        pins_.axiSlaveSetRReady(0);
        break;
      }

      case State::RBeats:
        pins_.axiSlaveSetArValid(0);
        pins_.axiSlaveSetRReady(1);
        break;
    }
}

void
Axi4SlaveEngine::sampleAndAdvance()
{
    switch (state_) {
      case State::Idle:
        break;

      case State::AwHandshake:
        if (pins_.axiSlaveGetAwReady())
            state_ = State::WBeats;
        break;

      case State::WBeats:
        if (pins_.axiSlaveGetWReady()) {
            beatsDone_++;
            state_ = (beatsDone_ == totalBeats_) ? State::BHandshake
                                                  : State::WBeats;
        }
        break;

      case State::BHandshake:
        if (pins_.axiSlaveGetBValid()) {
            uint8_t resp = pins_.axiSlaveGetBResp();
            pkt_->makeResponse();
            if (resp == static_cast<uint8_t>(AxiResp::SlvErr))
                pkt_->setBadCommand();
            else if (resp == static_cast<uint8_t>(AxiResp::DecErr))
                pkt_->setBadAddress();
            pendingDone_ = onDone_;
            pkt_ = nullptr;
            onDone_ = nullptr;
            state_ = State::Idle;
        }
        break;

      case State::ArHandshake:
        if (pins_.axiSlaveGetArReady())
            state_ = State::RBeats;
        break;

      case State::RBeats:
        if (pins_.axiSlaveGetRValid()) {
            uint64_t data = pins_.axiSlaveGetRData();
            unsigned off = beatsDone_ * beatBytes_;
            unsigned n = std::min<unsigned>(beatBytes_,
                                             pkt_->getSize() - off);
            std::memcpy(pkt_->getPtr<uint8_t>() + off, &data, n);
            worstResp_ = std::max(worstResp_, pins_.axiSlaveGetRResp());
            beatsDone_++;
            bool last = pins_.axiSlaveGetRLast() || beatsDone_ == totalBeats_;
            if (last) {
                pkt_->makeResponse();
                if (worstResp_ == static_cast<uint8_t>(AxiResp::SlvErr))
                    pkt_->setBadCommand();
                else if (worstResp_ == static_cast<uint8_t>(AxiResp::DecErr))
                    pkt_->setBadAddress();
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
Axi4SlaveEngine::tick()
{
    pins_.axiSetClk(0);
    driveInputs();
    pins_.axiEval();

    sampleAndAdvance();

    // Rising edge: the RTL's registers actually commit here.
    pins_.axiSetClk(1);
    pins_.axiEval();
    pins_.axiSetClk(0);

    if (pendingDone_) {
        auto done = pendingDone_;
        pendingDone_ = nullptr;
        done();
    }
}

} // namespace axion
} // namespace gem5
