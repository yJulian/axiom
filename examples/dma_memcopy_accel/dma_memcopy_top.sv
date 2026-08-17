// dma_memcopy_top.sv
//
// TOP module Verilator elaborates for the DMA memcopy example: connects
// two pre-built AXI4 pin adapters (hw/axi4/axi4_pins.sv) -- one slave
// (control/status), one master (DMA) -- to the DUT (dma_memcopy.sv) over
// clean axi4_if handles. Its own boundary is flat scalar ports, matching
// the s_axi_*/m_axi_* naming DmaMemcopyAccel's C++ pin-accessor overrides
// (dma_memcopy_device.cc) talk to via the generated Vdma_memcopy_top.h.

module dma_memcopy_top #(
  parameter int unsigned NUM_CH     = 4,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT
) (
  input  logic clk_i,
  input  logic rst_ni,

  // -- Control/status slave port --
  input  logic [ID_WIDTH-1:0]     s_axi_awid,
  input  logic [ADDR_WIDTH-1:0]   s_axi_awaddr,
  input  logic [7:0]              s_axi_awlen,
  input  logic [2:0]              s_axi_awsize,
  input  logic [1:0]              s_axi_awburst,
  input  logic                    s_axi_awlock,
  input  logic [3:0]              s_axi_awcache,
  input  logic [2:0]              s_axi_awprot,
  input  logic [3:0]              s_axi_awqos,
  input  logic [3:0]              s_axi_awregion,
  input  logic                    s_axi_awvalid,
  output logic                    s_axi_awready,

  input  logic [DATA_WIDTH-1:0]   s_axi_wdata,
  input  logic [DATA_WIDTH/8-1:0] s_axi_wstrb,
  input  logic                    s_axi_wlast,
  input  logic                    s_axi_wvalid,
  output logic                    s_axi_wready,

  output logic [ID_WIDTH-1:0]     s_axi_bid,
  output logic [1:0]              s_axi_bresp,
  output logic                    s_axi_bvalid,
  input  logic                    s_axi_bready,

  input  logic [ID_WIDTH-1:0]     s_axi_arid,
  input  logic [ADDR_WIDTH-1:0]   s_axi_araddr,
  input  logic [7:0]              s_axi_arlen,
  input  logic [2:0]              s_axi_arsize,
  input  logic [1:0]              s_axi_arburst,
  input  logic                    s_axi_arlock,
  input  logic [3:0]              s_axi_arcache,
  input  logic [2:0]              s_axi_arprot,
  input  logic [3:0]              s_axi_arqos,
  input  logic [3:0]              s_axi_arregion,
  input  logic                    s_axi_arvalid,
  output logic                    s_axi_arready,

  output logic [ID_WIDTH-1:0]     s_axi_rid,
  output logic [DATA_WIDTH-1:0]   s_axi_rdata,
  output logic [1:0]              s_axi_rresp,
  output logic                    s_axi_rlast,
  output logic                    s_axi_rvalid,
  input  logic                    s_axi_rready,

  // -- DMA master port --
  output logic [ID_WIDTH-1:0]     m_axi_awid,
  output logic [ADDR_WIDTH-1:0]   m_axi_awaddr,
  output logic [7:0]              m_axi_awlen,
  output logic [2:0]              m_axi_awsize,
  output logic [1:0]              m_axi_awburst,
  output logic                    m_axi_awlock,
  output logic [3:0]              m_axi_awcache,
  output logic [2:0]              m_axi_awprot,
  output logic [3:0]              m_axi_awqos,
  output logic [3:0]              m_axi_awregion,
  output logic                    m_axi_awvalid,
  input  logic                    m_axi_awready,

  output logic [DATA_WIDTH-1:0]   m_axi_wdata,
  output logic [DATA_WIDTH/8-1:0] m_axi_wstrb,
  output logic                    m_axi_wlast,
  output logic                    m_axi_wvalid,
  input  logic                    m_axi_wready,

  input  logic [ID_WIDTH-1:0]     m_axi_bid,
  input  logic [1:0]              m_axi_bresp,
  input  logic                    m_axi_bvalid,
  output logic                    m_axi_bready,

  output logic [ID_WIDTH-1:0]     m_axi_arid,
  output logic [ADDR_WIDTH-1:0]   m_axi_araddr,
  output logic [7:0]              m_axi_arlen,
  output logic [2:0]              m_axi_arsize,
  output logic [1:0]              m_axi_arburst,
  output logic                    m_axi_arlock,
  output logic [3:0]              m_axi_arcache,
  output logic [2:0]              m_axi_arprot,
  output logic [3:0]              m_axi_arqos,
  output logic [3:0]              m_axi_arregion,
  output logic                    m_axi_arvalid,
  input  logic                    m_axi_arready,

  input  logic [ID_WIDTH-1:0]     m_axi_rid,
  input  logic [DATA_WIDTH-1:0]   m_axi_rdata,
  input  logic [1:0]              m_axi_rresp,
  input  logic                    m_axi_rlast,
  input  logic                    m_axi_rvalid,
  output logic                    m_axi_rready
);

  axi4_if #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) s_aif ();

  axi4_if #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) m_aif ();

  axi4_pins_slave_port #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) s_pins (
    .s_axi_awid(s_axi_awid), .s_axi_awaddr(s_axi_awaddr),
    .s_axi_awlen(s_axi_awlen), .s_axi_awsize(s_axi_awsize),
    .s_axi_awburst(s_axi_awburst), .s_axi_awlock(s_axi_awlock),
    .s_axi_awcache(s_axi_awcache), .s_axi_awprot(s_axi_awprot),
    .s_axi_awqos(s_axi_awqos), .s_axi_awregion(s_axi_awregion),
    .s_axi_awvalid(s_axi_awvalid), .s_axi_awready(s_axi_awready),

    .s_axi_wdata(s_axi_wdata), .s_axi_wstrb(s_axi_wstrb),
    .s_axi_wlast(s_axi_wlast), .s_axi_wvalid(s_axi_wvalid),
    .s_axi_wready(s_axi_wready),

    .s_axi_bid(s_axi_bid), .s_axi_bresp(s_axi_bresp),
    .s_axi_bvalid(s_axi_bvalid), .s_axi_bready(s_axi_bready),

    .s_axi_arid(s_axi_arid), .s_axi_araddr(s_axi_araddr),
    .s_axi_arlen(s_axi_arlen), .s_axi_arsize(s_axi_arsize),
    .s_axi_arburst(s_axi_arburst), .s_axi_arlock(s_axi_arlock),
    .s_axi_arcache(s_axi_arcache), .s_axi_arprot(s_axi_arprot),
    .s_axi_arqos(s_axi_arqos), .s_axi_arregion(s_axi_arregion),
    .s_axi_arvalid(s_axi_arvalid), .s_axi_arready(s_axi_arready),

    .s_axi_rid(s_axi_rid), .s_axi_rdata(s_axi_rdata),
    .s_axi_rresp(s_axi_rresp), .s_axi_rlast(s_axi_rlast),
    .s_axi_rvalid(s_axi_rvalid), .s_axi_rready(s_axi_rready),

    .aif(s_aif)
  );

  axi4_pins_master_port #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) m_pins (
    .m_axi_awid(m_axi_awid), .m_axi_awaddr(m_axi_awaddr),
    .m_axi_awlen(m_axi_awlen), .m_axi_awsize(m_axi_awsize),
    .m_axi_awburst(m_axi_awburst), .m_axi_awlock(m_axi_awlock),
    .m_axi_awcache(m_axi_awcache), .m_axi_awprot(m_axi_awprot),
    .m_axi_awqos(m_axi_awqos), .m_axi_awregion(m_axi_awregion),
    .m_axi_awvalid(m_axi_awvalid), .m_axi_awready(m_axi_awready),

    .m_axi_wdata(m_axi_wdata), .m_axi_wstrb(m_axi_wstrb),
    .m_axi_wlast(m_axi_wlast), .m_axi_wvalid(m_axi_wvalid),
    .m_axi_wready(m_axi_wready),

    .m_axi_bid(m_axi_bid), .m_axi_bresp(m_axi_bresp),
    .m_axi_bvalid(m_axi_bvalid), .m_axi_bready(m_axi_bready),

    .m_axi_arid(m_axi_arid), .m_axi_araddr(m_axi_araddr),
    .m_axi_arlen(m_axi_arlen), .m_axi_arsize(m_axi_arsize),
    .m_axi_arburst(m_axi_arburst), .m_axi_arlock(m_axi_arlock),
    .m_axi_arcache(m_axi_arcache), .m_axi_arprot(m_axi_arprot),
    .m_axi_arqos(m_axi_arqos), .m_axi_arregion(m_axi_arregion),
    .m_axi_arvalid(m_axi_arvalid), .m_axi_arready(m_axi_arready),

    .m_axi_rid(m_axi_rid), .m_axi_rdata(m_axi_rdata),
    .m_axi_rresp(m_axi_rresp), .m_axi_rlast(m_axi_rlast),
    .m_axi_rvalid(m_axi_rvalid), .m_axi_rready(m_axi_rready),

    .aif(m_aif)
  );

  // DUT (Design Under Test)
  dma_memcopy #(
    .NUM_CH    (NUM_CH),
    .DATA_WIDTH(DATA_WIDTH),
    .ADDR_WIDTH(ADDR_WIDTH),
    .ID_WIDTH  (ID_WIDTH)
  ) dut (
    .clk_i (clk_i),
    .rst_ni(rst_ni),
    .s_axi (s_aif),
    .m_axi (m_aif)
  );

endmodule
