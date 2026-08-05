/*
 * FifoPioAccel: the leaf SimObject completing RTLPioDevice for this
 * example. Implements axion::Axi4SlavePins directly against the
 * Verilator-generated Vfifo_pio_top -- no dlopen/abstract-interface
 * indirection, per AXION's design (see src/axi/verilated_model.hh).
 */

#ifndef __FIFO_PIO_ACCEL_FIFO_PIO_DEVICE_HH__
#define __FIFO_PIO_ACCEL_FIFO_PIO_DEVICE_HH__

#include "Vfifo_pio_top.h"
#include "axi/verilated_model.hh"
#include "dev/rtl/rtl_pio_device.hh"
#include "params/FifoPioAccel.hh"

namespace gem5
{

class FifoPioAccel : public RTLPioDevice
{
  public:
    PARAMS(FifoPioAccel);
    explicit FifoPioAccel(const Params &p);

  protected:
    void axiSetClk(uint8_t val) override;
    void axiSetRstN(uint8_t val) override;
    void axiEval() override;

    void axiSlaveSetAwId(axion::AxiId id) override;
    void axiSlaveSetAwAddr(Addr addr) override;
    void axiSlaveSetAwLen(uint8_t len) override;
    void axiSlaveSetAwSize(uint8_t size) override;
    void axiSlaveSetAwBurst(uint8_t burst) override;
    void axiSlaveSetAwValid(uint8_t val) override;
    uint8_t axiSlaveGetAwReady() override;

    void axiSlaveSetWData(uint64_t data) override;
    void axiSlaveSetWStrb(uint64_t strb) override;
    void axiSlaveSetWLast(uint8_t val) override;
    void axiSlaveSetWValid(uint8_t val) override;
    uint8_t axiSlaveGetWReady() override;

    axion::AxiId axiSlaveGetBId() override;
    uint8_t axiSlaveGetBResp() override;
    uint8_t axiSlaveGetBValid() override;
    void axiSlaveSetBReady(uint8_t val) override;

    void axiSlaveSetArId(axion::AxiId id) override;
    void axiSlaveSetArAddr(Addr addr) override;
    void axiSlaveSetArLen(uint8_t len) override;
    void axiSlaveSetArSize(uint8_t size) override;
    void axiSlaveSetArBurst(uint8_t burst) override;
    void axiSlaveSetArValid(uint8_t val) override;
    uint8_t axiSlaveGetArReady() override;

    axion::AxiId axiSlaveGetRId() override;
    uint64_t axiSlaveGetRData() override;
    uint8_t axiSlaveGetRResp() override;
    uint8_t axiSlaveGetRLast() override;
    uint8_t axiSlaveGetRValid() override;
    void axiSlaveSetRReady(uint8_t val) override;

  private:
    axion::VerilatedRtlModel<Vfifo_pio_top> rtl_;
};

} // namespace gem5

#endif // __FIFO_PIO_ACCEL_FIFO_PIO_DEVICE_HH__
