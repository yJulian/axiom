#include "cpu/rtl/rtl_base_cpu.hh"

#include <cstring>

#include "base/logging.hh"
#include "mem/packet_access.hh"
#include "mem/request.hh"

namespace gem5
{

RTLBaseCpu::RtlCorePort::RtlCorePort(const std::string &name, RTLBaseCpu &cpu,
                                      bool isInst)
    : RequestPort(name), cpu_(cpu), isInst_(isInst)
{}

void
RTLBaseCpu::RtlCorePort::bindPins(axion::Axi4MasterPins &pins)
{
    // Not std::make_unique: the Backend upcast of *this needs to happen
    // textually inside this member function to see the private
    // inheritance -- make_unique's implicit conversion happens inside
    // <memory>'s own template code, which has no such access, and gcc
    // (correctly) rejects it there.
    engine_.reset(new axion::Axi4MasterEngine(pins, *this));
}

void
RTLBaseCpu::RtlCorePort::tick(bool driveClock)
{
    if (engine_)
        engine_->tick(driveClock);
}

void
RTLBaseCpu::RtlCorePort::issueRead(uint64_t seq, Addr addr, unsigned size)
{
    auto req = std::make_shared<Request>(
        addr, size, Request::Flags(),
        isInst_ ? cpu_.instRequestorId() : cpu_.dataRequestorId());
    PacketPtr pkt = Packet::createRead(req);
    pkt->allocate();
    inFlight_[seq] = pkt;
    if (!sendTimingReq(pkt))
        retryQueue_.push_back(pkt);
}

void
RTLBaseCpu::RtlCorePort::issueWrite(uint64_t seq, Addr addr, unsigned size,
                                     const uint8_t *data)
{
    auto req = std::make_shared<Request>(
        addr, size, Request::Flags(),
        isInst_ ? cpu_.instRequestorId() : cpu_.dataRequestorId());
    PacketPtr pkt = Packet::createWrite(req);
    pkt->allocate();
    pkt->setData(data);
    inFlight_[seq] = pkt;
    if (!sendTimingReq(pkt))
        retryQueue_.push_back(pkt);
}

bool
RTLBaseCpu::RtlCorePort::recvTimingResp(PacketPtr pkt)
{
    uint64_t foundSeq = 0;
    bool found = false;
    for (auto &kv : inFlight_) {
        if (kv.second == pkt) {
            foundSeq = kv.first;
            found = true;
            break;
        }
    }
    panic_if(!found, "%s: response for unknown request\n", name());
    inFlight_.erase(foundSeq);

    if (pkt->isRead()) {
        engine_->completeRead(foundSeq, pkt->getConstPtr<uint8_t>(),
                               pkt->getSize());
    } else {
        engine_->completeWrite(foundSeq);
    }
    delete pkt;
    return true;
}

void
RTLBaseCpu::RtlCorePort::recvReqRetry()
{
    while (!retryQueue_.empty()) {
        PacketPtr pkt = retryQueue_.front();
        if (!sendTimingReq(pkt))
            break;
        retryQueue_.pop_front();
    }
}

RTLBaseCpu::RTLBaseCpu(const Params &p)
    : BaseCPU(p),
      instPort_(name() + ".inst_port", *this, /*isInst=*/true),
      dataPort_(name() + ".data_port", *this, /*isInst=*/false),
      tickEvent([this] { tick(); }, name()),
      resetCycles(p.reset_cycles),
      resetCyclesLeft(p.reset_cycles)
{}

void
RTLBaseCpu::init()
{
    BaseCPU::init();
    instPort_.bindPins(instAxiPins());
    dataPort_.bindPins(dataAxiPins());
}

void
RTLBaseCpu::startup()
{
    BaseCPU::startup();
    schedule(tickEvent, clockEdge(Cycles(1)));
}

void
RTLBaseCpu::wakeup(ThreadID tid)
{
    if (!tickEvent.scheduled())
        schedule(tickEvent, clockEdge(Cycles(1)));
}

void
RTLBaseCpu::driveResetInputs()
{
    // Must toggle the clock each reset cycle, not just eval once with
    // clk left wherever it was -- synchronous-reset flops only actually
    // sample rst_n on a real clock edge (matches RTLPioDevice's own
    // driveResetInputs(), which does the same axiSetClk(1)/eval/
    // axiSetClk(0) sequence; this one predates that pattern and never
    // toggled the clock at all during the whole reset-hold phase, so a
    // leaf's RTL only ever saw a single, solitary rising edge exactly
    // when reset_cycles elapsed -- functionally close to no reset at
    // all for a real synchronous design).
    instAxiPins().axiSetRstN(0);
    instAxiPins().axiEval();
    instAxiPins().axiSetClk(1);
    instAxiPins().axiEval();
    instAxiPins().axiSetClk(0);
    if (&dataAxiPins() != &instAxiPins()) {
        dataAxiPins().axiSetRstN(0);
        dataAxiPins().axiEval();
        dataAxiPins().axiSetClk(1);
        dataAxiPins().axiEval();
        dataAxiPins().axiSetClk(0);
    }
}

void
RTLBaseCpu::tick()
{
    if (!resetDone) {
        driveResetInputs();
        if (resetCyclesLeft > 0) {
            resetCyclesLeft--;
        } else {
            instAxiPins().axiSetRstN(1);
            resetDone = true;
        }
        schedule(tickEvent, clockEdge(Cycles(1)));
        return;
    }

    bool sharedPins = &dataAxiPins() == &instAxiPins();
    instPort_.tick(/*driveClock=*/true);
    // When the pins are shared (one unified AXI4 master port), dataPort_
    // must not tick its own engine at all -- not even with driveClock
    // false. Axi4MasterEngine::tick() unconditionally runs
    // sampleAr()/sampleAwAndW()/driveR()/driveB() regardless of
    // driveClock (that parameter only gates the clk_i toggle); two
    // independent engines both sampling/driving the *same* physical AXI
    // channel every cycle race each other for the same transactions --
    // both accept the same AR, both issue their own Backend::issueRead(),
    // and whichever engine's driveR() runs last silently overwrites the
    // other's R data on the shared pins. Only instPort_'s engine may
    // touch the pins in the shared case; dataPort_ stays a live gem5
    // RequestPort (still usable for icache_port/dcache_port wiring) but
    // simply never issues anything.
    if (!sharedPins)
        dataPort_.tick(/*driveClock=*/true);
    postTick();

    schedule(tickEvent, clockEdge(Cycles(1)));
}

} // namespace gem5
