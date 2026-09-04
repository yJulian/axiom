/*
 * RTLPciDevice: a gem5 PCIe endpoint bridged to a Verilator-simulated RTL
 * block. gem5 owns everything that is PCIe -- the 256-byte config header,
 * BAR decoding, INTx routing through the PCI host -- and the RTL sees only
 * two ordinary AXI4 ports plus an interrupt pin, which is exactly the
 * split a real FPGA design has between a PCIe hard IP block (Xilinx
 * PCIe-to-AXI Bridge / XDMA, Intel's equivalent) and the user logic behind
 * it. Concretely:
 *
 *   - the BAR window is driven through the same Axi4SlaveEngine
 *     RTLPioDevice/RTLDmaDevice use, but with BAR-relative offsets rather
 *     than the guest-visible physical address (see pioStart());
 *   - bus-master DMA runs through the same Axi4MasterEngine, with PCI
 *     bus addresses translated by pciToDma() before they reach gem5's
 *     memory system;
 *   - an interrupt output pin on the RTL is sampled every cycle and
 *     turned into PciDevice::intrPost()/intrClear() edges.
 *
 * PciDevice already extends DmaDevice (which extends PioDevice), so this
 * is transitively a PioDevice too -- same reasoning as RTLDmaDevice's:
 * behavior is shared with the other two RTL device bases by composing the
 * same engine classes, not by a common base.
 *
 * A leaf class implements axion::Axi4SlavePins, axion::Axi4MasterPins and
 * rtlGetIrq() against a concrete Verilated top module (see
 * examples/pcie_template_accel/pcie_template_device.hh).
 */

#ifndef __DEV_RTL_RTL_PCI_DEVICE_HH__
#define __DEV_RTL_RTL_PCI_DEVICE_HH__

#include <deque>

#include "axi/axi4_master_engine.hh"
#include "axi/axi4_slave_engine.hh"
#include "axi/axi4_types.hh"
#include "dev/pci/device.hh"
#include "mem/tport.hh"
#include "params/RTLPciDevice.hh"

namespace gem5
{

/**
 * abstract = True in RTLPciDevice.py: instantiate a leaf class that
 * implements both pin contracts plus rtlGetIrq() against a concrete
 * Verilated top module.
 */
class RTLPciDevice : public PciEndpoint,
                     public axion::Axi4SlavePins,
                     public axion::Axi4MasterPins,
                     private axion::Axi4MasterEngine::Backend
{
  protected:
    /**
     * Same deferred-response timing port RTLPioDevice/RTLDmaDevice use:
     * a BAR access is answered only once the AXI4 handshake with the RTL
     * has actually completed, so the requester observes real hardware
     * latency instead of a made-up constant.
     *
     * Config-space accesses are the exception and never reach the RTL at
     * all -- the DUT has no config space, just as a hard IP block's user
     * logic doesn't. They are answered here synchronously out of
     * PciDevice's own header state.
     */
    class RtlPioPort : public SimpleTimingPort
    {
        RTLPciDevice &dev;

      public:
        RtlPioPort(const std::string &name, RTLPciDevice &dev)
            : SimpleTimingPort(name, &dev), dev(dev)
        {}

      protected:
        bool recvTimingReq(PacketPtr pkt) override;
        Tick recvAtomic(PacketPtr pkt) override;

        AddrRangeList
        getAddrRanges() const override
        {
            return dev.getAddrRanges();
        }
    };

    /**
     * The one piece of glue a PCIe device needs that a plain PioDevice
     * does not.
     *
     * gem5's own config-space code re-advertises this device's address
     * ranges by calling `pioPort.sendRangeChange()` directly -- on every
     * BAR write and on every COMMAND-register write (see
     * PciDevice::writeConfig() and PciEndpoint::writeConfig() in
     * ext/gem5/src/dev/pci/device.cc), which is how a BAR window becomes
     * visible on the bus in the first place. But like the other RTL
     * device bases, getPort("pio") hands out `rtlPio` instead of the
     * inherited `pioPort`, leaving `pioPort` unbound -- and
     * ResponsePort::sendRangeChange() dereferences its peer
     * unconditionally, so those calls would segfault.
     *
     * Rather than reimplementing (and having to keep re-syncing) gem5's
     * BAR-decode logic just to redirect those calls, bind `pioPort` to
     * this stub, whose only job is to forward the range change on to the
     * port that is actually connected to the PCI bus. Nothing else ever
     * traverses it -- no packet is routed through `pioPort` at all.
     */
    class RangeForwardPort : public RequestPort
    {
        RTLPciDevice &dev;

      public:
        RangeForwardPort(const std::string &name, RTLPciDevice &dev)
            : RequestPort(name), dev(dev)
        {}

      protected:
        void recvRangeChange() override { dev.rtlPio.sendRangeChange(); }

        bool
        recvTimingResp(PacketPtr pkt) override
        {
            panic("%s: nothing is ever routed through this port\n", name());
        }

        void
        recvReqRetry() override
        {
            panic("%s: nothing is ever routed through this port\n", name());
        }
    };

    Tick rtlPioDelay;
    unsigned resetCycles;
    unsigned idleGateCycles;

    RtlPioPort rtlPio;
    RangeForwardPort rangeForwarder;
    axion::Axi4SlaveEngine slaveEngine;
    axion::Axi4MasterEngine masterEngine;
    EventFunctionWrapper tickEvent;

    struct PioRequest
    {
        PacketPtr pkt;
        Tick recvDelay;
    };
    std::deque<PioRequest> pioQueue;

    bool resetDone = false;
    unsigned resetCyclesLeft;

    // See RTLDmaDevice's identically-named member: consecutive quiescent
    // cycles before the RTL clock stops ticking. wakeUp() resets it.
    unsigned idleCycles = 0;

    // Last value seen on the RTL's interrupt pin, so only edges turn into
    // intrPost()/intrClear() calls rather than one per cycle.
    bool irqAsserted = false;

    void tick();
    void wakeUp();
    void pioStart();
    void driveResetInputs();
    void sampleIrq();

    /** True when `pkt` targets this device's config space, not a BAR. */
    bool isConfigAccess(PacketPtr pkt) const;

    /**
     * The RTL's interrupt output. Sampled every cycle; a 0->1 edge posts
     * an INTx assert through the PCI host, a 1->0 edge deasserts it.
     *
     * Note the PLIC source that ends up carrying this on RISC-V is
     * derived from the device's *slot*, not from its InterruptLine param:
     * GenericRiscvPciHost::mapPciInterrupt() returns
     * `int_base + (pci_dev % int_count)`, i.e. 0x10 + pci_dev with
     * HiFive's defaults.
     */
    virtual uint8_t rtlGetIrq() = 0;

    /**
     * Same opt-in clock-gating hint as RTLDmaDevice::isIdle(): true when
     * the RTL has no autonomous work in flight (e.g. a DMA "busy" pin is
     * low). Default false (never idle) keeps the always-ticking behavior
     * for leaves that don't override it.
     */
    virtual bool
    isIdle()
    {
        return false;
    }

    // Axi4MasterEngine::Backend -- issues the actual gem5 DMA operations
    // once the master engine has accepted a full AW/W or AR burst.
    void issueRead(uint64_t seq, Addr addr, unsigned size) override;
    void issueWrite(uint64_t seq, Addr addr, unsigned size,
                     const uint8_t *data) override;

    // PciDevice's two pure virtuals. Never reached on the timing path
    // (RtlPioPort queues BAR accesses instead of going through
    // PciDevice::read/write), but implemented rather than panicking so an
    // atomic or functional BAR access reports a clean failure.
    Tick readDevice(PacketPtr pkt) override;
    Tick writeDevice(PacketPtr pkt) override;

  public:
    PARAMS(RTLPciDevice);
    explicit RTLPciDevice(const Params &p);

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;
    void init() override;
    void startup() override;
};

} // namespace gem5

#endif // __DEV_RTL_RTL_PCI_DEVICE_HH__
