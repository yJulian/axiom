// axi4_pins.sv
//
// Pre-built AXI4 pin adapters ("vorgefertigte AXIVerbindungen"). Each wraps
// a flat, scalar port list -- the only thing Verilator's generated C++
// model exposes as individually gettable/settable signals -- around a
// clean axi4_if handle that the DUT connects to directly. A TOP module
// (see examples/*/*_top.sv) instantiates one of these plus the DUT and
// wires the pins module's `aif` port straight into the DUT's own
// axi4_if port.
//
// Two flavors, named after the DUT's own role (matching the s_axi_*/
// m_axi_* naming convention already used for accelerator ports elsewhere):
//
//   axi4_pins_slave_port  -- the DUT is the AXI4 SLAVE (e.g. a PIO/register
//                             port, driven by gem5 as the sole requester).
//                             Flat s_axi_* pins carry the request in,
//                             response out.
//   axi4_pins_master_port -- the DUT is the AXI4 MASTER (e.g. a DMA
//                             requester issuing into gem5's memory
//                             system). Flat m_axi_* pins carry the
//                             request out, response in.

`ifndef AXI4_PINS_SV
`define AXI4_PINS_SV

module axi4_pins_slave_port #(
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned STRB_WIDTH = DATA_WIDTH / 8
) (
  // -- Write address channel (driven by the external requester) --
  input  logic [ID_WIDTH-1:0]   s_axi_awid,
  input  logic [ADDR_WIDTH-1:0] s_axi_awaddr,
  input  logic [7:0]            s_axi_awlen,
  input  logic [2:0]            s_axi_awsize,
  input  logic [1:0]            s_axi_awburst,
  input  logic                  s_axi_awlock,
  input  logic [3:0]            s_axi_awcache,
  input  logic [2:0]            s_axi_awprot,
  input  logic [3:0]            s_axi_awqos,
  input  logic [3:0]            s_axi_awregion,
  input  logic                  s_axi_awvalid,
  output logic                  s_axi_awready,

  // -- Write data channel --
  input  logic [DATA_WIDTH-1:0] s_axi_wdata,
  input  logic [STRB_WIDTH-1:0] s_axi_wstrb,
  input  logic                  s_axi_wlast,
  input  logic                  s_axi_wvalid,
  output logic                  s_axi_wready,

  // -- Write response channel --
  output logic [ID_WIDTH-1:0]   s_axi_bid,
  output logic [1:0]            s_axi_bresp,
  output logic                  s_axi_bvalid,
  input  logic                  s_axi_bready,

  // -- Read address channel --
  input  logic [ID_WIDTH-1:0]   s_axi_arid,
  input  logic [ADDR_WIDTH-1:0] s_axi_araddr,
  input  logic [7:0]            s_axi_arlen,
  input  logic [2:0]            s_axi_arsize,
  input  logic [1:0]            s_axi_arburst,
  input  logic                  s_axi_arlock,
  input  logic [3:0]            s_axi_arcache,
  input  logic [2:0]            s_axi_arprot,
  input  logic [3:0]            s_axi_arqos,
  input  logic [3:0]            s_axi_arregion,
  input  logic                  s_axi_arvalid,
  output logic                  s_axi_arready,

  // -- Read data channel --
  output logic [ID_WIDTH-1:0]   s_axi_rid,
  output logic [DATA_WIDTH-1:0] s_axi_rdata,
  output logic [1:0]            s_axi_rresp,
  output logic                  s_axi_rlast,
  output logic                  s_axi_rvalid,
  input  logic                  s_axi_rready,

  axi4_if.master aif
);

  assign aif.awid      = s_axi_awid;
  assign aif.awaddr    = s_axi_awaddr;
  assign aif.awlen     = axi4_pkg::axi_len_t'(s_axi_awlen);
  assign aif.awsize    = axi4_pkg::axi_size_t'(s_axi_awsize);
  assign aif.awburst   = axi4_pkg::axi_burst_t'(s_axi_awburst);
  assign aif.awlock    = axi4_pkg::axi_lock_t'(s_axi_awlock);
  assign aif.awcache   = axi4_pkg::axi_cache_t'(s_axi_awcache);
  assign aif.awprot    = axi4_pkg::axi_prot_t'(s_axi_awprot);
  assign aif.awqos     = axi4_pkg::axi_qos_t'(s_axi_awqos);
  assign aif.awregion  = axi4_pkg::axi_region_t'(s_axi_awregion);
  assign aif.awvalid   = s_axi_awvalid;
  assign s_axi_awready = aif.awready;

  assign aif.wdata    = s_axi_wdata;
  assign aif.wstrb    = s_axi_wstrb;
  assign aif.wlast    = s_axi_wlast;
  assign aif.wvalid   = s_axi_wvalid;
  assign s_axi_wready = aif.wready;

  assign s_axi_bid    = aif.bid;
  assign s_axi_bresp  = aif.bresp;
  assign s_axi_bvalid = aif.bvalid;
  assign aif.bready   = s_axi_bready;

  assign aif.arid      = s_axi_arid;
  assign aif.araddr    = s_axi_araddr;
  assign aif.arlen     = axi4_pkg::axi_len_t'(s_axi_arlen);
  assign aif.arsize    = axi4_pkg::axi_size_t'(s_axi_arsize);
  assign aif.arburst   = axi4_pkg::axi_burst_t'(s_axi_arburst);
  assign aif.arlock    = axi4_pkg::axi_lock_t'(s_axi_arlock);
  assign aif.arcache   = axi4_pkg::axi_cache_t'(s_axi_arcache);
  assign aif.arprot    = axi4_pkg::axi_prot_t'(s_axi_arprot);
  assign aif.arqos     = axi4_pkg::axi_qos_t'(s_axi_arqos);
  assign aif.arregion  = axi4_pkg::axi_region_t'(s_axi_arregion);
  assign aif.arvalid   = s_axi_arvalid;
  assign s_axi_arready = aif.arready;

  assign s_axi_rid    = aif.rid;
  assign s_axi_rdata  = aif.rdata;
  assign s_axi_rresp  = aif.rresp;
  assign s_axi_rlast  = aif.rlast;
  assign s_axi_rvalid = aif.rvalid;
  assign aif.rready   = s_axi_rready;

endmodule


module axi4_pins_master_port #(
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned STRB_WIDTH = DATA_WIDTH / 8
) (
  // -- Write address channel (driven by the DUT) --
  output logic [ID_WIDTH-1:0]   m_axi_awid,
  output logic [ADDR_WIDTH-1:0] m_axi_awaddr,
  output logic [7:0]            m_axi_awlen,
  output logic [2:0]            m_axi_awsize,
  output logic [1:0]            m_axi_awburst,
  output logic                  m_axi_awlock,
  output logic [3:0]            m_axi_awcache,
  output logic [2:0]            m_axi_awprot,
  output logic [3:0]            m_axi_awqos,
  output logic [3:0]            m_axi_awregion,
  output logic                  m_axi_awvalid,
  input  logic                  m_axi_awready,

  // -- Write data channel --
  output logic [DATA_WIDTH-1:0] m_axi_wdata,
  output logic [STRB_WIDTH-1:0] m_axi_wstrb,
  output logic                  m_axi_wlast,
  output logic                  m_axi_wvalid,
  input  logic                  m_axi_wready,

  // -- Write response channel --
  input  logic [ID_WIDTH-1:0]   m_axi_bid,
  input  logic [1:0]            m_axi_bresp,
  input  logic                  m_axi_bvalid,
  output logic                  m_axi_bready,

  // -- Read address channel --
  output logic [ID_WIDTH-1:0]   m_axi_arid,
  output logic [ADDR_WIDTH-1:0] m_axi_araddr,
  output logic [7:0]            m_axi_arlen,
  output logic [2:0]            m_axi_arsize,
  output logic [1:0]            m_axi_arburst,
  output logic                  m_axi_arlock,
  output logic [3:0]            m_axi_arcache,
  output logic [2:0]            m_axi_arprot,
  output logic [3:0]            m_axi_arqos,
  output logic [3:0]            m_axi_arregion,
  output logic                  m_axi_arvalid,
  input  logic                  m_axi_arready,

  // -- Read data channel --
  input  logic [ID_WIDTH-1:0]   m_axi_rid,
  input  logic [DATA_WIDTH-1:0] m_axi_rdata,
  input  logic [1:0]            m_axi_rresp,
  input  logic                  m_axi_rlast,
  input  logic                  m_axi_rvalid,
  output logic                  m_axi_rready,

  axi4_if.slave aif
);

  assign m_axi_awid     = aif.awid;
  assign m_axi_awaddr   = aif.awaddr;
  assign m_axi_awlen    = aif.awlen;
  assign m_axi_awsize   = aif.awsize;
  assign m_axi_awburst  = aif.awburst;
  assign m_axi_awlock   = aif.awlock;
  assign m_axi_awcache  = aif.awcache;
  assign m_axi_awprot   = aif.awprot;
  assign m_axi_awqos    = aif.awqos;
  assign m_axi_awregion = aif.awregion;
  assign m_axi_awvalid  = aif.awvalid;
  assign aif.awready    = m_axi_awready;

  assign m_axi_wdata  = aif.wdata;
  assign m_axi_wstrb  = aif.wstrb;
  assign m_axi_wlast  = aif.wlast;
  assign m_axi_wvalid = aif.wvalid;
  assign aif.wready   = m_axi_wready;

  assign aif.bid    = m_axi_bid;
  assign aif.bresp  = axi4_pkg::axi_resp_t'(m_axi_bresp);
  assign aif.bvalid = m_axi_bvalid;
  assign m_axi_bready = aif.bready;

  assign m_axi_arid     = aif.arid;
  assign m_axi_araddr   = aif.araddr;
  assign m_axi_arlen    = aif.arlen;
  assign m_axi_arsize   = aif.arsize;
  assign m_axi_arburst  = aif.arburst;
  assign m_axi_arlock   = aif.arlock;
  assign m_axi_arcache  = aif.arcache;
  assign m_axi_arprot   = aif.arprot;
  assign m_axi_arqos    = aif.arqos;
  assign m_axi_arregion = aif.arregion;
  assign m_axi_arvalid  = aif.arvalid;
  assign aif.arready    = m_axi_arready;

  assign aif.rid    = m_axi_rid;
  assign aif.rdata  = m_axi_rdata;
  assign aif.rresp  = axi4_pkg::axi_resp_t'(m_axi_rresp);
  assign aif.rlast  = m_axi_rlast;
  assign aif.rvalid = m_axi_rvalid;
  assign m_axi_rready = aif.rready;

endmodule

`endif // AXI4_PINS_SV
