/*
 * RTLDmaDevice: a gem5 DmaDevice bridged to a Verilator-simulated RTL
 * block over two full AXI4 ports: a slave port for control/status
 * registers (same Axi4SlaveEngine as RTLPioDevice) and a master port for
 * DMA into system memory (Axi4MasterEngine, with real ID-ordered
 * out-of-order completion). DmaDevice already extends PioDevice in gem5,
 * so RTLDmaDevice is transitively a PioDevice too -- no separate
 * inheritance from RTLPioDevice is needed (that would create a duplicate
 * PioDevice base); the two device classes share code via composition of
 * the same engine classes instead.
 */

#ifndef __DEV_RTL_RTL_DMA_DEVICE_HH__
#define __DEV_RTL_RTL_DMA_DEVICE_HH__

#include <deque>

#include "axi/axi4_master_engine.hh"
#include "axi/axi4_slave_engine.hh"
#include "axi/axi4_types.hh"
#include "dev/dma_device.hh"
#include "mem/tport.hh"
#include "params/RTLDmaDevice.hh"

namespace gem5
{

/**
 * abstract = True in RTLDmaDevice.py: instantiate a leaf class that
 * implements both axion::Axi4SlavePins (control/status port) and
 * axion::Axi4MasterPins (DMA port) against a concrete Verilated top.
 */
class RTLDmaDevice : public DmaDevice,
                      public axion::Axi4SlavePins,
                      public axion::Axi4MasterPins,
                      private axion::Axi4MasterEngine::Backend
{
  protected:
    class RtlPioPort : public SimpleTimingPort
    {
        RTLDmaDevice &dev;

      public:
        RtlPioPort(const std::string &name, RTLDmaDevice &dev)
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

    Addr pioAddr;
    Addr pioSize;
    Tick pioDelay;
    unsigned resetCycles;

    RtlPioPort rtlPio;
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

    void tick();
    void wakeUp();
    void pioStart();
    void driveResetInputs();

    // Axi4MasterEngine::Backend -- issues the actual gem5 DMA operations
    // once the master engine has accepted a full AW/W or AR burst.
    void issueRead(uint64_t seq, Addr addr, unsigned size) override;
    void issueWrite(uint64_t seq, Addr addr, unsigned size,
                     const uint8_t *data) override;

  public:
    PARAMS(RTLDmaDevice);
    explicit RTLDmaDevice(const Params &p);

    AddrRangeList getAddrRanges() const override;

    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;
    void init() override;
    void startup() override;
};

} // namespace gem5

#endif // __DEV_RTL_RTL_DMA_DEVICE_HH__
