/*
 * PcieTlpTemplateAccel: the leaf SimObject completing RTLPcieTlpDevice for
 * the TLP-level PCIe example. Implements both stream pin contracts plus the
 * two sideband pins, directly against the Verilator-generated
 * Vpcie_tlp_template_top -- no dlopen/abstract-interface indirection, per
 * AXION's design (see src/axi/verilated_model.hh).
 *
 * Note how much shorter this is than the AXI4 examples' pin accessors: 16
 * pins instead of 88, because a PCIe endpoint's four streams carry
 * addressing, length and byte enables inside the packet rather than on
 * sideband wires. The parsing that buys has to happen somewhere, and on
 * this path it happens in the RTL (pcie_tlp_template_accel.sv).
 */

#ifndef __PCIE_TLP_TEMPLATE_ACCEL_PCIE_TLP_TEMPLATE_DEVICE_HH__
#define __PCIE_TLP_TEMPLATE_ACCEL_PCIE_TLP_TEMPLATE_DEVICE_HH__

#include "Vpcie_tlp_template_top.h"
#include "axi/verilated_model.hh"
#include "dev/rtl/rtl_pcie_tlp_device.hh"
#include "params/PcieTlpTemplateAccel.hh"

namespace gem5
{

class PcieTlpTemplateAccel : public RTLPcieTlpDevice
{
  public:
    PARAMS(PcieTlpTemplateAccel);
    explicit PcieTlpTemplateAccel(const Params &p);

  protected:
    void tlpSetClk(uint8_t val) override;
    void tlpSetRstN(uint8_t val) override;
    void tlpEval() override;

    // -- axion::PcieTlpCompleterPins: CQ (host -> DUT), CC (DUT -> host) --
    void cqSetTData(uint64_t data) override;
    void cqSetTLast(uint8_t val) override;
    void cqSetTValid(uint8_t val) override;
    uint8_t cqGetTReady() override;

    uint64_t ccGetTData() override;
    uint8_t ccGetTLast() override;
    uint8_t ccGetTValid() override;
    void ccSetTReady(uint8_t val) override;

    // -- axion::PcieTlpRequesterPins: RQ (DUT -> host), RC (host -> DUT) --
    uint64_t rqGetTData() override;
    uint8_t rqGetTLast() override;
    uint8_t rqGetTValid() override;
    void rqSetTReady(uint8_t val) override;

    void rcSetTData(uint64_t data) override;
    void rcSetTLast(uint8_t val) override;
    void rcSetTValid(uint8_t val) override;
    uint8_t rcGetTReady() override;

    // -- Sideband pins (outside the TLP streams, read directly) --
    uint8_t rtlGetIrq() override;
    bool isIdle() override;

  private:
    axion::VerilatedRtlModel<Vpcie_tlp_template_top> rtl_;
};

} // namespace gem5

#endif // __PCIE_TLP_TEMPLATE_ACCEL_PCIE_TLP_TEMPLATE_DEVICE_HH__
