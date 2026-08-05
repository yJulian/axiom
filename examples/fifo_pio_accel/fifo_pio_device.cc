#include "fifo_pio_device.hh"

namespace gem5
{

FifoPioAccel::FifoPioAccel(const Params &p)
    : RTLPioDevice(p), rtl_(name())
{}

void
FifoPioAccel::axiSetClk(uint8_t val)
{
    rtl_.top()->clk_i = val;
}

void
FifoPioAccel::axiSetRstN(uint8_t val)
{
    rtl_.top()->rst_ni = val;
}

void
FifoPioAccel::axiEval()
{
    rtl_.settle();
}

void
FifoPioAccel::axiSlaveSetAwId(axion::AxiId id)
{
    rtl_.top()->s_axi_awid = id;
}

void
FifoPioAccel::axiSlaveSetAwAddr(Addr addr)
{
    rtl_.top()->s_axi_awaddr = addr;
}

void
FifoPioAccel::axiSlaveSetAwLen(uint8_t len)
{
    rtl_.top()->s_axi_awlen = len;
}

void
FifoPioAccel::axiSlaveSetAwSize(uint8_t size)
{
    rtl_.top()->s_axi_awsize = size;
}

void
FifoPioAccel::axiSlaveSetAwBurst(uint8_t burst)
{
    rtl_.top()->s_axi_awburst = burst;
}

void
FifoPioAccel::axiSlaveSetAwValid(uint8_t val)
{
    rtl_.top()->s_axi_awvalid = val;
}

uint8_t
FifoPioAccel::axiSlaveGetAwReady()
{
    return rtl_.top()->s_axi_awready;
}

void
FifoPioAccel::axiSlaveSetWData(uint64_t data)
{
    rtl_.top()->s_axi_wdata = data;
}

void
FifoPioAccel::axiSlaveSetWStrb(uint64_t strb)
{
    rtl_.top()->s_axi_wstrb = strb;
}

void
FifoPioAccel::axiSlaveSetWLast(uint8_t val)
{
    rtl_.top()->s_axi_wlast = val;
}

void
FifoPioAccel::axiSlaveSetWValid(uint8_t val)
{
    rtl_.top()->s_axi_wvalid = val;
}

uint8_t
FifoPioAccel::axiSlaveGetWReady()
{
    return rtl_.top()->s_axi_wready;
}

axion::AxiId
FifoPioAccel::axiSlaveGetBId()
{
    return rtl_.top()->s_axi_bid;
}

uint8_t
FifoPioAccel::axiSlaveGetBResp()
{
    return rtl_.top()->s_axi_bresp;
}

uint8_t
FifoPioAccel::axiSlaveGetBValid()
{
    return rtl_.top()->s_axi_bvalid;
}

void
FifoPioAccel::axiSlaveSetBReady(uint8_t val)
{
    rtl_.top()->s_axi_bready = val;
}

void
FifoPioAccel::axiSlaveSetArId(axion::AxiId id)
{
    rtl_.top()->s_axi_arid = id;
}

void
FifoPioAccel::axiSlaveSetArAddr(Addr addr)
{
    rtl_.top()->s_axi_araddr = addr;
}

void
FifoPioAccel::axiSlaveSetArLen(uint8_t len)
{
    rtl_.top()->s_axi_arlen = len;
}

void
FifoPioAccel::axiSlaveSetArSize(uint8_t size)
{
    rtl_.top()->s_axi_arsize = size;
}

void
FifoPioAccel::axiSlaveSetArBurst(uint8_t burst)
{
    rtl_.top()->s_axi_arburst = burst;
}

void
FifoPioAccel::axiSlaveSetArValid(uint8_t val)
{
    rtl_.top()->s_axi_arvalid = val;
}

uint8_t
FifoPioAccel::axiSlaveGetArReady()
{
    return rtl_.top()->s_axi_arready;
}

axion::AxiId
FifoPioAccel::axiSlaveGetRId()
{
    return rtl_.top()->s_axi_rid;
}

uint64_t
FifoPioAccel::axiSlaveGetRData()
{
    return rtl_.top()->s_axi_rdata;
}

uint8_t
FifoPioAccel::axiSlaveGetRResp()
{
    return rtl_.top()->s_axi_rresp;
}

uint8_t
FifoPioAccel::axiSlaveGetRLast()
{
    return rtl_.top()->s_axi_rlast;
}

uint8_t
FifoPioAccel::axiSlaveGetRValid()
{
    return rtl_.top()->s_axi_rvalid;
}

void
FifoPioAccel::axiSlaveSetRReady(uint8_t val)
{
    rtl_.top()->s_axi_rready = val;
}

} // namespace gem5
