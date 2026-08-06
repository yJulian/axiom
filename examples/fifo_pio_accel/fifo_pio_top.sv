// fifo_pio_top.sv
//
// TOP module Verilator elaborates for the FIFO/PIO example: connects the
// pre-built AXI4 pin adapter (hw/axi4/axi4_pins.sv) to the DUT
// (fifo_pio_accel.sv) over a clean axi4_if handle. Its own boundary is
// flat scalar ports -- exactly what FifoPioAccel's C++ pin-accessor
// overrides (fifo_pio_device.cc) talk to via the generated Vfifo_pio_top.h.

module fifo_pio_top #(
  parameter int unsigned FIFO_DEPTH = 4,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT
) (
  input  logic clk_i,
  input  logic rst_ni,

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
  input  logic                    s_axi_rready
);

  axi4_if #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) aif ();

  axi4_pins_slave_port #(
    .ID_WIDTH(ID_WIDTH), .ADDR_WIDTH(ADDR_WIDTH), .DATA_WIDTH(DATA_WIDTH)
  ) pins (
    .s_axi_awid    (s_axi_awid),
    .s_axi_awaddr  (s_axi_awaddr),
    .s_axi_awlen   (s_axi_awlen),
    .s_axi_awsize  (s_axi_awsize),
    .s_axi_awburst (s_axi_awburst),
    .s_axi_awlock  (s_axi_awlock),
    .s_axi_awcache (s_axi_awcache),
    .s_axi_awprot  (s_axi_awprot),
    .s_axi_awqos   (s_axi_awqos),
    .s_axi_awregion(s_axi_awregion),
    .s_axi_awvalid (s_axi_awvalid),
    .s_axi_awready (s_axi_awready),

    .s_axi_wdata  (s_axi_wdata),
    .s_axi_wstrb  (s_axi_wstrb),
    .s_axi_wlast  (s_axi_wlast),
    .s_axi_wvalid (s_axi_wvalid),
    .s_axi_wready (s_axi_wready),

    .s_axi_bid    (s_axi_bid),
    .s_axi_bresp  (s_axi_bresp),
    .s_axi_bvalid (s_axi_bvalid),
    .s_axi_bready (s_axi_bready),

    .s_axi_arid    (s_axi_arid),
    .s_axi_araddr  (s_axi_araddr),
    .s_axi_arlen   (s_axi_arlen),
    .s_axi_arsize  (s_axi_arsize),
    .s_axi_arburst (s_axi_arburst),
    .s_axi_arlock  (s_axi_arlock),
    .s_axi_arcache (s_axi_arcache),
    .s_axi_arprot  (s_axi_arprot),
    .s_axi_arqos   (s_axi_arqos),
    .s_axi_arregion(s_axi_arregion),
    .s_axi_arvalid (s_axi_arvalid),
    .s_axi_arready (s_axi_arready),

    .s_axi_rid    (s_axi_rid),
    .s_axi_rdata  (s_axi_rdata),
    .s_axi_rresp  (s_axi_rresp),
    .s_axi_rlast  (s_axi_rlast),
    .s_axi_rvalid (s_axi_rvalid),
    .s_axi_rready (s_axi_rready),

    .aif(aif)
  );

  // DUT (Design Under Test)
  fifo_pio_accel #(
    .N         (FIFO_DEPTH),
    .DATA_WIDTH(DATA_WIDTH),
    .ID_WIDTH  (ID_WIDTH)
  ) dut (
    .clk_i (clk_i),
    .rst_ni(rst_ni),
    .s_axi (aif)
  );

endmodule
