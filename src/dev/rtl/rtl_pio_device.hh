/*
 * RTLPioDevice: a gem5 PioDevice bridged to a Verilator-simulated RTL
 * block over a full AXI4 slave port. The device's own address range and
 * MMIO queueing live here; the AXI4 pin handshake itself is delegated to
 * a composed Axi4SlaveEngine. A leaf class (e.g. FifoPioAccel) implements
 * the Axi4SlavePins virtuals against its specific Verilated top module.
 */

#ifndef __DEV_RTL_RTL_PIO_DEVICE_HH__
#define __DEV_RTL_RTL_PIO_DEVICE_HH__

#include <deque>

#include "axi/axi4_slave_engine.hh"
#include "axi/axi4_types.hh"
#include "dev/io_device.hh"
#include "mem/tport.hh"
#include "params/RTLPioDevice.hh"

namespace gem5
{

/**
 * abstract = True in RTLPioDevice.py: instantiate a leaf class that
 * implements axion::Axi4SlavePins against a concrete Verilated top
 * module (see examples/fifo_pio_accel/fifo_pio_device.hh).
 */
class RTLPioDevice : public PioDevice, public axion::Axi4SlavePins
{
  protected:
    // Timing PIO port: requests are queued and answered only once the
    // AXI4 handshake with the RTL has actually completed, so the
    // requester observes the real hardware latency. Mirrors
    // gem5_cva6's RtlAccelerator::AccelPioPort.
    class RtlPioPort : public SimpleTimingPort
    {
        RTLPioDevice &dev;

      public:
        RtlPioPort(const std::string &name, RTLPioDevice &dev)
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

  public:
    PARAMS(RTLPioDevice);
    explicit RTLPioDevice(const Params &p);

    AddrRangeList getAddrRanges() const override;

    // Legacy PioDevice entry points; never called (custom timing port
    // replaces the default one via getPort()).
    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;
    void init() override;
    void startup() override;
};

} // namespace gem5

#endif // __DEV_RTL_RTL_PIO_DEVICE_HH__
