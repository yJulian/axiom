#include "axi/axi4_master_engine.hh"

#include <algorithm>
#include <cstring>
#include <limits>

#include "base/logging.hh"

namespace gem5
{
namespace axion
{

Axi4MasterEngine::Axi4MasterEngine(Axi4MasterPins &pins, Backend &backend)
    : pins_(pins), backend_(backend)
{}

void
Axi4MasterEngine::completeRead(uint64_t seq, const uint8_t *data,
                                unsigned size)
{
    auto it = reads_.find(seq);
    if (it == reads_.end())
        return;
    it->second.data.assign(data, data + size);
    it->second.dataReady = true;
}

void
Axi4MasterEngine::completeWrite(uint64_t seq)
{
    auto it = writes_.find(seq);
    if (it == writes_.end())
        return;
    it->second.respReady = true;
}

uint64_t
Axi4MasterEngine::pickOldestEligibleRead() const
{
    uint64_t best = UINT64_MAX;
    for (auto &kv : readOrder_) {
        if (kv.second.empty())
            continue;
        uint64_t frontSeq = kv.second.front();
        auto it = reads_.find(frontSeq);
        if (it == reads_.end() || !it->second.dataReady)
            continue;
        if (best == UINT64_MAX || frontSeq < best)
            best = frontSeq;
    }
    return best;
}

uint64_t
Axi4MasterEngine::pickOldestEligibleWrite() const
{
    uint64_t best = UINT64_MAX;
    for (auto &kv : writeOrder_) {
        if (kv.second.empty())
            continue;
        uint64_t frontSeq = kv.second.front();
        auto it = writes_.find(frontSeq);
        if (it == writes_.end() || !it->second.respReady)
            continue;
        if (best == UINT64_MAX || frontSeq < best)
            best = frontSeq;
    }
    return best;
}

void
Axi4MasterEngine::sampleAr()
{
    // Always ready to accept a new read address beat -- backpressure
    // toward the DUT is not modeled at the AR channel itself; the memory
    // system's real latency shows up later, on the R channel, via
    // pickOldestEligibleRead() gating when data is actually presented.
    pins_.axiMasterSetArReady(1);
    if (!pins_.axiMasterGetArValid())
        return;

    AxiId id = pins_.axiMasterGetArId();
    Addr addr = pins_.axiMasterGetArAddr();
    unsigned len = pins_.axiMasterGetArLen();
    unsigned size = 1u << pins_.axiMasterGetArSize();
    unsigned totalBytes = (len + 1) * size;

    uint8_t burst = pins_.axiMasterGetArBurst();
    panic_if(burst != static_cast<uint8_t>(AxiBurst::Incr),
             "Axi4MasterEngine: ARBURST=%d (FIXED/WRAP) requested, but "
             "only INCR is supported -- Backend::issueRead() models a "
             "single linear gem5 memory access per burst, not per-beat "
             "addressing.", burst);

    uint64_t seq = nextSeq_++;
    ReadXact x;
    x.seq = seq;
    x.id = id;
    x.addr = addr;
    x.size = totalBytes;
    x.beatBytes = size;
    x.attrs.lock = pins_.axiMasterGetArLock();
    x.attrs.cache = pins_.axiMasterGetArCache();
    x.attrs.prot = pins_.axiMasterGetArProt();
    x.attrs.qos = pins_.axiMasterGetArQos();
    x.attrs.region = pins_.axiMasterGetArRegion();
    reads_.emplace(seq, std::move(x));
    readOrder_[id].push_back(seq);
    backend_.issueRead(seq, addr, totalBytes);
}

void
Axi4MasterEngine::sampleAwAndW()
{
    pins_.axiMasterSetAwReady(1);
    pins_.axiMasterSetWReady(1);

    if (currentWriteSeq_ == UINT64_MAX && pins_.axiMasterGetAwValid()) {
        AxiId id = pins_.axiMasterGetAwId();
        Addr addr = pins_.axiMasterGetAwAddr();
        unsigned len = pins_.axiMasterGetAwLen();
        unsigned size = 1u << pins_.axiMasterGetAwSize();

        uint8_t burst = pins_.axiMasterGetAwBurst();
        panic_if(burst != static_cast<uint8_t>(AxiBurst::Incr),
                 "Axi4MasterEngine: AWBURST=%d (FIXED/WRAP) requested, but "
                 "only INCR is supported -- Backend::issueWrite() models a "
                 "single linear gem5 memory access per burst, not per-beat "
                 "addressing.", burst);

        uint64_t seq = nextSeq_++;
        WriteXact x;
        x.seq = seq;
        x.id = id;
        x.addr = addr;
        x.data.reserve((len + 1) * size);
        x.attrs.lock = pins_.axiMasterGetAwLock();
        x.attrs.cache = pins_.axiMasterGetAwCache();
        x.attrs.prot = pins_.axiMasterGetAwProt();
        x.attrs.qos = pins_.axiMasterGetAwQos();
        x.attrs.region = pins_.axiMasterGetAwRegion();
        writes_.emplace(seq, std::move(x));
        writeOrder_[id].push_back(seq);
        currentWriteSeq_ = seq;
    }

    if (currentWriteSeq_ != UINT64_MAX && pins_.axiMasterGetWValid()) {
        auto &x = writes_.at(currentWriteSeq_);
        uint64_t data = pins_.axiMasterGetWData();
        uint64_t strb = pins_.axiMasterGetWStrb();
        unsigned beatBytes = 8;
        size_t off = x.data.size();
        x.data.resize(off + beatBytes, 0);
        for (unsigned b = 0; b < beatBytes; b++) {
            if (strb & (1ull << b))
                x.data[off + b] = static_cast<uint8_t>(data >> (8 * b));
        }
        if (pins_.axiMasterGetWLast()) {
            backend_.issueWrite(x.seq, x.addr,
                                 static_cast<unsigned>(x.data.size()),
                                 x.data.data());
            currentWriteSeq_ = UINT64_MAX;
        }
    }
}

void
Axi4MasterEngine::driveR()
{
    // Advance the in-flight transaction once its currently-presented beat
    // handshakes (RVALID && RREADY); otherwise keep re-presenting the same
    // beat below. Only erase/pop the transaction once its *last* beat has
    // handshaken -- multi-beat reads (ARLEN > 0) stream beatsSent beats
    // one per cycle, RLAST asserted only on the final one.
    if (rInFlightSeq_ != UINT64_MAX) {
        auto it = reads_.find(rInFlightSeq_);
        if (it == reads_.end()) {
            rInFlightSeq_ = UINT64_MAX;
        } else if (pins_.axiMasterGetRReady()) {
            auto &x = it->second;
            x.beatsSent++;
            unsigned totalBeats = (x.size + x.beatBytes - 1) / x.beatBytes;
            if (x.beatsSent >= totalBeats) {
                readOrder_[x.id].pop_front();
                reads_.erase(it);
                rInFlightSeq_ = UINT64_MAX;
            }
        }
    }

    if (rInFlightSeq_ == UINT64_MAX) {
        uint64_t seq = pickOldestEligibleRead();
        if (seq == UINT64_MAX) {
            pins_.axiMasterSetRValid(0);
            return;
        }
        rInFlightSeq_ = seq;
    }

    auto &x = reads_.at(rInFlightSeq_);
    unsigned totalBeats = (x.size + x.beatBytes - 1) / x.beatBytes;
    unsigned off = x.beatsSent * x.beatBytes;
    unsigned n = std::min<unsigned>(x.beatBytes, x.size - off);
    uint64_t data = 0;
    std::memcpy(&data, x.data.data() + off, n);
    pins_.axiMasterSetRId(x.id);
    pins_.axiMasterSetRData(data);
    pins_.axiMasterSetRResp(static_cast<uint8_t>(AxiResp::Okay));
    pins_.axiMasterSetRLast(x.beatsSent == totalBeats - 1 ? 1 : 0);
    pins_.axiMasterSetRValid(1);
}

void
Axi4MasterEngine::driveB()
{
    if (bInFlightSeq_ != UINT64_MAX) {
        auto it = writes_.find(bInFlightSeq_);
        if (it != writes_.end() && pins_.axiMasterGetBReady()) {
            writeOrder_[it->second.id].pop_front();
            writes_.erase(it);
            bInFlightSeq_ = UINT64_MAX;
        } else {
            return;
        }
    }

    uint64_t seq = pickOldestEligibleWrite();
    if (seq == UINT64_MAX) {
        pins_.axiMasterSetBValid(0);
        return;
    }

    auto &x = writes_.at(seq);
    pins_.axiMasterSetBId(x.id);
    pins_.axiMasterSetBResp(static_cast<uint8_t>(AxiResp::Okay));
    pins_.axiMasterSetBValid(1);
    bInFlightSeq_ = seq;
}

void
Axi4MasterEngine::tick(bool driveClock)
{
    if (driveClock)
        pins_.axiSetClk(0);

    sampleAr();
    sampleAwAndW();
    driveR();
    driveB();

    pins_.axiEval();

    if (driveClock) {
        pins_.axiSetClk(1);
        pins_.axiEval();
        pins_.axiSetClk(0);
    }
}

} // namespace axion
} // namespace gem5
