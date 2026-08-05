#include "dev/rtl/rtl_pio_device.hh"

#include "base/logging.hh"

namespace gem5
{

RTLPioDevice::RTLPioDevice(const Params &p)
    : PioDevice(p),
      pioAddr(p.pio_addr),
      pioSize(p.pio_size),
      pioDelay(p.pio_latency),
      resetCycles(p.reset_cycles),
      rtlPio(name() + ".pio", *this),
      slaveEngine(*this),
      tickEvent([this] { tick(); }, name()),
      resetCyclesLeft(p.reset_cycles)
{}

AddrRangeList
RTLPioDevice::getAddrRanges() const
{
    return {RangeSize(pioAddr, pioSize)};
}

Tick
RTLPioDevice::read(PacketPtr pkt)
{
    panic("%s: unexpected call to legacy pio read\n", name());
}

Tick
RTLPioDevice::write(PacketPtr pkt)
{
    panic("%s: unexpected call to legacy pio write\n", name());
}

Port &
RTLPioDevice::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "pio")
        return rtlPio;
    return PioDevice::getPort(if_name, idx);
}

void
RTLPioDevice::init()
{
    PioDevice::init();
    panic_if(!rtlPio.isConnected(),
             "Pio port of %s not connected to anything!", name());
    rtlPio.sendRangeChange();
}

void
RTLPioDevice::startup()
{
    schedule(tickEvent, clockEdge(Cycles(1)));
}

bool
RTLPioDevice::RtlPioPort::recvTimingReq(PacketPtr pkt)
{
    panic_if(!pkt->isRead() && !pkt->isWrite(),
             "%s: unsupported command %s\n", name(), pkt->cmdString());

    Tick recv_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    dev.pioQueue.push_back({pkt, recv_delay});
    dev.wakeUp();
    return true;
}

Tick
RTLPioDevice::RtlPioPort::recvAtomic(PacketPtr pkt)
{
    panic("%s: RTLPioDevice is timing-only; atomic/functional accesses "
          "to the RTL are not supported (use mem_mode='timing')\n",
          name());
}

void
RTLPioDevice::wakeUp()
{
    if (!tickEvent.scheduled())
        schedule(tickEvent, clockEdge(Cycles(1)));
}

void
RTLPioDevice::driveResetInputs()
{
    axiSetRstN(0);
    axiEval();
    axiSetClk(1);
    axiEval();
    axiSetClk(0);
}

void
RTLPioDevice::pioStart()
{
    if (slaveEngine.busy() || pioQueue.empty())
        return;

    PioRequest req = pioQueue.front();
    pioQueue.pop_front();
    PacketPtr pkt = req.pkt;
    Tick recvDelay = req.recvDelay;

    slaveEngine.issue(pkt, [this, pkt, recvDelay] {
        rtlPio.schedTimingResp(pkt, curTick() + pioDelay + recvDelay);
    });
}

void
RTLPioDevice::tick()
{
    if (!resetDone) {
        driveResetInputs();
        if (resetCyclesLeft > 0) {
            resetCyclesLeft--;
        } else {
            axiSetRstN(1);
            resetDone = true;
        }
        schedule(tickEvent, clockEdge(Cycles(1)));
        return;
    }

    pioStart();
    slaveEngine.tick();

    if (!pioQueue.empty() || slaveEngine.busy())
        schedule(tickEvent, clockEdge(Cycles(1)));
}

} // namespace gem5
