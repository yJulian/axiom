#include "dev/rtl/rtl_pcie_tlp_device.hh"

#include <cstring>

#include "base/logging.hh"

namespace gem5
{

RTLPcieTlpDevice::RTLPcieTlpDevice(const Params &p)
    : PciEndpoint(p),
      rtlPioDelay(p.rtl_pio_latency),
      resetCycles(p.reset_cycles),
      idleGateCycles(p.idle_gate_cycles),
      rtlPio(name() + ".pio", *this),
      rangeForwarder(name() + ".range_forwarder", *this),
      completerEngine(*this, p.requester_id),
      requesterEngine(*this, *this, p.completer_id),
      tickEvent([this] { tick(); }, name()),
      resetCyclesLeft(p.reset_cycles)
{}

Port &
RTLPcieTlpDevice::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "pio")
        return rtlPio;
    return PciEndpoint::getPort(if_name, idx);
}

void
RTLPcieTlpDevice::init()
{
    panic_if(!rtlPio.isConnected(),
             "Pio port of %s not connected to anything!", name());
    panic_if(!dmaPort.isConnected(),
             "DMA port of %s not connected to anything!", name());

    // Give the inherited (and otherwise unused) pioPort a peer whose only
    // behavior is to forward range changes to rtlPio -- see
    // RTLPciDevice::RangeForwardPort's comment for the full reasoning.
    // Must happen before anything can write a BAR or the COMMAND
    // register, hence init() rather than startup().
    rangeForwarder.bind(pioPort);

    PciEndpoint::init();
}

void
RTLPcieTlpDevice::startup()
{
    schedule(tickEvent, clockEdge(Cycles(1)));
}

bool
RTLPcieTlpDevice::isConfigAccess(PacketPtr pkt) const
{
    return upstreamInterface.configRange().contains(pkt->getAddr());
}

bool
RTLPcieTlpDevice::RtlPioPort::recvTimingReq(PacketPtr pkt)
{
    panic_if(!pkt->isRead() && !pkt->isWrite(),
             "%s: unsupported command %s\n", name(), pkt->cmdString());

    Tick recv_delay = pkt->headerDelay + pkt->payloadDelay;
    pkt->headerDelay = pkt->payloadDelay = 0;

    if (dev.isConfigAccess(pkt)) {
        // Config space lives entirely in PciDevice's own header state --
        // no RTL involvement, so it can be answered right here.
        // PciDevice::read/write dispatch on the address themselves and
        // return the configured latency.
        Tick latency = pkt->isWrite() ? dev.write(pkt) : dev.read(pkt);
        schedTimingResp(pkt, curTick() + latency + recv_delay);
        return true;
    }

    dev.pioQueue.push_back({pkt, recv_delay});
    dev.wakeUp();
    return true;
}

Tick
RTLPcieTlpDevice::RtlPioPort::recvAtomic(PacketPtr pkt)
{
    // Config space works atomically and functionally (gem5 itself reads
    // it that way during setup); a BAR access cannot, because completing
    // it means running the RTL's clock, which only the tick loop does.
    if (dev.isConfigAccess(pkt))
        return pkt->isWrite() ? dev.write(pkt) : dev.read(pkt);

    panic("%s: BAR accesses to an RTL-backed PCIe device are timing-only; "
          "atomic/functional access is not supported (use "
          "mem_mode='timing')\n", name());
}

Tick
RTLPcieTlpDevice::readDevice(PacketPtr pkt)
{
    panic("%s: unexpected atomic BAR read; RtlPioPort queues BAR accesses "
          "for the RTL instead of routing them through PciDevice::read()\n",
          name());
}

Tick
RTLPcieTlpDevice::writeDevice(PacketPtr pkt)
{
    panic("%s: unexpected atomic BAR write; RtlPioPort queues BAR accesses "
          "for the RTL instead of routing them through PciDevice::write()\n",
          name());
}

void
RTLPcieTlpDevice::wakeUp()
{
    idleCycles = 0;
    if (!tickEvent.scheduled())
        schedule(tickEvent, clockEdge(Cycles(1)));
}

void
RTLPcieTlpDevice::driveResetInputs()
{
    tlpSetRstN(0);
    tlpEval();
    tlpSetClk(1);
    tlpEval();
    tlpSetClk(0);
}

void
RTLPcieTlpDevice::pioStart()
{
    if (completerEngine.busy() || pioQueue.empty())
        return;

    PioRequest req = pioQueue.front();
    PacketPtr pkt = req.pkt;

    // Hand the RTL a BAR-relative offset, not the address the guest's
    // enumeration happened to land on. This is what a PCIe hard IP block
    // does in silicon, and it means the DUT's register map is a fixed
    // set of offsets independent of where Linux (or a bare-metal test)
    // decides to map BAR0.
    int bar = 0;
    Addr offset = 0;
    if (!getBAR(pkt->getAddr(), bar, offset)) {
        // The bus only routes addresses this device advertised, so this
        // means our advertised ranges and our BAR decode disagree.
        panic("%s: address %#x is in no BAR of this device\n",
              name(), pkt->getAddr());
    }

    pioQueue.pop_front();
    Tick recvDelay = req.recvDelay;
    completerEngine.issue(pkt, offset, [this, pkt, recvDelay] {
        rtlPio.schedTimingResp(pkt, curTick() + rtlPioDelay + recvDelay);
    });
}

void
RTLPcieTlpDevice::issueRead(uint64_t seq, Addr addr, unsigned size)
{
    auto *buf = new uint8_t[size];
    auto *ev = new EventFunctionWrapper(
        [this, seq, buf, size] {
            requesterEngine.completeRead(seq, buf, size);
            delete[] buf;
            wakeUp();
        },
        name(), true);
    // The addresses come straight out of the DUT's own request TLP
    // headers, i.e. PCI bus addresses; the host maps those into system
    // addresses (a no-op with HiFive's pci_dma_base of 0, but not in
    // general -- and getting it wrong is invisible until someone
    // configures a nonzero base).
    dmaRead(pciToDma(addr), size, ev, buf);
}

void
RTLPcieTlpDevice::issueWrite(uint64_t seq, Addr addr, unsigned size,
                          const uint8_t *data)
{
    auto *buf = new uint8_t[size];
    std::memcpy(buf, data, size);
    auto *ev = new EventFunctionWrapper(
        [this, seq, buf] {
            requesterEngine.completeWrite(seq);
            delete[] buf;
            wakeUp();
        },
        name(), true);
    dmaWrite(pciToDma(addr), size, ev, buf);
}

void
RTLPcieTlpDevice::sampleIrq()
{
    bool now = rtlGetIrq() != 0;
    if (now == irqAsserted)
        return;

    irqAsserted = now;
    if (now)
        intrPost();
    else
        intrClear();
}

void
RTLPcieTlpDevice::tick()
{
    if (!resetDone) {
        driveResetInputs();
        if (resetCyclesLeft > 0) {
            resetCyclesLeft--;
        } else {
            tlpSetRstN(1);
            resetDone = true;
        }
        schedule(tickEvent, clockEdge(Cycles(1)));
        return;
    }

    pioStart();
    completerEngine.tick();
    // completerEngine.tick() already toggled the shared Verilated model's
    // clk pin this cycle, so the requester engine must not toggle it again
    // -- see RTLDmaDevice::tick() for the full explanation of what a
    // double toggle breaks.
    requesterEngine.tick(false);
    sampleIrq();

    // Idle clock gating. Note the extra condition versus RTLDmaDevice: an
    // asserted interrupt line still has to be sampled for its falling
    // edge, and nothing on the gem5 side calls wakeUp() when software
    // acknowledges it -- that write is a BAR access, which does wake us,
    // but the deassertion happens a cycle or two later inside the RTL.
    // Staying awake while irq is asserted keeps that edge observable.
    if (pioQueue.empty() && !completerEngine.busy() &&
        requesterEngine.idle() && !irqAsserted && isIdle()) {
        idleCycles++;
        if (idleGateCycles > 0 && idleCycles >= idleGateCycles)
            return;
    } else {
        idleCycles = 0;
    }

    schedule(tickEvent, clockEdge(Cycles(1)));
}

} // namespace gem5
