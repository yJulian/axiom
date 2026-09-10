/*
 * PcieTemplateAccel: the leaf SimObject completing RTLPciDevice for the
 * PCIe worked example. Implements both axion::Axi4SlavePins (the BAR0
 * window) and axion::Axi4MasterPins (bus-master DMA) directly against the
 * Verilator-generated Vpcie_template_top, plus the two sideband pins
 * RTLPciDevice needs that have no AXI4 equivalent: the interrupt output
 * and the busy indication driving clock gating. No dlopen/abstract-
 * interface indirection, per AXION's design (see
 * src/axi/verilated_model.hh).
 */

#ifndef __PCIE_TEMPLATE_ACCEL_PCIE_TEMPLATE_DEVICE_HH__
#define __PCIE_TEMPLATE_ACCEL_PCIE_TEMPLATE_DEVICE_HH__

#include "Vpcie_template_top.h"
#include "axi/verilated_model.hh"
#include "dev/rtl/rtl_pci_device.hh"
#include "params/PcieTemplateAccel.hh"

namespace gem5
{

class PcieTemplateAccel : public RTLPciDevice
{
  public:
    PARAMS(PcieTemplateAccel);
    explicit PcieTemplateAccel(const Params &p);

  protected:
    void axiSetClk(uint8_t val) override;
    void axiSetRstN(uint8_t val) override;
    void axiEval() override;

    // -- axion::Axi4SlavePins: the BAR0 window --
    void axiSlaveSetAwId(axion::AxiId id) override;
    void axiSlaveSetAwAddr(Addr addr) override;
    void axiSlaveSetAwLen(uint8_t len) override;
    void axiSlaveSetAwSize(uint8_t size) override;
    void axiSlaveSetAwBurst(uint8_t burst) override;
    void axiSlaveSetAwLock(uint8_t lock) override;
    void axiSlaveSetAwCache(uint8_t cache) override;
    void axiSlaveSetAwProt(uint8_t prot) override;
    void axiSlaveSetAwQos(uint8_t qos) override;
    void axiSlaveSetAwRegion(uint8_t region) override;
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
    void axiSlaveSetArLock(uint8_t lock) override;
    void axiSlaveSetArCache(uint8_t cache) override;
    void axiSlaveSetArProt(uint8_t prot) override;
    void axiSlaveSetArQos(uint8_t qos) override;
    void axiSlaveSetArRegion(uint8_t region) override;
    void axiSlaveSetArValid(uint8_t val) override;
    uint8_t axiSlaveGetArReady() override;

    axion::AxiId axiSlaveGetRId() override;
    uint64_t axiSlaveGetRData() override;
    uint8_t axiSlaveGetRResp() override;
    uint8_t axiSlaveGetRLast() override;
    uint8_t axiSlaveGetRValid() override;
    void axiSlaveSetRReady(uint8_t val) override;

    // -- axion::Axi4MasterPins: bus-master DMA port --
    axion::AxiId axiMasterGetAwId() override;
    Addr axiMasterGetAwAddr() override;
    uint8_t axiMasterGetAwLen() override;
    uint8_t axiMasterGetAwSize() override;
    uint8_t axiMasterGetAwBurst() override;
    uint8_t axiMasterGetAwLock() override;
    uint8_t axiMasterGetAwCache() override;
    uint8_t axiMasterGetAwProt() override;
    uint8_t axiMasterGetAwQos() override;
    uint8_t axiMasterGetAwRegion() override;
    uint8_t axiMasterGetAwValid() override;
    void axiMasterSetAwReady(uint8_t val) override;

    uint64_t axiMasterGetWData() override;
    uint64_t axiMasterGetWStrb() override;
    uint8_t axiMasterGetWLast() override;
    uint8_t axiMasterGetWValid() override;
    void axiMasterSetWReady(uint8_t val) override;

    void axiMasterSetBId(axion::AxiId id) override;
    void axiMasterSetBResp(uint8_t resp) override;
    void axiMasterSetBValid(uint8_t val) override;
    uint8_t axiMasterGetBReady() override;

    axion::AxiId axiMasterGetArId() override;
    Addr axiMasterGetArAddr() override;
    uint8_t axiMasterGetArLen() override;
    uint8_t axiMasterGetArSize() override;
    uint8_t axiMasterGetArBurst() override;
    uint8_t axiMasterGetArLock() override;
    uint8_t axiMasterGetArCache() override;
    uint8_t axiMasterGetArProt() override;
    uint8_t axiMasterGetArQos() override;
    uint8_t axiMasterGetArRegion() override;
    uint8_t axiMasterGetArValid() override;
    void axiMasterSetArReady(uint8_t val) override;

    void axiMasterSetRId(axion::AxiId id) override;
    void axiMasterSetRData(uint64_t data) override;
    void axiMasterSetRResp(uint8_t resp) override;
    void axiMasterSetRLast(uint8_t val) override;
    void axiMasterSetRValid(uint8_t val) override;
    uint8_t axiMasterGetRReady() override;

    // -- Sideband pins (no AXI4 equivalent, read directly) --
    uint8_t rtlGetIrq() override;
    bool isIdle() override;

  private:
    axion::VerilatedRtlModel<Vpcie_template_top> rtl_;
};

} // namespace gem5

#endif // __PCIE_TEMPLATE_ACCEL_PCIE_TEMPLATE_DEVICE_HH__
