// pcie_tlp_template_top.sv
//
// TOP module Verilator elaborates for the TLP-level PCIe example: wraps
// the flat-port adapter (hw/pcie/pcie_tlp_pins.sv) around a clean
// pcie_tlp_if handle and connects the DUT to it. Its own boundary is flat
// scalar ports, matching the cq_*/cc_*/rq_*/rc_* naming
// PcieTlpTemplateAccel's C++ pin-accessor overrides
// (pcie_tlp_template_device.cc) talk to via the generated
// Vpcie_tlp_template_top.h.
//
// One adapter, not two: a PCIe endpoint always has all four streams with
// fixed directions, so there is no master/slave role to pick the way there
// is for AXI4 (see hw/pcie/pcie_tlp_pins.sv).

module pcie_tlp_template_top #(
  parameter int unsigned RAM_WORDS  = 512,
  parameter int unsigned DATA_WIDTH = 64
) (
  input  logic clk_i,
  input  logic rst_ni,

  // -- Sideband --
  output logic                  irq_o,
  output logic                  busy_o,

  // -- Completer request (host -> DUT) --
  input  logic [DATA_WIDTH-1:0] cq_tdata,
  input  logic                  cq_tlast,
  input  logic                  cq_tvalid,
  output logic                  cq_tready,

  // -- Completer completion (DUT -> host) --
  output logic [DATA_WIDTH-1:0] cc_tdata,
  output logic                  cc_tlast,
  output logic                  cc_tvalid,
  input  logic                  cc_tready,

  // -- Requester request (DUT -> host) --
  output logic [DATA_WIDTH-1:0] rq_tdata,
  output logic                  rq_tlast,
  output logic                  rq_tvalid,
  input  logic                  rq_tready,

  // -- Requester completion (host -> DUT) --
  input  logic [DATA_WIDTH-1:0] rc_tdata,
  input  logic                  rc_tlast,
  input  logic                  rc_tvalid,
  output logic                  rc_tready
);

  pcie_tlp_if #(.DATA_WIDTH(DATA_WIDTH)) tif ();

  pcie_tlp_pins_endpoint_port #(.DATA_WIDTH(DATA_WIDTH)) pins (
    .cq_tdata(cq_tdata), .cq_tlast(cq_tlast),
    .cq_tvalid(cq_tvalid), .cq_tready(cq_tready),

    .cc_tdata(cc_tdata), .cc_tlast(cc_tlast),
    .cc_tvalid(cc_tvalid), .cc_tready(cc_tready),

    .rq_tdata(rq_tdata), .rq_tlast(rq_tlast),
    .rq_tvalid(rq_tvalid), .rq_tready(rq_tready),

    .rc_tdata(rc_tdata), .rc_tlast(rc_tlast),
    .rc_tvalid(rc_tvalid), .rc_tready(rc_tready),

    .tif(tif)
  );

  // DUT (Design Under Test)
  pcie_tlp_template_accel #(
    .RAM_WORDS(RAM_WORDS)
  ) dut (
    .clk_i (clk_i),
    .rst_ni(rst_ni),
    .tif   (tif),
    .irq_o (irq_o),
    .busy_o(busy_o)
  );

endmodule
