/*
 * RTLPioDevicePlugin: concrete RTLPioDevice that dlopen()s a .so built by
 * plugin/rtl_plugin.mk (implementing src/axi/axi4_plugin_abi.h's stable C
 * ABI) instead of binding to a Verilator-generated top module at compile
 * time. Lets a new RTL model be plugged in via a `rtl_library` param alone
 * -- no new C++ leaf class or .py SimObject file, unlike the direct-link
 * path (see examples/fifo_pio_accel/fifo_pio_device.hh for that path's
 * worked example).
 *
 * Every Axi4SlavePins Set* call just writes into a member input struct;
 * the dlopen boundary is only actually crossed, batched, inside axiEval()
 * (one axion_rtl_slave_drive() + axion_rtl_eval() + axion_rtl_slave_sample()
 * per tick). Modeled on gem5_cva6's RtlAccelerator (~/gem5_cva6/src/accel/
 * rtl_accel.hh), which dlopen()s an AccelInterface the same way -- the
 * deliberate difference here is a plain C ABI rather than a dlopen'd C++
 * vtable, since C++ ABI stability across independently compiled .so files
 * isn't guaranteed.
 */

#ifndef __DEV_RTL_RTL_PIO_DEVICE_PLUGIN_HH__
#define __DEV_RTL_RTL_PIO_DEVICE_PLUGIN_HH__

#include <string>

#include "axi/axi4_plugin_abi.h"
#include "dev/rtl/rtl_pio_device.hh"
#include "params/RTLPioDevicePlugin.hh"

namespace gem5
{

class RTLPioDevicePlugin : public RTLPioDevice
{
  public:
    PARAMS(RTLPioDevicePlugin);
    explicit RTLPioDevicePlugin(const Params &p);
    ~RTLPioDevicePlugin();

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
    void *libHandle_ = nullptr;
    AxionRtlInstance *rtl_ = nullptr;

    axion_rtl_destroy_t destroy_ = nullptr;
    axion_rtl_set_clk_t setClk_ = nullptr;
    axion_rtl_set_rst_n_t setRstN_ = nullptr;
    axion_rtl_eval_t eval_ = nullptr;
    axion_rtl_slave_drive_t slaveDrive_ = nullptr;
    axion_rtl_slave_sample_t slaveSample_ = nullptr;

    AxionAxi4SlaveInputs in_{};
    AxionAxi4SlaveOutputs out_{};

    void loadPlugin(const std::string &libPath);
};

} // namespace gem5

#endif // __DEV_RTL_RTL_PIO_DEVICE_PLUGIN_HH__
