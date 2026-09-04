// pcie_tlp_pins.sv
//
// Flat-port adapters for pcie_tlp_if, the TLP-level counterpart to
// hw/axi4/axi4_pins.sv: a Verilator-generated C++ model only exposes
// individually gettable/settable signals at the top module's boundary, so a
// TOP module wraps one of these around the clean interface handle its DUT
// connects to.
//
// Only one adapter is needed here, not two: unlike AXI4 -- where a port's
// direction depends on whether the DUT is master or slave -- a PCIe endpoint
// always has all four streams, with fixed directions. The `endpoint` role is
// the only one a DUT ever plays.
//
// Port naming follows the same convention as the AXI4 adapters: the prefix
// names the stream (cq_/cc_/rq_/rc_), and the direction is the DUT's.

`ifndef PCIE_TLP_PINS_SV
`define PCIE_TLP_PINS_SV

module pcie_tlp_pins_endpoint_port #(
  parameter int unsigned DATA_WIDTH = 64
) (
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
  output logic                  rc_tready,

  pcie_tlp_if.host tif
);

  assign tif.cq_tdata  = cq_tdata;
  assign tif.cq_tlast  = cq_tlast;
  assign tif.cq_tvalid = cq_tvalid;
  assign cq_tready     = tif.cq_tready;

  assign cc_tdata      = tif.cc_tdata;
  assign cc_tlast      = tif.cc_tlast;
  assign cc_tvalid     = tif.cc_tvalid;
  assign tif.cc_tready = cc_tready;

  assign rq_tdata      = tif.rq_tdata;
  assign rq_tlast      = tif.rq_tlast;
  assign rq_tvalid     = tif.rq_tvalid;
  assign tif.rq_tready = rq_tready;

  assign tif.rc_tdata  = rc_tdata;
  assign tif.rc_tlast  = rc_tlast;
  assign tif.rc_tvalid = rc_tvalid;
  assign rc_tready     = tif.rc_tready;

endmodule

`endif // PCIE_TLP_PINS_SV
