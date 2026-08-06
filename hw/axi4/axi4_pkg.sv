// axi4_pkg.sv
//
// Shared AXI4 typedefs and default field widths used by axi4_if.sv and
// axi4_pins.sv. Individual axi4_if instances may override
// ID_WIDTH/ADDR_WIDTH/DATA_WIDTH per instantiation; the constants below are
// only the defaults AXION falls back to when nothing else is specified.

`ifndef AXI4_PKG_SV
`define AXI4_PKG_SV

package axi4_pkg;

  localparam int unsigned AXI_ID_WIDTH_DEFAULT   = 4;
  localparam int unsigned AXI_ADDR_WIDTH_DEFAULT = 64;
  localparam int unsigned AXI_DATA_WIDTH_DEFAULT = 64;

  // AXI4 burst types (AxBURST)
  typedef enum logic [1:0] {
    AXI_BURST_FIXED = 2'b00,
    AXI_BURST_INCR  = 2'b01,
    AXI_BURST_WRAP  = 2'b10
  } axi_burst_t;

  // AXI4 response codes (BRESP / RRESP)
  typedef enum logic [1:0] {
    AXI_RESP_OKAY   = 2'b00,
    AXI_RESP_EXOKAY = 2'b01,
    AXI_RESP_SLVERR = 2'b10,
    AXI_RESP_DECERR = 2'b11
  } axi_resp_t;

  // AxSIZE: bytes-per-beat is 2**size (size in [0,7] -> 1..128 bytes/beat)
  typedef logic [2:0] axi_size_t;

  // AxLEN: burst_length - 1 (AXI4 allows up to 256 beats on INCR)
  typedef logic [7:0] axi_len_t;

  // AxLOCK: 1 bit in AXI4 (exclusive access; AXI3's 2-bit locked/exclusive
  // encoding was collapsed going into AXI4)
  typedef logic axi_lock_t;

  // AxCACHE: memory-type/bufferable/cacheable/allocate attributes
  typedef logic [3:0] axi_cache_t;

  // AxPROT: privileged / secure / instruction-vs-data access attributes
  typedef logic [2:0] axi_prot_t;

  // AxQOS: per-transaction quality-of-service priority
  typedef logic [3:0] axi_qos_t;

  // AxREGION: multi-region slave decode
  typedef logic [3:0] axi_region_t;

  function automatic int unsigned beat_bytes(axi_size_t size);
    return 32'(1) << size;
  endfunction

endpackage

`endif // AXI4_PKG_SV
