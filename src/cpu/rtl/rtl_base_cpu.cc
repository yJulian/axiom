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
    instAxiPins().axiSetRstN(0);
    instAxiPins().axiEval();
    if (&dataAxiPins() != &instAxiPins()) {
        dataAxiPins().axiSetRstN(0);
        dataAxiPins().axiEval();
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
    dataPort_.tick(/*driveClock=*/!sharedPins);

    schedule(tickEvent, clockEdge(Cycles(1)));
}

} // namespace gem5
