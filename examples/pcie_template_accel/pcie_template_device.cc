#include "pcie_template_device.hh"

namespace gem5
{

PcieTemplateAccel::PcieTemplateAccel(const Params &p)
    : RTLPciDevice(p), rtl_(name())
{}

void
PcieTemplateAccel::axiSetClk(uint8_t val)
{
    rtl_.top()->clk_i = val;
}

void
PcieTemplateAccel::axiSetRstN(uint8_t val)
{
    rtl_.top()->rst_ni = val;
}

void
PcieTemplateAccel::axiEval()
{
    rtl_.settle();
}

// -- axion::Axi4SlavePins: the BAR0 window. Addresses arriving here
// are BAR-relative offsets; RTLPciDevice::pioStart() translates them
// with PciDevice::getBAR() before the engine drives them. --

void
PcieTemplateAccel::axiSlaveSetAwId(axion::AxiId id)
{
    rtl_.top()->s_axi_awid = id;
}

void
PcieTemplateAccel::axiSlaveSetAwAddr(Addr addr)
{
    rtl_.top()->s_axi_awaddr = addr;
}

void
PcieTemplateAccel::axiSlaveSetAwLen(uint8_t len)
{
    rtl_.top()->s_axi_awlen = len;
}

void
PcieTemplateAccel::axiSlaveSetAwSize(uint8_t size)
{
    rtl_.top()->s_axi_awsize = size;
}

void
PcieTemplateAccel::axiSlaveSetAwBurst(uint8_t burst)
{
    rtl_.top()->s_axi_awburst = burst;
}

void
PcieTemplateAccel::axiSlaveSetAwLock(uint8_t lock)
{
    rtl_.top()->s_axi_awlock = lock;
}

void
PcieTemplateAccel::axiSlaveSetAwCache(uint8_t cache)
{
    rtl_.top()->s_axi_awcache = cache;
}

void
PcieTemplateAccel::axiSlaveSetAwProt(uint8_t prot)
{
    rtl_.top()->s_axi_awprot = prot;
}

void
PcieTemplateAccel::axiSlaveSetAwQos(uint8_t qos)
{
    rtl_.top()->s_axi_awqos = qos;
}

void
PcieTemplateAccel::axiSlaveSetAwRegion(uint8_t region)
{
    rtl_.top()->s_axi_awregion = region;
}

void
PcieTemplateAccel::axiSlaveSetAwValid(uint8_t val)
{
    rtl_.top()->s_axi_awvalid = val;
}

uint8_t
PcieTemplateAccel::axiSlaveGetAwReady()
{
    return rtl_.top()->s_axi_awready;
}

void
PcieTemplateAccel::axiSlaveSetWData(uint64_t data)
{
    rtl_.top()->s_axi_wdata = data;
}

void
PcieTemplateAccel::axiSlaveSetWStrb(uint64_t strb)
{
    rtl_.top()->s_axi_wstrb = strb;
}

void
PcieTemplateAccel::axiSlaveSetWLast(uint8_t val)
{
    rtl_.top()->s_axi_wlast = val;
}

void
PcieTemplateAccel::axiSlaveSetWValid(uint8_t val)
{
    rtl_.top()->s_axi_wvalid = val;
}

uint8_t
PcieTemplateAccel::axiSlaveGetWReady()
{
    return rtl_.top()->s_axi_wready;
}

axion::AxiId
PcieTemplateAccel::axiSlaveGetBId()
{
    return rtl_.top()->s_axi_bid;
}

uint8_t
PcieTemplateAccel::axiSlaveGetBResp()
{
    return rtl_.top()->s_axi_bresp;
}

uint8_t
PcieTemplateAccel::axiSlaveGetBValid()
{
    return rtl_.top()->s_axi_bvalid;
}

void
PcieTemplateAccel::axiSlaveSetBReady(uint8_t val)
{
    rtl_.top()->s_axi_bready = val;
}

void
PcieTemplateAccel::axiSlaveSetArId(axion::AxiId id)
{
    rtl_.top()->s_axi_arid = id;
}

void
PcieTemplateAccel::axiSlaveSetArAddr(Addr addr)
{
    rtl_.top()->s_axi_araddr = addr;
}

void
PcieTemplateAccel::axiSlaveSetArLen(uint8_t len)
{
    rtl_.top()->s_axi_arlen = len;
}

void
PcieTemplateAccel::axiSlaveSetArSize(uint8_t size)
{
    rtl_.top()->s_axi_arsize = size;
}

void
PcieTemplateAccel::axiSlaveSetArBurst(uint8_t burst)
{
    rtl_.top()->s_axi_arburst = burst;
}

void
PcieTemplateAccel::axiSlaveSetArLock(uint8_t lock)
{
    rtl_.top()->s_axi_arlock = lock;
}

void
PcieTemplateAccel::axiSlaveSetArCache(uint8_t cache)
{
    rtl_.top()->s_axi_arcache = cache;
}

void
PcieTemplateAccel::axiSlaveSetArProt(uint8_t prot)
{
    rtl_.top()->s_axi_arprot = prot;
}

void
PcieTemplateAccel::axiSlaveSetArQos(uint8_t qos)
{
    rtl_.top()->s_axi_arqos = qos;
}

void
PcieTemplateAccel::axiSlaveSetArRegion(uint8_t region)
{
    rtl_.top()->s_axi_arregion = region;
}

void
PcieTemplateAccel::axiSlaveSetArValid(uint8_t val)
{
    rtl_.top()->s_axi_arvalid = val;
}

uint8_t
PcieTemplateAccel::axiSlaveGetArReady()
{
    return rtl_.top()->s_axi_arready;
}

axion::AxiId
PcieTemplateAccel::axiSlaveGetRId()
{
    return rtl_.top()->s_axi_rid;
}

uint64_t
PcieTemplateAccel::axiSlaveGetRData()
{
    return rtl_.top()->s_axi_rdata;
}

uint8_t
PcieTemplateAccel::axiSlaveGetRResp()
{
    return rtl_.top()->s_axi_rresp;
}

uint8_t
PcieTemplateAccel::axiSlaveGetRLast()
{
    return rtl_.top()->s_axi_rlast;
}

uint8_t
PcieTemplateAccel::axiSlaveGetRValid()
{
    return rtl_.top()->s_axi_rvalid;
}

void
PcieTemplateAccel::axiSlaveSetRReady(uint8_t val)
{
    rtl_.top()->s_axi_rready = val;
}

// -- axion::Axi4MasterPins: bus-master DMA port --

axion::AxiId
PcieTemplateAccel::axiMasterGetAwId()
{
    return rtl_.top()->m_axi_awid;
}

Addr
PcieTemplateAccel::axiMasterGetAwAddr()
{
    return rtl_.top()->m_axi_awaddr;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwLen()
{
    return rtl_.top()->m_axi_awlen;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwSize()
{
    return rtl_.top()->m_axi_awsize;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwBurst()
{
    return rtl_.top()->m_axi_awburst;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwLock()
{
    return rtl_.top()->m_axi_awlock;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwCache()
{
    return rtl_.top()->m_axi_awcache;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwProt()
{
    return rtl_.top()->m_axi_awprot;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwQos()
{
    return rtl_.top()->m_axi_awqos;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwRegion()
{
    return rtl_.top()->m_axi_awregion;
}

uint8_t
PcieTemplateAccel::axiMasterGetAwValid()
{
    return rtl_.top()->m_axi_awvalid;
}

void
PcieTemplateAccel::axiMasterSetAwReady(uint8_t val)
{
    rtl_.top()->m_axi_awready = val;
}

uint64_t
PcieTemplateAccel::axiMasterGetWData()
{
    return rtl_.top()->m_axi_wdata;
}

uint64_t
PcieTemplateAccel::axiMasterGetWStrb()
{
    return rtl_.top()->m_axi_wstrb;
}

uint8_t
PcieTemplateAccel::axiMasterGetWLast()
{
    return rtl_.top()->m_axi_wlast;
}

uint8_t
PcieTemplateAccel::axiMasterGetWValid()
{
    return rtl_.top()->m_axi_wvalid;
}

void
PcieTemplateAccel::axiMasterSetWReady(uint8_t val)
{
    rtl_.top()->m_axi_wready = val;
}

void
PcieTemplateAccel::axiMasterSetBId(axion::AxiId id)
{
    rtl_.top()->m_axi_bid = id;
}

void
PcieTemplateAccel::axiMasterSetBResp(uint8_t resp)
{
    rtl_.top()->m_axi_bresp = resp;
}

void
PcieTemplateAccel::axiMasterSetBValid(uint8_t val)
{
    rtl_.top()->m_axi_bvalid = val;
}

uint8_t
PcieTemplateAccel::axiMasterGetBReady()
{
    return rtl_.top()->m_axi_bready;
}

axion::AxiId
PcieTemplateAccel::axiMasterGetArId()
{
    return rtl_.top()->m_axi_arid;
}

Addr
PcieTemplateAccel::axiMasterGetArAddr()
{
    return rtl_.top()->m_axi_araddr;
}

uint8_t
PcieTemplateAccel::axiMasterGetArLen()
{
    return rtl_.top()->m_axi_arlen;
}

uint8_t
PcieTemplateAccel::axiMasterGetArSize()
{
    return rtl_.top()->m_axi_arsize;
}

uint8_t
PcieTemplateAccel::axiMasterGetArBurst()
{
    return rtl_.top()->m_axi_arburst;
}

uint8_t
PcieTemplateAccel::axiMasterGetArLock()
{
    return rtl_.top()->m_axi_arlock;
}

uint8_t
PcieTemplateAccel::axiMasterGetArCache()
{
    return rtl_.top()->m_axi_arcache;
}

uint8_t
PcieTemplateAccel::axiMasterGetArProt()
{
    return rtl_.top()->m_axi_arprot;
}

uint8_t
PcieTemplateAccel::axiMasterGetArQos()
{
    return rtl_.top()->m_axi_arqos;
}

uint8_t
PcieTemplateAccel::axiMasterGetArRegion()
{
    return rtl_.top()->m_axi_arregion;
}

uint8_t
PcieTemplateAccel::axiMasterGetArValid()
{
    return rtl_.top()->m_axi_arvalid;
}

void
PcieTemplateAccel::axiMasterSetArReady(uint8_t val)
{
    rtl_.top()->m_axi_arready = val;
}

void
PcieTemplateAccel::axiMasterSetRId(axion::AxiId id)
{
    rtl_.top()->m_axi_rid = id;
}

void
PcieTemplateAccel::axiMasterSetRData(uint64_t data)
{
    rtl_.top()->m_axi_rdata = data;
}

void
PcieTemplateAccel::axiMasterSetRResp(uint8_t resp)
{
    rtl_.top()->m_axi_rresp = resp;
}

void
PcieTemplateAccel::axiMasterSetRLast(uint8_t val)
{
    rtl_.top()->m_axi_rlast = val;
}

void
PcieTemplateAccel::axiMasterSetRValid(uint8_t val)
{
    rtl_.top()->m_axi_rvalid = val;
}

uint8_t
PcieTemplateAccel::axiMasterGetRReady()
{
    return rtl_.top()->m_axi_rready;
}

// -- Sideband pins --

uint8_t
PcieTemplateAccel::rtlGetIrq()
{
    return rtl_.top()->irq_o;
}

bool
PcieTemplateAccel::isIdle()
{
    // busy_o is low whenever the DUT's DMA state machine is in D_IDLE,
    // which is the only autonomous work it has -- everything else is
    // driven by an AXI4 transaction the engines already track. That makes
    // it safe for RTLPciDevice's clock gating to stop ticking here.
    return !rtl_.top()->busy_o;
}

} // namespace gem5
