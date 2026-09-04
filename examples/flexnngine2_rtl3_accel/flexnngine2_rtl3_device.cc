#include "flexnngine2_rtl3_device.hh"

#include <cstdio>

namespace gem5
{

Flexnngine2Rtl3Accel::Flexnngine2Rtl3Accel(const Params &p)
    : RTLDmaDevice(p), rtl_(name())
{}

void
Flexnngine2Rtl3Accel::axiSetClk(uint8_t val)
{
    rtl_.top()->clk_i = val;
}

void
Flexnngine2Rtl3Accel::axiSetRstN(uint8_t val)
{
    rtl_.top()->rst_ni = val;
}

void
Flexnngine2Rtl3Accel::axiEval()
{
    rtl_.settle();
}

bool
Flexnngine2Rtl3Accel::isIdle()
{
    return !rtl_.top()->dma_busy_o;
}

uint32_t
Flexnngine2Rtl3Accel::debugEngineState()
{
    return rtl_.top()->dbg_state_o;
}

// -- axion::Axi4SlavePins: control/status register port --

void
Flexnngine2Rtl3Accel::axiSlaveSetAwId(axion::AxiId id)
{
    rtl_.top()->s_axi_awid = id;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwAddr(Addr addr)
{
    rtl_.top()->s_axi_awaddr = addr;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwLen(uint8_t len)
{
    rtl_.top()->s_axi_awlen = len;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwSize(uint8_t size)
{
    rtl_.top()->s_axi_awsize = size;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwBurst(uint8_t burst)
{
    rtl_.top()->s_axi_awburst = burst;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwLock(uint8_t lock)
{
    rtl_.top()->s_axi_awlock = lock;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwCache(uint8_t cache)
{
    rtl_.top()->s_axi_awcache = cache;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwProt(uint8_t prot)
{
    rtl_.top()->s_axi_awprot = prot;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwQos(uint8_t qos)
{
    rtl_.top()->s_axi_awqos = qos;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwRegion(uint8_t region)
{
    rtl_.top()->s_axi_awregion = region;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetAwValid(uint8_t val)
{
    rtl_.top()->s_axi_awvalid = val;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetAwReady()
{
    return rtl_.top()->s_axi_awready;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetWData(uint64_t data)
{
    rtl_.top()->s_axi_wdata = data;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetWStrb(uint64_t strb)
{
    rtl_.top()->s_axi_wstrb = strb;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetWLast(uint8_t val)
{
    rtl_.top()->s_axi_wlast = val;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetWValid(uint8_t val)
{
    rtl_.top()->s_axi_wvalid = val;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetWReady()
{
    return rtl_.top()->s_axi_wready;
}

axion::AxiId
Flexnngine2Rtl3Accel::axiSlaveGetBId()
{
    return rtl_.top()->s_axi_bid;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetBResp()
{
    return rtl_.top()->s_axi_bresp;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetBValid()
{
    return rtl_.top()->s_axi_bvalid;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetBReady(uint8_t val)
{
    rtl_.top()->s_axi_bready = val;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArId(axion::AxiId id)
{
    rtl_.top()->s_axi_arid = id;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArAddr(Addr addr)
{
    rtl_.top()->s_axi_araddr = addr;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArLen(uint8_t len)
{
    rtl_.top()->s_axi_arlen = len;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArSize(uint8_t size)
{
    rtl_.top()->s_axi_arsize = size;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArBurst(uint8_t burst)
{
    rtl_.top()->s_axi_arburst = burst;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArLock(uint8_t lock)
{
    rtl_.top()->s_axi_arlock = lock;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArCache(uint8_t cache)
{
    rtl_.top()->s_axi_arcache = cache;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArProt(uint8_t prot)
{
    rtl_.top()->s_axi_arprot = prot;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArQos(uint8_t qos)
{
    rtl_.top()->s_axi_arqos = qos;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArRegion(uint8_t region)
{
    rtl_.top()->s_axi_arregion = region;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetArValid(uint8_t val)
{
    rtl_.top()->s_axi_arvalid = val;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetArReady()
{
    return rtl_.top()->s_axi_arready;
}

axion::AxiId
Flexnngine2Rtl3Accel::axiSlaveGetRId()
{
    return rtl_.top()->s_axi_rid;
}

uint64_t
Flexnngine2Rtl3Accel::axiSlaveGetRData()
{
    return rtl_.top()->s_axi_rdata;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetRResp()
{
    return rtl_.top()->s_axi_rresp;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetRLast()
{
    return rtl_.top()->s_axi_rlast;
}

uint8_t
Flexnngine2Rtl3Accel::axiSlaveGetRValid()
{
    return rtl_.top()->s_axi_rvalid;
}

void
Flexnngine2Rtl3Accel::axiSlaveSetRReady(uint8_t val)
{
    rtl_.top()->s_axi_rready = val;
}

// -- axion::Axi4MasterPins: DMA port --

axion::AxiId
Flexnngine2Rtl3Accel::axiMasterGetAwId()
{
    return rtl_.top()->m_axi_awid;
}

Addr
Flexnngine2Rtl3Accel::axiMasterGetAwAddr()
{
    return rtl_.top()->m_axi_awaddr;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwLen()
{
    return rtl_.top()->m_axi_awlen;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwSize()
{
    return rtl_.top()->m_axi_awsize;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwBurst()
{
    return rtl_.top()->m_axi_awburst;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwLock()
{
    return rtl_.top()->m_axi_awlock;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwCache()
{
    return rtl_.top()->m_axi_awcache;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwProt()
{
    return rtl_.top()->m_axi_awprot;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwQos()
{
    return rtl_.top()->m_axi_awqos;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwRegion()
{
    return rtl_.top()->m_axi_awregion;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetAwValid()
{
    return rtl_.top()->m_axi_awvalid;
}

void
Flexnngine2Rtl3Accel::axiMasterSetAwReady(uint8_t val)
{
    rtl_.top()->m_axi_awready = val;
}

uint64_t
Flexnngine2Rtl3Accel::axiMasterGetWData()
{
    return rtl_.top()->m_axi_wdata;
}

uint64_t
Flexnngine2Rtl3Accel::axiMasterGetWStrb()
{
    return rtl_.top()->m_axi_wstrb;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetWLast()
{
    return rtl_.top()->m_axi_wlast;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetWValid()
{
    return rtl_.top()->m_axi_wvalid;
}

void
Flexnngine2Rtl3Accel::axiMasterSetWReady(uint8_t val)
{
    rtl_.top()->m_axi_wready = val;
}

void
Flexnngine2Rtl3Accel::axiMasterSetBId(axion::AxiId id)
{
    rtl_.top()->m_axi_bid = id;
}

void
Flexnngine2Rtl3Accel::axiMasterSetBResp(uint8_t resp)
{
    rtl_.top()->m_axi_bresp = resp;
}

void
Flexnngine2Rtl3Accel::axiMasterSetBValid(uint8_t val)
{
    rtl_.top()->m_axi_bvalid = val;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetBReady()
{
    return rtl_.top()->m_axi_bready;
}

axion::AxiId
Flexnngine2Rtl3Accel::axiMasterGetArId()
{
    return rtl_.top()->m_axi_arid;
}

Addr
Flexnngine2Rtl3Accel::axiMasterGetArAddr()
{
    return rtl_.top()->m_axi_araddr;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArLen()
{
    return rtl_.top()->m_axi_arlen;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArSize()
{
    return rtl_.top()->m_axi_arsize;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArBurst()
{
    return rtl_.top()->m_axi_arburst;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArLock()
{
    return rtl_.top()->m_axi_arlock;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArCache()
{
    return rtl_.top()->m_axi_arcache;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArProt()
{
    return rtl_.top()->m_axi_arprot;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArQos()
{
    return rtl_.top()->m_axi_arqos;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArRegion()
{
    return rtl_.top()->m_axi_arregion;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetArValid()
{
    return rtl_.top()->m_axi_arvalid;
}

void
Flexnngine2Rtl3Accel::axiMasterSetArReady(uint8_t val)
{
    rtl_.top()->m_axi_arready = val;
}

void
Flexnngine2Rtl3Accel::axiMasterSetRId(axion::AxiId id)
{
    rtl_.top()->m_axi_rid = id;
}

void
Flexnngine2Rtl3Accel::axiMasterSetRData(uint64_t data)
{
    rtl_.top()->m_axi_rdata = data;
}

void
Flexnngine2Rtl3Accel::axiMasterSetRResp(uint8_t resp)
{
    rtl_.top()->m_axi_rresp = resp;
}

void
Flexnngine2Rtl3Accel::axiMasterSetRLast(uint8_t val)
{
    rtl_.top()->m_axi_rlast = val;
}

void
Flexnngine2Rtl3Accel::axiMasterSetRValid(uint8_t val)
{
    rtl_.top()->m_axi_rvalid = val;
}

uint8_t
Flexnngine2Rtl3Accel::axiMasterGetRReady()
{
    return rtl_.top()->m_axi_rready;
}

} // namespace gem5
