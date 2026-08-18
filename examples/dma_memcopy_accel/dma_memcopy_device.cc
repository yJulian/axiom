#include "dma_memcopy_device.hh"

namespace gem5
{

DmaMemcopyAccel::DmaMemcopyAccel(const Params &p)
    : RTLDmaDevice(p), rtl_(name())
{}

void
DmaMemcopyAccel::axiSetClk(uint8_t val)
{
    rtl_.top()->clk_i = val;
}

void
DmaMemcopyAccel::axiSetRstN(uint8_t val)
{
    rtl_.top()->rst_ni = val;
}

void
DmaMemcopyAccel::axiEval()
{
    rtl_.settle();
}

// -- axion::Axi4SlavePins: control/status register port --

void
DmaMemcopyAccel::axiSlaveSetAwId(axion::AxiId id)
{
    rtl_.top()->s_axi_awid = id;
}

void
DmaMemcopyAccel::axiSlaveSetAwAddr(Addr addr)
{
    rtl_.top()->s_axi_awaddr = addr;
}

void
DmaMemcopyAccel::axiSlaveSetAwLen(uint8_t len)
{
    rtl_.top()->s_axi_awlen = len;
}

void
DmaMemcopyAccel::axiSlaveSetAwSize(uint8_t size)
{
    rtl_.top()->s_axi_awsize = size;
}

void
DmaMemcopyAccel::axiSlaveSetAwBurst(uint8_t burst)
{
    rtl_.top()->s_axi_awburst = burst;
}

void
DmaMemcopyAccel::axiSlaveSetAwLock(uint8_t lock)
{
    rtl_.top()->s_axi_awlock = lock;
}

void
DmaMemcopyAccel::axiSlaveSetAwCache(uint8_t cache)
{
    rtl_.top()->s_axi_awcache = cache;
}

void
DmaMemcopyAccel::axiSlaveSetAwProt(uint8_t prot)
{
    rtl_.top()->s_axi_awprot = prot;
}

void
DmaMemcopyAccel::axiSlaveSetAwQos(uint8_t qos)
{
    rtl_.top()->s_axi_awqos = qos;
}

void
DmaMemcopyAccel::axiSlaveSetAwRegion(uint8_t region)
{
    rtl_.top()->s_axi_awregion = region;
}

void
DmaMemcopyAccel::axiSlaveSetAwValid(uint8_t val)
{
    rtl_.top()->s_axi_awvalid = val;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetAwReady()
{
    return rtl_.top()->s_axi_awready;
}

void
DmaMemcopyAccel::axiSlaveSetWData(uint64_t data)
{
    rtl_.top()->s_axi_wdata = data;
}

void
DmaMemcopyAccel::axiSlaveSetWStrb(uint64_t strb)
{
    rtl_.top()->s_axi_wstrb = strb;
}

void
DmaMemcopyAccel::axiSlaveSetWLast(uint8_t val)
{
    rtl_.top()->s_axi_wlast = val;
}

void
DmaMemcopyAccel::axiSlaveSetWValid(uint8_t val)
{
    rtl_.top()->s_axi_wvalid = val;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetWReady()
{
    return rtl_.top()->s_axi_wready;
}

axion::AxiId
DmaMemcopyAccel::axiSlaveGetBId()
{
    return rtl_.top()->s_axi_bid;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetBResp()
{
    return rtl_.top()->s_axi_bresp;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetBValid()
{
    return rtl_.top()->s_axi_bvalid;
}

void
DmaMemcopyAccel::axiSlaveSetBReady(uint8_t val)
{
    rtl_.top()->s_axi_bready = val;
}

void
DmaMemcopyAccel::axiSlaveSetArId(axion::AxiId id)
{
    rtl_.top()->s_axi_arid = id;
}

void
DmaMemcopyAccel::axiSlaveSetArAddr(Addr addr)
{
    rtl_.top()->s_axi_araddr = addr;
}

void
DmaMemcopyAccel::axiSlaveSetArLen(uint8_t len)
{
    rtl_.top()->s_axi_arlen = len;
}

void
DmaMemcopyAccel::axiSlaveSetArSize(uint8_t size)
{
    rtl_.top()->s_axi_arsize = size;
}

void
DmaMemcopyAccel::axiSlaveSetArBurst(uint8_t burst)
{
    rtl_.top()->s_axi_arburst = burst;
}

void
DmaMemcopyAccel::axiSlaveSetArLock(uint8_t lock)
{
    rtl_.top()->s_axi_arlock = lock;
}

void
DmaMemcopyAccel::axiSlaveSetArCache(uint8_t cache)
{
    rtl_.top()->s_axi_arcache = cache;
}

void
DmaMemcopyAccel::axiSlaveSetArProt(uint8_t prot)
{
    rtl_.top()->s_axi_arprot = prot;
}

void
DmaMemcopyAccel::axiSlaveSetArQos(uint8_t qos)
{
    rtl_.top()->s_axi_arqos = qos;
}

void
DmaMemcopyAccel::axiSlaveSetArRegion(uint8_t region)
{
    rtl_.top()->s_axi_arregion = region;
}

void
DmaMemcopyAccel::axiSlaveSetArValid(uint8_t val)
{
    rtl_.top()->s_axi_arvalid = val;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetArReady()
{
    return rtl_.top()->s_axi_arready;
}

axion::AxiId
DmaMemcopyAccel::axiSlaveGetRId()
{
    return rtl_.top()->s_axi_rid;
}

uint64_t
DmaMemcopyAccel::axiSlaveGetRData()
{
    return rtl_.top()->s_axi_rdata;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetRResp()
{
    return rtl_.top()->s_axi_rresp;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetRLast()
{
    return rtl_.top()->s_axi_rlast;
}

uint8_t
DmaMemcopyAccel::axiSlaveGetRValid()
{
    return rtl_.top()->s_axi_rvalid;
}

void
DmaMemcopyAccel::axiSlaveSetRReady(uint8_t val)
{
    rtl_.top()->s_axi_rready = val;
}

// -- axion::Axi4MasterPins: DMA port --

axion::AxiId
DmaMemcopyAccel::axiMasterGetAwId()
{
    return rtl_.top()->m_axi_awid;
}

Addr
DmaMemcopyAccel::axiMasterGetAwAddr()
{
    return rtl_.top()->m_axi_awaddr;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwLen()
{
    return rtl_.top()->m_axi_awlen;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwSize()
{
    return rtl_.top()->m_axi_awsize;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwBurst()
{
    return rtl_.top()->m_axi_awburst;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwLock()
{
    return rtl_.top()->m_axi_awlock;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwCache()
{
    return rtl_.top()->m_axi_awcache;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwProt()
{
    return rtl_.top()->m_axi_awprot;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwQos()
{
    return rtl_.top()->m_axi_awqos;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwRegion()
{
    return rtl_.top()->m_axi_awregion;
}

uint8_t
DmaMemcopyAccel::axiMasterGetAwValid()
{
    return rtl_.top()->m_axi_awvalid;
}

void
DmaMemcopyAccel::axiMasterSetAwReady(uint8_t val)
{
    rtl_.top()->m_axi_awready = val;
}

uint64_t
DmaMemcopyAccel::axiMasterGetWData()
{
    return rtl_.top()->m_axi_wdata;
}

uint64_t
DmaMemcopyAccel::axiMasterGetWStrb()
{
    return rtl_.top()->m_axi_wstrb;
}

uint8_t
DmaMemcopyAccel::axiMasterGetWLast()
{
    return rtl_.top()->m_axi_wlast;
}

uint8_t
DmaMemcopyAccel::axiMasterGetWValid()
{
    return rtl_.top()->m_axi_wvalid;
}

void
DmaMemcopyAccel::axiMasterSetWReady(uint8_t val)
{
    rtl_.top()->m_axi_wready = val;
}

void
DmaMemcopyAccel::axiMasterSetBId(axion::AxiId id)
{
    rtl_.top()->m_axi_bid = id;
}

void
DmaMemcopyAccel::axiMasterSetBResp(uint8_t resp)
{
    rtl_.top()->m_axi_bresp = resp;
}

void
DmaMemcopyAccel::axiMasterSetBValid(uint8_t val)
{
    rtl_.top()->m_axi_bvalid = val;
}

uint8_t
DmaMemcopyAccel::axiMasterGetBReady()
{
    return rtl_.top()->m_axi_bready;
}

axion::AxiId
DmaMemcopyAccel::axiMasterGetArId()
{
    return rtl_.top()->m_axi_arid;
}

Addr
DmaMemcopyAccel::axiMasterGetArAddr()
{
    return rtl_.top()->m_axi_araddr;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArLen()
{
    return rtl_.top()->m_axi_arlen;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArSize()
{
    return rtl_.top()->m_axi_arsize;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArBurst()
{
    return rtl_.top()->m_axi_arburst;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArLock()
{
    return rtl_.top()->m_axi_arlock;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArCache()
{
    return rtl_.top()->m_axi_arcache;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArProt()
{
    return rtl_.top()->m_axi_arprot;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArQos()
{
    return rtl_.top()->m_axi_arqos;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArRegion()
{
    return rtl_.top()->m_axi_arregion;
}

uint8_t
DmaMemcopyAccel::axiMasterGetArValid()
{
    return rtl_.top()->m_axi_arvalid;
}

void
DmaMemcopyAccel::axiMasterSetArReady(uint8_t val)
{
    rtl_.top()->m_axi_arready = val;
}

void
DmaMemcopyAccel::axiMasterSetRId(axion::AxiId id)
{
    rtl_.top()->m_axi_rid = id;
}

void
DmaMemcopyAccel::axiMasterSetRData(uint64_t data)
{
    rtl_.top()->m_axi_rdata = data;
}

void
DmaMemcopyAccel::axiMasterSetRResp(uint8_t resp)
{
    rtl_.top()->m_axi_rresp = resp;
}

void
DmaMemcopyAccel::axiMasterSetRLast(uint8_t val)
{
    rtl_.top()->m_axi_rlast = val;
}

void
DmaMemcopyAccel::axiMasterSetRValid(uint8_t val)
{
    rtl_.top()->m_axi_rvalid = val;
}

uint8_t
DmaMemcopyAccel::axiMasterGetRReady()
{
    return rtl_.top()->m_axi_rready;
}

} // namespace gem5
