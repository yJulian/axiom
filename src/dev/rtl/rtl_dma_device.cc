#include "dev/rtl/rtl_dma_device.hh"

#include <cstring>

#include "base/logging.hh"

namespace gem5
{

RTLDmaDevice::RTLDmaDevice(const Params &p)
    : DmaDevice(p),
      pioAddr(p.pio_addr),
      pioSize(p.pio_size),
      pioDelay(p.pio_latency),
      resetCycles(p.reset_cycles),
      idleGateCycles(p.idle_gate_cycles),
      rtlPio(name() + ".pio", *this),
      slaveEngine(*this),
      masterEngine(*this, *this),
      tickEvent([this] { tick(); }, name()),
      resetCyclesLeft(p.reset_cycles)
{}

AddrRangeList
RTLDmaDevice::getAddrRanges() const
{
    return {RangeSize(pioAddr, pioSize)};
}

Tick
RTLDmaDevice::read(PacketPtr pkt)
{
    panic("%s: unexpected call to legacy pio read\n", name());
}

Tick
RTLDmaDevice::write(PacketPtr pkt)
{
    panic("%s: unexpected call to legacy pio write\n", name());
}

Port &
RTLDmaDevice::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "pio")
        return rtlPio;
    return DmaDevice::getPort(if_name, idx);
}

void
RTLDmaDevice::init()
{
    // Deliberately not calling DmaDevice::init(): it transitively calls
    // PioDevice::init(), which checks its own pioPort member -- but
    // getPort("pio") above never binds to that, rtlPio is bound instead,
    // so PioDevice::init() would panic on every config, connected or not.
    // Do the equivalent checks/range-change against the ports that
    // actually get used (dmaPort is DmaDevice's own, still valid to check
    // directly; it's just DmaDevice::init()'s call into PioDevice::init()
    // that's the problem).
    panic_if(!rtlPio.isConnected(),
             "Pio port of %s not connected to anything!", name());
    panic_if(!dmaPort.isConnected(),
             "DMA port of %s not connected to anything!", name());
    rtlPio.sendRangeChange();
}

void
RTLDmaDevice::startup()
{
    schedule(tickEvent, clockEdge(Cycles(1)));
}

bool
RTLDmaDevice::RtlPioPort::recvTimingReq(PacketPtr pkt)
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
RTLDmaDevice::RtlPioPort::recvAtomic(PacketPtr pkt)
{
    panic("%s: RTLDmaDevice is timing-only; atomic/functional accesses "
          "to the RTL are not supported (use mem_mode='timing')\n",
          name());
}

void
RTLDmaDevice::wakeUp()
{
    idleCycles = 0;
    if (!tickEvent.scheduled())
        schedule(tickEvent, clockEdge(Cycles(1)));
}

void
RTLDmaDevice::driveResetInputs()
{
    axiSetRstN(0);
    axiEval();
    axiSetClk(1);
    axiEval();
    axiSetClk(0);
}

void
RTLDmaDevice::pioStart()
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
RTLDmaDevice::issueRead(uint64_t seq, Addr addr, unsigned size)
{
    auto *buf = new uint8_t[size];
    auto *ev = new EventFunctionWrapper(
        [this, seq, buf, size] {
            masterEngine.completeRead(seq, buf, size);
            delete[] buf;
            wakeUp();
        },
        name(), true);
    dmaRead(addr, size, ev, buf);
}

void
RTLDmaDevice::issueWrite(uint64_t seq, Addr addr, unsigned size,
                          const uint8_t *data)
{
    auto *buf = new uint8_t[size];
    std::memcpy(buf, data, size);
    auto *ev = new EventFunctionWrapper(
        [this, seq, buf] {
            masterEngine.completeWrite(seq);
            delete[] buf;
            wakeUp();
        },
        name(), true);
    dmaWrite(addr, size, ev, buf);
}

void
RTLDmaDevice::tick()
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
    // slaveEngine.tick() already toggled the shared Verilated model's one
    // clk pin for this cycle (see its own tick()'s clk 0->1->0 sequence);
    // masterEngine shares the same underlying model, so it must NOT toggle
    // the clock a second time here, or the RTL's registers advance twice
    // per device cycle -- silently skipping every single-cycle handshake
    // window (e.g. an AXI4-Lite register slave's one-cycle AWREADY/ARREADY
    // pulse), which looks like a permanent protocol deadlock from the
    // gem5 side. See Axi4MasterEngine::tick()'s driveClock docstring.
    masterEngine.tick(false);

    // Idle clock gating: once the device has been quiescent (nothing
    // queued, no PIO/DMA transaction in flight, and the leaf reports the
    // RTL itself idle) for idleGateCycles consecutive cycles, stop the
    // clock; wakeUp() restarts it on the next PIO request or DMA
    // completion. Gating preserves all RTL state, so skipping cycles in a
    // stable quiescent state is behaviorally invisible; the hysteresis
    // covers the internal kick-off latency between a register write and
    // the RTL's busy indication actually rising.
    if (pioQueue.empty() && !slaveEngine.busy() && masterEngine.idle() &&
        isIdle()) {
        idleCycles++;
        if (idleGateCycles > 0 && idleCycles >= idleGateCycles)
            return;
    } else {
        idleCycles = 0;
    }

    schedule(tickEvent, clockEdge(Cycles(1)));
}

} // namespace gem5
