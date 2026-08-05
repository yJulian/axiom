/*
 * Shared AXI4 types for the AXION RTL bridge (src/axi).
 *
 * Defines the pin-accessor contracts (Axi4SlavePins / Axi4MasterPins) that
 * a leaf SimObject implements against a specific Verilator-generated top
 * module, plus the small value types the bridge engines pass around. The
 * contracts mirror hw/axi4/axi4_pins.sv's flat s_axi_*/m_axi_* port lists
 * one-for-one -- one virtual method per pin.
 */

#ifndef __AXI_AXI4_TYPES_HH__
#define __AXI_AXI4_TYPES_HH__

#include <cstdint>

#include "base/types.hh"

namespace gem5
{
namespace axion
{

using AxiId = uint32_t;

enum class AxiBurst : uint8_t { Fixed = 0, Incr = 1, Wrap = 2 };
enum class AxiResp : uint8_t { Okay = 0, ExOkay = 1, SlvErr = 2, DecErr = 3 };

/**
 * Pin contract for an AXI4 SLAVE port: the pins a device exposes when
 * something else (gem5, or another RTL master) is the sole requester --
 * e.g. a control/status register port. Implemented by the leaf SimObject
 * wrapping a specific Verilated top module's s_axi_* pins (see
 * examples/fifo_pio_accel/fifo_pio_device.hh for a worked example).
 *
 * Because a slave port only ever has one requester in this design, there
 * is nothing to reorder here -- Axi4SlaveEngine drives one transaction at
 * a time and simply echoes back whatever ID it was given.
 */
class Axi4SlavePins
{
  public:
    virtual ~Axi4SlavePins() = default;

    virtual void axiSetClk(uint8_t val) = 0;
    virtual void axiSetRstN(uint8_t val) = 0;
    virtual void axiEval() = 0;

    // Write address (AW)
    virtual void axiSlaveSetAwId(AxiId id) = 0;
    virtual void axiSlaveSetAwAddr(Addr addr) = 0;
    virtual void axiSlaveSetAwLen(uint8_t len) = 0;
    virtual void axiSlaveSetAwSize(uint8_t size) = 0;
    virtual void axiSlaveSetAwBurst(uint8_t burst) = 0;
    virtual void axiSlaveSetAwValid(uint8_t val) = 0;
    virtual uint8_t axiSlaveGetAwReady() = 0;

    // Write data (W)
    virtual void axiSlaveSetWData(uint64_t data) = 0;
    virtual void axiSlaveSetWStrb(uint64_t strb) = 0;
    virtual void axiSlaveSetWLast(uint8_t val) = 0;
    virtual void axiSlaveSetWValid(uint8_t val) = 0;
    virtual uint8_t axiSlaveGetWReady() = 0;

    // Write response (B)
    virtual AxiId axiSlaveGetBId() = 0;
    virtual uint8_t axiSlaveGetBResp() = 0;
    virtual uint8_t axiSlaveGetBValid() = 0;
    virtual void axiSlaveSetBReady(uint8_t val) = 0;

    // Read address (AR)
    virtual void axiSlaveSetArId(AxiId id) = 0;
    virtual void axiSlaveSetArAddr(Addr addr) = 0;
    virtual void axiSlaveSetArLen(uint8_t len) = 0;
    virtual void axiSlaveSetArSize(uint8_t size) = 0;
    virtual void axiSlaveSetArBurst(uint8_t burst) = 0;
    virtual void axiSlaveSetArValid(uint8_t val) = 0;
    virtual uint8_t axiSlaveGetArReady() = 0;

    // Read data (R)
    virtual AxiId axiSlaveGetRId() = 0;
    virtual uint64_t axiSlaveGetRData() = 0;
    virtual uint8_t axiSlaveGetRResp() = 0;
    virtual uint8_t axiSlaveGetRLast() = 0;
    virtual uint8_t axiSlaveGetRValid() = 0;
    virtual void axiSlaveSetRReady(uint8_t val) = 0;
};

/**
 * Pin contract for an AXI4 MASTER port: the pins a device exposes when it
 * is itself the requester -- a DMA engine driving into gem5's memory
 * system (RTLDmaDevice), or a CPU core's inst/data port (RTLBaseCpu).
 *
 * Unlike the slave side, a master port can have many transactions
 * outstanding at once, tagged by ID; Axi4MasterEngine is what actually
 * implements the AXI4 ordering rule (same ID completes in issue order,
 * different IDs may complete in any order) on top of this pin contract.
 */
class Axi4MasterPins
{
  public:
    virtual ~Axi4MasterPins() = default;

    virtual void axiSetClk(uint8_t val) = 0;
    virtual void axiSetRstN(uint8_t val) = 0;
    virtual void axiEval() = 0;

    // Write address (AW) -- driven by the DUT, sampled here
    virtual AxiId axiMasterGetAwId() = 0;
    virtual Addr axiMasterGetAwAddr() = 0;
    virtual uint8_t axiMasterGetAwLen() = 0;
    virtual uint8_t axiMasterGetAwSize() = 0;
    virtual uint8_t axiMasterGetAwBurst() = 0;
    virtual uint8_t axiMasterGetAwValid() = 0;
    virtual void axiMasterSetAwReady(uint8_t val) = 0;

    // Write data (W)
    virtual uint64_t axiMasterGetWData() = 0;
    virtual uint64_t axiMasterGetWStrb() = 0;
    virtual uint8_t axiMasterGetWLast() = 0;
    virtual uint8_t axiMasterGetWValid() = 0;
    virtual void axiMasterSetWReady(uint8_t val) = 0;

    // Write response (B)
    virtual void axiMasterSetBId(AxiId id) = 0;
    virtual void axiMasterSetBResp(uint8_t resp) = 0;
    virtual void axiMasterSetBValid(uint8_t val) = 0;
    virtual uint8_t axiMasterGetBReady() = 0;

    // Read address (AR)
    virtual AxiId axiMasterGetArId() = 0;
    virtual Addr axiMasterGetArAddr() = 0;
    virtual uint8_t axiMasterGetArLen() = 0;
    virtual uint8_t axiMasterGetArSize() = 0;
    virtual uint8_t axiMasterGetArBurst() = 0;
    virtual uint8_t axiMasterGetArValid() = 0;
    virtual void axiMasterSetArReady(uint8_t val) = 0;

    // Read data (R)
    virtual void axiMasterSetRId(AxiId id) = 0;
    virtual void axiMasterSetRData(uint64_t data) = 0;
    virtual void axiMasterSetRResp(uint8_t resp) = 0;
    virtual void axiMasterSetRLast(uint8_t val) = 0;
    virtual void axiMasterSetRValid(uint8_t val) = 0;
    virtual uint8_t axiMasterGetRReady() = 0;
};

} // namespace axion
} // namespace gem5

#endif // __AXI_AXI4_TYPES_HH__
