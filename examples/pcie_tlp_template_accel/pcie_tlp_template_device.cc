#include "pcie_tlp_template_device.hh"

namespace gem5
{

PcieTlpTemplateAccel::PcieTlpTemplateAccel(const Params &p)
    : RTLPcieTlpDevice(p), rtl_(name())
{}

void
PcieTlpTemplateAccel::tlpSetClk(uint8_t val)
{
    rtl_.top()->clk_i = val;
}

void
PcieTlpTemplateAccel::tlpSetRstN(uint8_t val)
{
    rtl_.top()->rst_ni = val;
}

void
PcieTlpTemplateAccel::tlpEval()
{
    rtl_.settle();
}

// -- axion::PcieTlpCompleterPins --

void
PcieTlpTemplateAccel::cqSetTData(uint64_t data)
{
    rtl_.top()->cq_tdata = data;
}

void
PcieTlpTemplateAccel::cqSetTLast(uint8_t val)
{
    rtl_.top()->cq_tlast = val;
}

void
PcieTlpTemplateAccel::cqSetTValid(uint8_t val)
{
    rtl_.top()->cq_tvalid = val;
}

uint8_t
PcieTlpTemplateAccel::cqGetTReady()
{
    return rtl_.top()->cq_tready;
}

uint64_t
PcieTlpTemplateAccel::ccGetTData()
{
    return rtl_.top()->cc_tdata;
}

uint8_t
PcieTlpTemplateAccel::ccGetTLast()
{
    return rtl_.top()->cc_tlast;
}

uint8_t
PcieTlpTemplateAccel::ccGetTValid()
{
    return rtl_.top()->cc_tvalid;
}

void
PcieTlpTemplateAccel::ccSetTReady(uint8_t val)
{
    rtl_.top()->cc_tready = val;
}

// -- axion::PcieTlpRequesterPins --

uint64_t
PcieTlpTemplateAccel::rqGetTData()
{
    return rtl_.top()->rq_tdata;
}

uint8_t
PcieTlpTemplateAccel::rqGetTLast()
{
    return rtl_.top()->rq_tlast;
}

uint8_t
PcieTlpTemplateAccel::rqGetTValid()
{
    return rtl_.top()->rq_tvalid;
}

void
PcieTlpTemplateAccel::rqSetTReady(uint8_t val)
{
    rtl_.top()->rq_tready = val;
}

void
PcieTlpTemplateAccel::rcSetTData(uint64_t data)
{
    rtl_.top()->rc_tdata = data;
}

void
PcieTlpTemplateAccel::rcSetTLast(uint8_t val)
{
    rtl_.top()->rc_tlast = val;
}

void
PcieTlpTemplateAccel::rcSetTValid(uint8_t val)
{
    rtl_.top()->rc_tvalid = val;
}

uint8_t
PcieTlpTemplateAccel::rcGetTReady()
{
    return rtl_.top()->rc_tready;
}

// -- Sideband pins --

uint8_t
PcieTlpTemplateAccel::rtlGetIrq()
{
    return rtl_.top()->irq_o;
}

bool
PcieTlpTemplateAccel::isIdle()
{
    // busy_o is low whenever the DUT's DMA state machine is in D_IDLE, the
    // only autonomous work it has; everything else is driven by a TLP the
    // engines already track.
    return !rtl_.top()->busy_o;
}

} // namespace gem5
