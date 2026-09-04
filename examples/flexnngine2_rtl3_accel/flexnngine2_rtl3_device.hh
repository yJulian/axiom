/*
 * Flexnngine2Rtl3Accel: the leaf SimObject completing RTLDmaDevice for the
 * rtl3 (scratchpad-fed) FleXNNgine2 GEMM accelerator port. Implements both
 * axion::Axi4SlavePins (control/status port) and axion::Axi4MasterPins
 * (DMA port) directly against the Verilator-generated
 * Vflexnngine2_rtl3_axion_top -- same direct-link pattern as
 * examples/dma_memcopy_accel/dma_memcopy_device.hh, just against the
 * wrapper top module in this directory instead of a DUT-native one (see
 * flexnngine2_rtl3_axion_top.sv's header for why a wrapper is needed here
 * and dma_memcopy isn't).
 */

#ifndef __FLEXNNGINE2_RTL3_ACCEL_FLEXNNGINE2_RTL3_DEVICE_HH__
#define __FLEXNNGINE2_RTL3_ACCEL_FLEXNNGINE2_RTL3_DEVICE_HH__

#include "Vflexnngine2_rtl3_axion_top.h"
#include "axi/verilated_model.hh"
#include "dev/rtl/rtl_dma_device.hh"
#include "params/Flexnngine2Rtl3Accel.hh"

namespace gem5
{

class Flexnngine2Rtl3Accel : public RTLDmaDevice
{
  public:
    PARAMS(Flexnngine2Rtl3Accel);
    explicit Flexnngine2Rtl3Accel(const Params &p);

  protected:
    void axiSetClk(uint8_t val) override;
    void axiSetRstN(uint8_t val) override;
    void axiEval() override;

    // RTLDmaDevice idle clock gating (see rtl_dma_device.hh's isIdle()
    // docstring): dma_busy_o is high for the whole duration of a job,
    // including the compute phase where neither AXI port has any traffic
    // in flight, so this is the correct hint -- unlike inferring idleness
    // from AXI activity alone, it doesn't gate the clock out from under an
    // in-progress compute phase.
    bool isIdle() override;

    // TEMPORARY, see flexnngine2_rtl3_axion_top.sv's dbg_state_o docstring:
    // packed {store,compute,load} engine states, exposed for whoever
    // continues debugging the still-open "job never signals DONE" issue
    // (see ../README.md) -- not part of the Axi4SlavePins/Axi4MasterPins
    // contract, just a raw hierarchical-reference debug tap.
    uint32_t debugEngineState();

    // -- axion::Axi4SlavePins: control/status register port --
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

    // -- axion::Axi4MasterPins: DMA port --
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

  private:
    axion::VerilatedRtlModel<Vflexnngine2_rtl3_axion_top> rtl_;
};

} // namespace gem5

#endif // __FLEXNNGINE2_RTL3_ACCEL_FLEXNNGINE2_RTL3_DEVICE_HH__
