/*
 * RTLPcieTlpDevice: the TLP-level counterpart to RTLPciDevice. Both are
 * gem5 PciEndpoints backed by Verilator-simulated RTL, and both keep
 * config space, BAR decode and INTx on the gem5 side; what differs is the
 * interface the RTL sees.
 *
 *   RTLPciDevice     -- the RTL gets an ordinary AXI4 slave port for its
 *                       BAR window and an AXI4 master port for DMA. This
 *                       is what a PCIe-to-AXI bridge IP block presents,
 *                       and it is the path to use unless the point is the
 *                       PCIe protocol itself.
 *   RTLPcieTlpDevice -- the RTL gets genuine transaction-layer packets on
 *                       four AXI4-Stream channels and does its own header
 *                       parsing and building. This is what a PCIe hard
 *                       block's CQ/CC/RQ/RC interfaces look like.
 *
 * Structurally the two are near-identical: same deferred-response timing
 * port, same range-forwarder for gem5's direct pioPort.sendRangeChange()
 * calls, same interrupt-edge sampling, same idle clock gating. Only the
 * engines differ. That similarity is deliberate -- it is what makes the
 * two examples a fair comparison of the abstraction levels rather than of
 * two unrelated implementations.
 *
 * A leaf class implements axion::PcieTlpCompleterPins,
 * axion::PcieTlpRequesterPins and rtlGetIrq() against a concrete Verilated
 * top module (see examples/pcie_tlp_template_accel/).
 */

#ifndef __DEV_RTL_RTL_PCIE_TLP_DEVICE_HH__
#define __DEV_RTL_RTL_PCIE_TLP_DEVICE_HH__

#include <deque>

#include "dev/pci/device.hh"
#include "mem/tport.hh"
#include "params/RTLPcieTlpDevice.hh"
#include "pcie/pcie_tlp_engine.hh"
#include "pcie/pcie_tlp_types.hh"

namespace gem5
{

/**
 * abstract = True in RTLPcieTlpDevice.py.
 */
class RTLPcieTlpDevice : public PciEndpoint,
                         public axion::PcieTlpCompleterPins,
                         public axion::PcieTlpRequesterPins,
                         private axion::PcieTlpRequesterEngine::Backend
{
  protected:
    /** See RTLPciDevice::RtlPioPort -- identical role and reasoning. */
    class RtlPioPort : public SimpleTimingPort
    {
        RTLPcieTlpDevice &dev;

      public:
        RtlPioPort(const std::string &name, RTLPcieTlpDevice &dev)
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
     * See RTLPciDevice::RangeForwardPort for the full explanation: gem5's
     * config-space code calls pioPort.sendRangeChange() directly, and
     * pioPort is not the port that is actually connected here.
     */
    class RangeForwardPort : public RequestPort
    {
        RTLPcieTlpDevice &dev;

      public:
        RangeForwardPort(const std::string &name, RTLPcieTlpDevice &dev)
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
    axion::PcieTlpCompleterEngine completerEngine;
    axion::PcieTlpRequesterEngine requesterEngine;
    EventFunctionWrapper tickEvent;

    struct PioRequest
    {
        PacketPtr pkt;
        Tick recvDelay;
    };
    std::deque<PioRequest> pioQueue;

    bool resetDone = false;
    unsigned resetCyclesLeft;
    unsigned idleCycles = 0;
    bool irqAsserted = false;

    void tick();
    void wakeUp();
    void pioStart();
    void driveResetInputs();
    void sampleIrq();

    bool isConfigAccess(PacketPtr pkt) const;

    /** See RTLPciDevice::rtlGetIrq(). */
    virtual uint8_t rtlGetIrq() = 0;

    /** See RTLPciDevice::isIdle(). */
    virtual bool
    isIdle()
    {
        return false;
    }

    // PcieTlpRequesterEngine::Backend
    void issueRead(uint64_t seq, Addr addr, unsigned size) override;
    void issueWrite(uint64_t seq, Addr addr, unsigned size,
                     const uint8_t *data) override;

    Tick readDevice(PacketPtr pkt) override;
    Tick writeDevice(PacketPtr pkt) override;

  public:
    PARAMS(RTLPcieTlpDevice);
    explicit RTLPcieTlpDevice(const Params &p);

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;
    void init() override;
    void startup() override;
};

} // namespace gem5

#endif // __DEV_RTL_RTL_PCIE_TLP_DEVICE_HH__
