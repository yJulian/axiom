// axi4_if.sv
//
// Full AXI4 interface: all 5 channels (AW/W/B/AR/R), ID-tagged on the
// address and response channels. This is the "vorgefertigte
// AXIVerbindung" DUTs connect to directly (via the `slave`/`master`
// modport) -- axi4_pins.sv adapts it to the flat scalar ports Verilator's
// generated C++ model exposes at the top level.
//
// ID_WIDTH is what makes the out-of-order semantics representable: AXI4
// only requires that transactions sharing an ID complete in issue order --
// transactions with different IDs may complete in any order. The bridge
// engines in src/axi (Axi4SlaveEngine / Axi4MasterEngine) implement that
// rule; this interface only carries the ID field, it does not enforce
// ordering itself (that would require modeling a specific interconnect,
// not a point-to-point channel).

`ifndef AXI4_IF_SV
`define AXI4_IF_SV

interface axi4_if #(
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned STRB_WIDTH = DATA_WIDTH / 8
);
  import axi4_pkg::*;

  // -- Write address channel (AW) --
  logic [ID_WIDTH-1:0]   awid;
  logic [ADDR_WIDTH-1:0] awaddr;
  axi_len_t              awlen;
  axi_size_t              awsize;
  axi_burst_t              awburst;
  logic                    awvalid;
  logic                    awready;

  // -- Write data channel (W) --
  logic [DATA_WIDTH-1:0] wdata;
  logic [STRB_WIDTH-1:0] wstrb;
  logic                    wlast;
  logic                    wvalid;
  logic                    wready;

  // -- Write response channel (B) --
  logic [ID_WIDTH-1:0]   bid;
  axi_resp_t                bresp;
  logic                    bvalid;
  logic                    bready;

  // -- Read address channel (AR) --
  logic [ID_WIDTH-1:0]   arid;
  logic [ADDR_WIDTH-1:0] araddr;
  axi_len_t                arlen;
  axi_size_t                arsize;
  axi_burst_t                arburst;
  logic                      arvalid;
  logic                      arready;

  // -- Read data channel (R) --
  logic [ID_WIDTH-1:0]   rid;
  logic [DATA_WIDTH-1:0] rdata;
  axi_resp_t                rresp;
  logic                    rlast;
  logic                    rvalid;
  logic                    rready;

  modport master (
    output awid, awaddr, awlen, awsize, awburst, awvalid,
    input  awready,
    output wdata, wstrb, wlast, wvalid,
    input  wready,
    input  bid, bresp, bvalid,
    output bready,
    output arid, araddr, arlen, arsize, arburst, arvalid,
    input  arready,
    input  rid, rdata, rresp, rlast, rvalid,
    output rready
  );

  modport slave (
    input  awid, awaddr, awlen, awsize, awburst, awvalid,
    output awready,
    input  wdata, wstrb, wlast, wvalid,
    output wready,
    output bid, bresp, bvalid,
    input  bready,
    input  arid, araddr, arlen, arsize, arburst, arvalid,
    output arready,
    output rid, rdata, rresp, rlast, rvalid,
    input  rready
  );

  // Read-only view of every signal, for tracing/protocol-checker modules.
  modport monitor (
    input awid, awaddr, awlen, awsize, awburst, awvalid, awready,
    input wdata, wstrb, wlast, wvalid, wready,
    input bid, bresp, bvalid, bready,
    input arid, araddr, arlen, arsize, arburst, arvalid, arready,
    input rid, rdata, rresp, rlast, rvalid, rready
  );

endinterface

`endif // AXI4_IF_SV
