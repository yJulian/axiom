#include "dev/rtl/rtl_dma_device_plugin.hh"

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
        fatal("RTLDmaDevicePlugin: symbol '%s' not found in '%s': %s\n",
              name, libPath, dlerror());
    return reinterpret_cast<FnT>(sym);
}

} // namespace

RTLDmaDevicePlugin::RTLDmaDevicePlugin(const Params &p)
    : RTLDmaDevice(p)
{
    loadPlugin(p.rtl_library);
}

RTLDmaDevicePlugin::~RTLDmaDevicePlugin()
{
    if (rtl_)
        destroy_(rtl_);
    if (libHandle_)
        dlclose(libHandle_);
}

void
RTLDmaDevicePlugin::loadPlugin(const std::string &libPath)
{
    libHandle_ = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!libHandle_)
        fatal("RTLDmaDevicePlugin: dlopen('%s') failed: %s\n", libPath,
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
    auto hasMasterPort = resolveSymbol<axion_rtl_has_master_port_t>(
        libHandle_, "axion_rtl_has_master_port", libPath);
    slaveDrive_ = resolveSymbol<axion_rtl_slave_drive_t>(
        libHandle_, "axion_rtl_slave_drive", libPath);
    slaveSample_ = resolveSymbol<axion_rtl_slave_sample_t>(
        libHandle_, "axion_rtl_slave_sample", libPath);
    masterDrive_ = resolveSymbol<axion_rtl_master_drive_t>(
        libHandle_, "axion_rtl_master_drive", libPath);
    masterSample_ = resolveSymbol<axion_rtl_master_sample_t>(
        libHandle_, "axion_rtl_master_sample", libPath);
    auto abiVersion = resolveSymbol<axion_rtl_abi_version_t>(
        libHandle_, "axion_rtl_abi_version", libPath);

    fatal_if(abiVersion() != AXION_RTL_PLUGIN_ABI_VERSION,
             "RTLDmaDevicePlugin: '%s' reports plugin ABI version %u, "
             "expected %u\n",
             libPath, abiVersion(), AXION_RTL_PLUGIN_ABI_VERSION);
    fatal_if(!hasSlavePort(),
             "RTLDmaDevicePlugin: '%s' does not implement an AXI4 slave "
             "port\n",
             libPath);
    fatal_if(!hasMasterPort(),
             "RTLDmaDevicePlugin: '%s' does not implement an AXI4 master "
             "(DMA) port\n",
             libPath);

    rtl_ = create(name().c_str());
}

void
RTLDmaDevicePlugin::axiSetClk(uint8_t val)
{
    setClk_(rtl_, val);
}

void
RTLDmaDevicePlugin::axiSetRstN(uint8_t val)
{
    setRstN_(rtl_, val);
}

void
RTLDmaDevicePlugin::axiEval()
{
    slaveDrive_(rtl_, &slaveIn_);
    masterDrive_(rtl_, &masterIn_);
    eval_(rtl_);
    slaveSample_(rtl_, &slaveOut_);
    masterSample_(rtl_, &masterOut_);
}

// -- Axi4SlavePins --------------------------------------------------------

void
RTLDmaDevicePlugin::axiSlaveSetAwId(axion::AxiId id)
{
    slaveIn_.awid = id;
}

void
RTLDmaDevicePlugin::axiSlaveSetAwAddr(Addr addr)
{
    slaveIn_.awaddr = addr;
}

void
RTLDmaDevicePlugin::axiSlaveSetAwLen(uint8_t len)
{
    slaveIn_.awlen = len;
}

void
RTLDmaDevicePlugin::axiSlaveSetAwSize(uint8_t size)
{
    slaveIn_.awsize = size;
}

void
RTLDmaDevicePlugin::axiSlaveSetAwBurst(uint8_t burst)
{
    slaveIn_.awburst = burst;
}

void
RTLDmaDevicePlugin::axiSlaveSetAwValid(uint8_t val)
{
    slaveIn_.awvalid = val;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetAwReady()
{
    return slaveOut_.awready;
}

void
RTLDmaDevicePlugin::axiSlaveSetWData(uint64_t data)
{
    slaveIn_.wdata = data;
}

void
RTLDmaDevicePlugin::axiSlaveSetWStrb(uint64_t strb)
{
    slaveIn_.wstrb = strb;
}

void
RTLDmaDevicePlugin::axiSlaveSetWLast(uint8_t val)
{
    slaveIn_.wlast = val;
}

void
RTLDmaDevicePlugin::axiSlaveSetWValid(uint8_t val)
{
    slaveIn_.wvalid = val;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetWReady()
{
    return slaveOut_.wready;
}

axion::AxiId
RTLDmaDevicePlugin::axiSlaveGetBId()
{
    return slaveOut_.bid;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetBResp()
{
    return slaveOut_.bresp;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetBValid()
{
    return slaveOut_.bvalid;
}

void
RTLDmaDevicePlugin::axiSlaveSetBReady(uint8_t val)
{
    slaveIn_.bready = val;
}

void
RTLDmaDevicePlugin::axiSlaveSetArId(axion::AxiId id)
{
    slaveIn_.arid = id;
}

void
RTLDmaDevicePlugin::axiSlaveSetArAddr(Addr addr)
{
    slaveIn_.araddr = addr;
}

void
RTLDmaDevicePlugin::axiSlaveSetArLen(uint8_t len)
{
    slaveIn_.arlen = len;
}

void
RTLDmaDevicePlugin::axiSlaveSetArSize(uint8_t size)
{
    slaveIn_.arsize = size;
}

void
RTLDmaDevicePlugin::axiSlaveSetArBurst(uint8_t burst)
{
    slaveIn_.arburst = burst;
}

void
RTLDmaDevicePlugin::axiSlaveSetArValid(uint8_t val)
{
    slaveIn_.arvalid = val;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetArReady()
{
    return slaveOut_.arready;
}

axion::AxiId
RTLDmaDevicePlugin::axiSlaveGetRId()
{
    return slaveOut_.rid;
}

uint64_t
RTLDmaDevicePlugin::axiSlaveGetRData()
{
    return slaveOut_.rdata;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetRResp()
{
    return slaveOut_.rresp;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetRLast()
{
    return slaveOut_.rlast;
}

uint8_t
RTLDmaDevicePlugin::axiSlaveGetRValid()
{
    return slaveOut_.rvalid;
}

void
RTLDmaDevicePlugin::axiSlaveSetRReady(uint8_t val)
{
    slaveIn_.rready = val;
}

// -- Axi4MasterPins ---------------------------------------------------------

axion::AxiId
RTLDmaDevicePlugin::axiMasterGetAwId()
{
    return masterOut_.awid;
}

Addr
RTLDmaDevicePlugin::axiMasterGetAwAddr()
{
    return masterOut_.awaddr;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetAwLen()
{
    return masterOut_.awlen;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetAwSize()
{
    return masterOut_.awsize;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetAwBurst()
{
    return masterOut_.awburst;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetAwValid()
{
    return masterOut_.awvalid;
}

void
RTLDmaDevicePlugin::axiMasterSetAwReady(uint8_t val)
{
    masterIn_.awready = val;
}

uint64_t
RTLDmaDevicePlugin::axiMasterGetWData()
{
    return masterOut_.wdata;
}

uint64_t
RTLDmaDevicePlugin::axiMasterGetWStrb()
{
    return masterOut_.wstrb;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetWLast()
{
    return masterOut_.wlast;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetWValid()
{
    return masterOut_.wvalid;
}

void
RTLDmaDevicePlugin::axiMasterSetWReady(uint8_t val)
{
    masterIn_.wready = val;
}

void
RTLDmaDevicePlugin::axiMasterSetBId(axion::AxiId id)
{
    masterIn_.bid = id;
}

void
RTLDmaDevicePlugin::axiMasterSetBResp(uint8_t resp)
{
    masterIn_.bresp = resp;
}

void
RTLDmaDevicePlugin::axiMasterSetBValid(uint8_t val)
{
    masterIn_.bvalid = val;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetBReady()
{
    return masterOut_.bready;
}

axion::AxiId
RTLDmaDevicePlugin::axiMasterGetArId()
{
    return masterOut_.arid;
}

Addr
RTLDmaDevicePlugin::axiMasterGetArAddr()
{
    return masterOut_.araddr;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetArLen()
{
    return masterOut_.arlen;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetArSize()
{
    return masterOut_.arsize;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetArBurst()
{
    return masterOut_.arburst;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetArValid()
{
    return masterOut_.arvalid;
}

void
RTLDmaDevicePlugin::axiMasterSetArReady(uint8_t val)
{
    masterIn_.arready = val;
}

void
RTLDmaDevicePlugin::axiMasterSetRId(axion::AxiId id)
{
    masterIn_.rid = id;
}

void
RTLDmaDevicePlugin::axiMasterSetRData(uint64_t data)
{
    masterIn_.rdata = data;
}

void
RTLDmaDevicePlugin::axiMasterSetRResp(uint8_t resp)
{
    masterIn_.rresp = resp;
}

void
RTLDmaDevicePlugin::axiMasterSetRLast(uint8_t val)
{
    masterIn_.rlast = val;
}

void
RTLDmaDevicePlugin::axiMasterSetRValid(uint8_t val)
{
    masterIn_.rvalid = val;
}

uint8_t
RTLDmaDevicePlugin::axiMasterGetRReady()
{
    return masterOut_.rready;
}

} // namespace gem5
