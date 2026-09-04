// flexnngine2_rtl3_axion_top
//
// TOP module Verilator elaborates for the rtl3 (scratchpad-fed FleXNNgine2
// GEMM accelerator) AXION port. Unlike dma_memcopy_top.sv this does NOT
// instantiate hw/axi4/axi4_pins -- the DUT (flexnngine2_rtl3_top.sv, from
// the sibling gem5_cva6 checkout, see ../README.md) already exposes flat
// s_axi_*/m_axi_* ports, so this module's only job is to pad them out to
// the full AXI4 field set axion::Axi4SlavePins/Axi4MasterPins expect and
// adapt the two genuine mismatches:
//
//   1. The control/status port is real AXI4-Lite (no ID, no burst fields
//      at all). Every register access from Axi4SlaveEngine is single-beat
//      (awlen/arlen are ignored below -- the whole reason this works
//      without real burst-splitting logic), so all that's needed is to
//      latch AWID/ARID on the AW/AR handshake and echo them back on
//      BID/RID; nothing else about a burst needs modeling.
//   2. Neither AXI4 port drives/has AxLOCK/AxCACHE/AxPROT/AxQOS/AxREGION.
//      Tied to 0 on the (few) inputs the DUT would otherwise need driven;
//      the corresponding outputs from this module are just constants.
//
// Every other field (IDs and all burst fields on the DMA master port; the
// core AW/W/B/AR/R handshake signals on both ports) lines up 1:1 with the
// DUT's own port names and widths -- plain wires, no adapter logic.
module flexnngine2_rtl3_axion_top #(
    parameter int unsigned MESH_PE_ROW = 8,
    parameter int unsigned MESH_PE_COL = 8,
    parameter int unsigned TILE_K      = MESH_PE_ROW,
    // Slave (control/status) port widths -- match the DUT's own
    // AXI_ADDR_WIDTH/AXI_DATA_WIDTH exactly so no data-width adaptation is
    // needed either, only the ID latch described above.
    parameter int unsigned S_ADDR_WIDTH = 12,
    parameter int unsigned S_DATA_WIDTH = 32,
    parameter int unsigned S_ID_WIDTH   = 4,
    // Master (DMA) port widths -- match the DUT's MEM_ADDR_WIDTH/
    // MEM_DATA_WIDTH and its 8-bit AXI IDs exactly.
    parameter int unsigned M_ADDR_WIDTH = 64,
    parameter int unsigned M_DATA_WIDTH = 64,
    parameter int unsigned M_ID_WIDTH   = 8
) (
    input  logic clk_i,
    input  logic rst_ni,

    // -- Control/status slave port (full AXI4 pin set) --
    input  logic [S_ID_WIDTH-1:0]     s_axi_awid,
    input  logic [S_ADDR_WIDTH-1:0]   s_axi_awaddr,
    input  logic [7:0]                s_axi_awlen,
    input  logic [2:0]                s_axi_awsize,
    input  logic [1:0]                s_axi_awburst,
    input  logic                      s_axi_awlock,
    input  logic [3:0]                s_axi_awcache,
    input  logic [2:0]                s_axi_awprot,
    input  logic [3:0]                s_axi_awqos,
    input  logic [3:0]                s_axi_awregion,
    input  logic                      s_axi_awvalid,
    output logic                      s_axi_awready,

    input  logic [S_DATA_WIDTH-1:0]   s_axi_wdata,
    input  logic [S_DATA_WIDTH/8-1:0] s_axi_wstrb,
    input  logic                      s_axi_wlast,
    input  logic                      s_axi_wvalid,
    output logic                      s_axi_wready,

    output logic [S_ID_WIDTH-1:0]     s_axi_bid,
    output logic [1:0]                s_axi_bresp,
    output logic                      s_axi_bvalid,
    input  logic                      s_axi_bready,

    input  logic [S_ID_WIDTH-1:0]     s_axi_arid,
    input  logic [S_ADDR_WIDTH-1:0]   s_axi_araddr,
    input  logic [7:0]                s_axi_arlen,
    input  logic [2:0]                s_axi_arsize,
    input  logic [1:0]                s_axi_arburst,
    input  logic                      s_axi_arlock,
    input  logic [3:0]                s_axi_arcache,
    input  logic [2:0]                s_axi_arprot,
    input  logic [3:0]                s_axi_arqos,
    input  logic [3:0]                s_axi_arregion,
    input  logic                      s_axi_arvalid,
    output logic                      s_axi_arready,

    output logic [S_ID_WIDTH-1:0]     s_axi_rid,
    output logic [S_DATA_WIDTH-1:0]   s_axi_rdata,
    output logic [1:0]                s_axi_rresp,
    output logic                      s_axi_rlast,
    output logic                      s_axi_rvalid,
    input  logic                      s_axi_rready,

    // -- DMA master port (full AXI4 pin set) --
    output logic [M_ID_WIDTH-1:0]     m_axi_awid,
    output logic [M_ADDR_WIDTH-1:0]   m_axi_awaddr,
    output logic [7:0]                m_axi_awlen,
    output logic [2:0]                m_axi_awsize,
    output logic [1:0]                m_axi_awburst,
    output logic                      m_axi_awlock,
    output logic [3:0]                m_axi_awcache,
    output logic [2:0]                m_axi_awprot,
    output logic [3:0]                m_axi_awqos,
    output logic [3:0]                m_axi_awregion,
    output logic                      m_axi_awvalid,
    input  logic                      m_axi_awready,

    output logic [M_DATA_WIDTH-1:0]   m_axi_wdata,
    output logic [M_DATA_WIDTH/8-1:0] m_axi_wstrb,
    output logic                      m_axi_wlast,
    output logic                      m_axi_wvalid,
    input  logic                      m_axi_wready,

    input  logic [M_ID_WIDTH-1:0]     m_axi_bid,
    input  logic [1:0]                m_axi_bresp,
    input  logic                      m_axi_bvalid,
    output logic                      m_axi_bready,

    output logic [M_ID_WIDTH-1:0]     m_axi_arid,
    output logic [M_ADDR_WIDTH-1:0]   m_axi_araddr,
    output logic [7:0]                m_axi_arlen,
    output logic [2:0]                m_axi_arsize,
    output logic [1:0]                m_axi_arburst,
    output logic                      m_axi_arlock,
    output logic [3:0]                m_axi_arcache,
    output logic [2:0]                m_axi_arprot,
    output logic [3:0]                m_axi_arqos,
    output logic [3:0]                m_axi_arregion,
    output logic                      m_axi_arvalid,
    input  logic                      m_axi_arready,

    input  logic [M_ID_WIDTH-1:0]     m_axi_rid,
    input  logic [M_DATA_WIDTH-1:0]   m_axi_rdata,
    input  logic [1:0]                m_axi_rresp,
    input  logic                      m_axi_rlast,
    input  logic                      m_axi_rvalid,
    output logic                      m_axi_rready,

    // Idle-clock-gating hint for RTLDmaDevice::isIdle() (see
    // Flexnngine2Rtl3Accel::isIdle() in flexnngine2_rtl3_device.cc) -- the
    // DUT's own "engine busy" output, high for the whole duration of a job
    // including the internal compute phase between the operand DMA reads
    // completing and the result DMA write starting, when there is zero AXI
    // traffic in flight on either port.
    output logic dma_busy_o,

    // TEMPORARY debug taps (hierarchical reference into the DUT, which
    // doesn't expose these as ports) -- packed {store,compute,load} engine
    // states, see flexnngine2_rtl3_engine.sv. Remove once the rtl3 port's
    // stuck-mid-job issue is root-caused.
    output logic [31:0] dbg_state_o
);

  // -- Control port: AXI4 -> AXI4-Lite ID latch --
  logic [S_ID_WIDTH-1:0] awid_q, arid_q;
  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      awid_q <= '0;
      arid_q <= '0;
    end else begin
      if (s_axi_awvalid && s_axi_awready) awid_q <= s_axi_awid;
      if (s_axi_arvalid && s_axi_arready) arid_q <= s_axi_arid;
    end
  end
  assign s_axi_bid = awid_q;
  assign s_axi_rid = arid_q;

  // -- DMA master port: fields the DUT doesn't drive at all --
  assign m_axi_awlock   = 1'b0;
  assign m_axi_awcache  = 4'b0000;
  assign m_axi_awprot   = 3'b000;
  assign m_axi_awqos    = 4'b0000;
  assign m_axi_awregion = 4'b0000;
  assign m_axi_arlock   = 1'b0;
  assign m_axi_arcache  = 4'b0000;
  assign m_axi_arprot   = 3'b000;
  assign m_axi_arqos    = 4'b0000;
  assign m_axi_arregion = 4'b0000;

  logic dma_done_unused;

  flexnngine2_rtl3_top #(
      .MESH_PE_ROW    (MESH_PE_ROW),
      .MESH_PE_COL    (MESH_PE_COL),
      .TILE_K         (TILE_K),
      .AXI_ADDR_WIDTH (S_ADDR_WIDTH),
      .AXI_DATA_WIDTH (S_DATA_WIDTH),
      .MEM_ADDR_WIDTH (M_ADDR_WIDTH),
      .MEM_DATA_WIDTH (M_DATA_WIDTH)
  ) dut (
      .i_clk   (clk_i),
      .i_rst_n (rst_ni),

      .s_axi_awaddr  (s_axi_awaddr),
      .s_axi_awvalid (s_axi_awvalid),
      .s_axi_awready (s_axi_awready),
      .s_axi_wdata   (s_axi_wdata),
      .s_axi_wstrb   (s_axi_wstrb),
      .s_axi_wvalid  (s_axi_wvalid),
      .s_axi_wready  (s_axi_wready),
      .s_axi_bresp   (s_axi_bresp),
      .s_axi_bvalid  (s_axi_bvalid),
      .s_axi_bready  (s_axi_bready),
      .s_axi_araddr  (s_axi_araddr),
      .s_axi_arvalid (s_axi_arvalid),
      .s_axi_arready (s_axi_arready),
      .s_axi_rdata   (s_axi_rdata),
      .s_axi_rresp   (s_axi_rresp),
      .s_axi_rvalid  (s_axi_rvalid),
      .s_axi_rready  (s_axi_rready),

      .m_axi_awaddr  (m_axi_awaddr),
      .m_axi_awlen   (m_axi_awlen),
      .m_axi_awsize  (m_axi_awsize),
      .m_axi_awburst (m_axi_awburst),
      .m_axi_awvalid (m_axi_awvalid),
      .m_axi_awready (m_axi_awready),
      .m_axi_awid    (m_axi_awid),
      .m_axi_wdata   (m_axi_wdata),
      .m_axi_wstrb   (m_axi_wstrb),
      .m_axi_wlast   (m_axi_wlast),
      .m_axi_wvalid  (m_axi_wvalid),
      .m_axi_wready  (m_axi_wready),
      .m_axi_bresp   (m_axi_bresp),
      .m_axi_bvalid  (m_axi_bvalid),
      .m_axi_bready  (m_axi_bready),
      .m_axi_bid     (m_axi_bid),
      .m_axi_araddr  (m_axi_araddr),
      .m_axi_arlen   (m_axi_arlen),
      .m_axi_arsize  (m_axi_arsize),
      .m_axi_arburst (m_axi_arburst),
      .m_axi_arvalid (m_axi_arvalid),
      .m_axi_arready (m_axi_arready),
      .m_axi_arid    (m_axi_arid),
      .m_axi_rdata   (m_axi_rdata),
      .m_axi_rresp   (m_axi_rresp),
      .m_axi_rlast   (m_axi_rlast),
      .m_axi_rvalid  (m_axi_rvalid),
      .m_axi_rready  (m_axi_rready),
      .m_axi_rid     (m_axi_rid),

      .dma_busy_o (dma_busy_o),
      .dma_done_o (dma_done_unused)
  );

  assign dbg_state_o = dut.dbg_state;

  // s_axi_wlast/awlen/awsize/awburst/awlock/awcache/awprot/awqos/awregion
  // and their AR equivalents are genuinely unused above: the DUT's
  // control port is AXI4-Lite and never looks at burst/QoS/protection
  // metadata (-Wno-UNUSEDSIGNAL, see the Makefile).

endmodule
