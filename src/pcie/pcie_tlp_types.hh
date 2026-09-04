/*
 * Pin-accessor contracts and header helpers for AXION's TLP-level PCIe
 * path -- the counterpart to src/axi/axi4_types.hh, and the same idea: one
 * virtual method per RTL pin, implemented by a leaf SimObject against its
 * Verilator-generated top module.
 *
 * There is far less of it than the AXI4 contract (16 pins versus 88)
 * because a PCIe endpoint's four streams are plain AXI4-Stream and carry
 * all their addressing, length and byte-enable information *inside* the
 * packet rather than on sideband wires. That is the whole trade this path
 * makes: fewer wires, more parsing.
 *
 * See hw/pcie/pcie_tlp_pkg.sv for the header layout these helpers encode
 * and decode, and hw/pcie/pcie_tlp_if.sv for the streams themselves
 * (including why they carry no TKEEP).
 */

#ifndef __PCIE_PCIE_TLP_TYPES_HH__
#define __PCIE_PCIE_TLP_TYPES_HH__

#include <cstdint>

#include "base/types.hh"

namespace gem5
{
namespace axion
{

/** TLP format field, header DW0 [31:29]. */
enum class TlpFmt : uint8_t
{
    Hdr3NoData = 0b000,
    Hdr4NoData = 0b001,
    Hdr3Data = 0b010,
    Hdr4Data = 0b011,
};

/** TLP type field, header DW0 [28:24]. */
enum class TlpType : uint8_t
{
    Mem = 0b00000,  // MRd (no data) / MWr (data)
    Cpl = 0b01010,  // Cpl (no data) / CplD (data)
};

/** Completion status, completion header DW1 [15:13]. */
enum class TlpCplStatus : uint8_t
{
    Success = 0b000,
    UnsupportedRequest = 0b001,
    CompleterAbort = 0b100,
};

/**
 * Header field encode/decode. Free functions rather than a struct so both
 * engines and their tests can use them without dragging state along; the
 * bit positions are the PCIe base spec's, not a local convention.
 */
namespace tlp
{

inline uint32_t
makeDw0(TlpFmt fmt, TlpType type, uint16_t lengthDw)
{
    return (static_cast<uint32_t>(fmt) << 29) |
           (static_cast<uint32_t>(type) << 24) | (lengthDw & 0x3ff);
}

inline TlpFmt
dw0Fmt(uint32_t dw0)
{
    return static_cast<TlpFmt>((dw0 >> 29) & 0x7);
}

inline TlpType
dw0Type(uint32_t dw0)
{
    return static_cast<TlpType>((dw0 >> 24) & 0x1f);
}

/** Payload length in DWs. The wire encodes 1024 DW as 0. */
inline uint16_t
dw0LengthDw(uint32_t dw0)
{
    return dw0 & 0x3ff;
}

inline bool
fmtHasData(TlpFmt fmt)
{
    return (static_cast<uint8_t>(fmt) & 0b010) != 0;
}

inline bool
fmtIs4Dw(TlpFmt fmt)
{
    return (static_cast<uint8_t>(fmt) & 0b001) != 0;
}

inline uint32_t
makeReqDw1(uint16_t requesterId, uint8_t tag, uint8_t lastBe, uint8_t firstBe)
{
    return (static_cast<uint32_t>(requesterId) << 16) |
           (static_cast<uint32_t>(tag) << 8) |
           ((lastBe & 0xf) << 4) | (firstBe & 0xf);
}

inline uint16_t
reqDw1RequesterId(uint32_t dw1)
{
    return dw1 >> 16;
}

inline uint8_t
reqDw1Tag(uint32_t dw1)
{
    return (dw1 >> 8) & 0xff;
}

inline uint8_t
reqDw1LastBe(uint32_t dw1)
{
    return (dw1 >> 4) & 0xf;
}

inline uint8_t
reqDw1FirstBe(uint32_t dw1)
{
    return dw1 & 0xf;
}

inline uint32_t
makeCplDw1(uint16_t completerId, TlpCplStatus status, uint16_t byteCount)
{
    return (static_cast<uint32_t>(completerId) << 16) |
           (static_cast<uint32_t>(status) << 13) | (byteCount & 0xfff);
}

inline TlpCplStatus
cplDw1Status(uint32_t dw1)
{
    return static_cast<TlpCplStatus>((dw1 >> 13) & 0x7);
}

inline uint16_t
cplDw1ByteCount(uint32_t dw1)
{
    return dw1 & 0xfff;
}

inline uint32_t
makeCplDw2(uint16_t requesterId, uint8_t tag, uint8_t lowerAddress)
{
    return (static_cast<uint32_t>(requesterId) << 16) |
           (static_cast<uint32_t>(tag) << 8) | (lowerAddress & 0x7f);
}

inline uint8_t
cplDw2Tag(uint32_t dw2)
{
    return (dw2 >> 8) & 0xff;
}

/** Pack two DWs into one 64-bit stream beat: DW N low, DW N+1 high. */
inline uint64_t
beat(uint32_t lowDw, uint32_t highDw)
{
    return (static_cast<uint64_t>(highDw) << 32) | lowDw;
}

inline uint32_t
beatLowDw(uint64_t b)
{
    return static_cast<uint32_t>(b);
}

inline uint32_t
beatHighDw(uint64_t b)
{
    return static_cast<uint32_t>(b >> 32);
}

} // namespace tlp

/**
 * Clock/reset/eval, inherited virtually by both pin roles so a leaf
 * implementing an endpoint has exactly one of each -- same arrangement as
 * Axi4ClockPins.
 */
class PcieTlpClockPins
{
  public:
    virtual ~PcieTlpClockPins() = default;

    virtual void tlpSetClk(uint8_t val) = 0;
    virtual void tlpSetRstN(uint8_t val) = 0;
    virtual void tlpEval() = 0;
};

/**
 * The completer streams: requests we push at the DUT's BAR window (CQ) and
 * the completions it returns (CC). Directions in the method names are the
 * DUT's, matching the pin names in hw/pcie/pcie_tlp_pins.sv -- so we
 * *drive* CQ and *sample* CC.
 */
class PcieTlpCompleterPins : public virtual PcieTlpClockPins
{
  public:
    virtual ~PcieTlpCompleterPins() = default;

    // CQ -- completer request, host -> DUT
    virtual void cqSetTData(uint64_t data) = 0;
    virtual void cqSetTLast(uint8_t val) = 0;
    virtual void cqSetTValid(uint8_t val) = 0;
    virtual uint8_t cqGetTReady() = 0;

    // CC -- completer completion, DUT -> host
    virtual uint64_t ccGetTData() = 0;
    virtual uint8_t ccGetTLast() = 0;
    virtual uint8_t ccGetTValid() = 0;
    virtual void ccSetTReady(uint8_t val) = 0;
};

/**
 * The requester streams: the DUT's own DMA requests (RQ) and the
 * completions we return for its reads (RC).
 */
class PcieTlpRequesterPins : public virtual PcieTlpClockPins
{
  public:
    virtual ~PcieTlpRequesterPins() = default;

    // RQ -- requester request, DUT -> host
    virtual uint64_t rqGetTData() = 0;
    virtual uint8_t rqGetTLast() = 0;
    virtual uint8_t rqGetTValid() = 0;
    virtual void rqSetTReady(uint8_t val) = 0;

    // RC -- requester completion, host -> DUT
    virtual void rcSetTData(uint64_t data) = 0;
    virtual void rcSetTLast(uint8_t val) = 0;
    virtual void rcSetTValid(uint8_t val) = 0;
    virtual uint8_t rcGetTReady() = 0;
};

} // namespace axion
} // namespace gem5

#endif // __PCIE_PCIE_TLP_TYPES_HH__
