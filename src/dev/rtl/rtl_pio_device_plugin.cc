#include "dev/rtl/rtl_pio_device_plugin.hh"

#include <dlfcn.h>

#include <string>

#include "base/logging.hh"

namespace gem5
{

namespace
{

template <class FnT>
FnT
resolveSymbol(void *handle, const char *name, const std::string &libPath)
{
    void *sym = dlsym(handle, name);
    if (!sym)
        fatal("RTLPioDevicePlugin: symbol '%s' not found in '%s': %s\n",
              name, libPath, dlerror());
    return reinterpret_cast<FnT>(sym);
}

} // namespace

RTLPioDevicePlugin::RTLPioDevicePlugin(const Params &p)
    : RTLPioDevice(p)
{
    loadPlugin(p.rtl_library);
}

RTLPioDevicePlugin::~RTLPioDevicePlugin()
{
    if (rtl_)
        destroy_(rtl_);
    if (libHandle_)
        dlclose(libHandle_);
}

void
RTLPioDevicePlugin::loadPlugin(const std::string &libPath)
{
    libHandle_ = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!libHandle_)
        fatal("RTLPioDevicePlugin: dlopen('%s') failed: %s\n", libPath,
              dlerror());

    auto create = resolveSymbol<axion_rtl_create_t>(
        libHandle_, "axion_rtl_create", libPath);
    destroy_ = resolveSymbol<axion_rtl_destroy_t>(
        libHandle_, "axion_rtl_destroy", libPath);
    setClk_ = resolveSymbol<axion_rtl_set_clk_t>(
        libHandle_, "axion_rtl_set_clk", libPath);
    setRstN_ = resolveSymbol<axion_rtl_set_rst_n_t>(
        libHandle_, "axion_rtl_set_rst_n", libPath);
    eval_ = resolveSymbol<axion_rtl_eval_t>(
        libHandle_, "axion_rtl_eval", libPath);
    auto hasSlavePort = resolveSymbol<axion_rtl_has_slave_port_t>(
        libHandle_, "axion_rtl_has_slave_port", libPath);
    slaveDrive_ = resolveSymbol<axion_rtl_slave_drive_t>(
        libHandle_, "axion_rtl_slave_drive", libPath);
    slaveSample_ = resolveSymbol<axion_rtl_slave_sample_t>(
        libHandle_, "axion_rtl_slave_sample", libPath);
    auto abiVersion = resolveSymbol<axion_rtl_abi_version_t>(
        libHandle_, "axion_rtl_abi_version", libPath);

    fatal_if(abiVersion() != AXION_RTL_PLUGIN_ABI_VERSION,
             "RTLPioDevicePlugin: '%s' reports plugin ABI version %u, "
             "expected %u\n",
             libPath, abiVersion(), AXION_RTL_PLUGIN_ABI_VERSION);
    fatal_if(!hasSlavePort(),
             "RTLPioDevicePlugin: '%s' does not implement an AXI4 slave "
             "port\n",
             libPath);

    rtl_ = create(name().c_str());
}

void
RTLPioDevicePlugin::axiSetClk(uint8_t val)
{
    setClk_(rtl_, val);
}

void
RTLPioDevicePlugin::axiSetRstN(uint8_t val)
{
    setRstN_(rtl_, val);
}

void
RTLPioDevicePlugin::axiEval()
{
    slaveDrive_(rtl_, &in_);
    eval_(rtl_);
    slaveSample_(rtl_, &out_);
}

void
RTLPioDevicePlugin::axiSlaveSetAwId(axion::AxiId id)
{
    in_.awid = id;
}

void
RTLPioDevicePlugin::axiSlaveSetAwAddr(Addr addr)
{
    in_.awaddr = addr;
}

void
RTLPioDevicePlugin::axiSlaveSetAwLen(uint8_t len)
{
    in_.awlen = len;
}

void
RTLPioDevicePlugin::axiSlaveSetAwSize(uint8_t size)
{
    in_.awsize = size;
}

void
RTLPioDevicePlugin::axiSlaveSetAwBurst(uint8_t burst)
{
    in_.awburst = burst;
}

void
RTLPioDevicePlugin::axiSlaveSetAwLock(uint8_t lock)
{
    in_.awlock = lock;
}

void
RTLPioDevicePlugin::axiSlaveSetAwCache(uint8_t cache)
{
    in_.awcache = cache;
}

void
RTLPioDevicePlugin::axiSlaveSetAwProt(uint8_t prot)
{
    in_.awprot = prot;
}

void
RTLPioDevicePlugin::axiSlaveSetAwQos(uint8_t qos)
{
    in_.awqos = qos;
}

void
RTLPioDevicePlugin::axiSlaveSetAwRegion(uint8_t region)
{
    in_.awregion = region;
}

void
RTLPioDevicePlugin::axiSlaveSetAwValid(uint8_t val)
{
    in_.awvalid = val;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetAwReady()
{
    return out_.awready;
}

void
RTLPioDevicePlugin::axiSlaveSetWData(uint64_t data)
{
    in_.wdata = data;
}

void
RTLPioDevicePlugin::axiSlaveSetWStrb(uint64_t strb)
{
    in_.wstrb = strb;
}

void
RTLPioDevicePlugin::axiSlaveSetWLast(uint8_t val)
{
    in_.wlast = val;
}

void
RTLPioDevicePlugin::axiSlaveSetWValid(uint8_t val)
{
    in_.wvalid = val;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetWReady()
{
    return out_.wready;
}

axion::AxiId
RTLPioDevicePlugin::axiSlaveGetBId()
{
    return out_.bid;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetBResp()
{
    return out_.bresp;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetBValid()
{
    return out_.bvalid;
}

void
RTLPioDevicePlugin::axiSlaveSetBReady(uint8_t val)
{
    in_.bready = val;
}

void
RTLPioDevicePlugin::axiSlaveSetArId(axion::AxiId id)
{
    in_.arid = id;
}

void
RTLPioDevicePlugin::axiSlaveSetArAddr(Addr addr)
{
    in_.araddr = addr;
}

void
RTLPioDevicePlugin::axiSlaveSetArLen(uint8_t len)
{
    in_.arlen = len;
}

void
RTLPioDevicePlugin::axiSlaveSetArSize(uint8_t size)
{
    in_.arsize = size;
}

void
RTLPioDevicePlugin::axiSlaveSetArBurst(uint8_t burst)
{
    in_.arburst = burst;
}

void
RTLPioDevicePlugin::axiSlaveSetArLock(uint8_t lock)
{
    in_.arlock = lock;
}

void
RTLPioDevicePlugin::axiSlaveSetArCache(uint8_t cache)
{
    in_.arcache = cache;
}

void
RTLPioDevicePlugin::axiSlaveSetArProt(uint8_t prot)
{
    in_.arprot = prot;
}

void
RTLPioDevicePlugin::axiSlaveSetArQos(uint8_t qos)
{
    in_.arqos = qos;
}

void
RTLPioDevicePlugin::axiSlaveSetArRegion(uint8_t region)
{
    in_.arregion = region;
}

void
RTLPioDevicePlugin::axiSlaveSetArValid(uint8_t val)
{
    in_.arvalid = val;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetArReady()
{
    return out_.arready;
}

axion::AxiId
RTLPioDevicePlugin::axiSlaveGetRId()
{
    return out_.rid;
}

uint64_t
RTLPioDevicePlugin::axiSlaveGetRData()
{
    return out_.rdata;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetRResp()
{
    return out_.rresp;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetRLast()
{
    return out_.rlast;
}

uint8_t
RTLPioDevicePlugin::axiSlaveGetRValid()
{
    return out_.rvalid;
}

void
RTLPioDevicePlugin::axiSlaveSetRReady(uint8_t val)
{
    in_.rready = val;
}

} // namespace gem5
