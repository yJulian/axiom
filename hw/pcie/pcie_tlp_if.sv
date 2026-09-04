// pcie_tlp_if.sv
//
// The four TLP streams a PCIe endpoint's user logic sees, in the shape a
// hard IP block presents them (Xilinx UltraScale+ PCIe names in brackets):
//
//   cq -- completer request    [s_axis_cq]  host -> us: MemRd/MemWr hitting a BAR
//   cc -- completer completion [m_axis_cc]  us -> host: our completions for those
//   rq -- requester request    [m_axis_rq]  us -> host: our own DMA MemRd/MemWr
//   rc -- requester completion [s_axis_rc]  host -> us: completions for our reads
//
// Each is a plain AXI4-Stream: DATA_WIDTH-bit tdata, tlast, tvalid/tready.
// See pcie_tlp_pkg.sv for how TLPs are packed onto them.
//
// There is deliberately no TKEEP. A hard IP block carries one as a
// convenience, but on these streams it would be a second source of truth
// for something the packet already states exactly: a TLP header's `length`
// field (in DWs) plus its first/last byte enables fully determine how many
// bytes of the final beat are live. Two encodings of the same fact can
// disagree; one cannot. Beats are therefore always full DATA_WIDTH, and a
// final beat carrying a single DW simply leaves its upper DW undefined --
// the receiver knows to ignore it from `length`.
//
// Directions here are named from the *endpoint's* point of view, which is
// also the DUT's -- so the `endpoint` modport is what a DUT connects to and
// the `host` modport is what AXION's PcieTlpEngine drives.

interface pcie_tlp_if #(
  parameter int unsigned DATA_WIDTH = 64
);

  // Completer request: host -> endpoint
  logic [DATA_WIDTH-1:0] cq_tdata;
  logic                  cq_tlast;
  logic                  cq_tvalid;
  logic                  cq_tready;

  // Completer completion: endpoint -> host
  logic [DATA_WIDTH-1:0] cc_tdata;
  logic                  cc_tlast;
  logic                  cc_tvalid;
  logic                  cc_tready;

  // Requester request: endpoint -> host
  logic [DATA_WIDTH-1:0] rq_tdata;
  logic                  rq_tlast;
  logic                  rq_tvalid;
  logic                  rq_tready;

  // Requester completion: host -> endpoint
  logic [DATA_WIDTH-1:0] rc_tdata;
  logic                  rc_tlast;
  logic                  rc_tvalid;
  logic                  rc_tready;

  modport endpoint (
    input  cq_tdata, cq_tlast, cq_tvalid,
    output cq_tready,
    output cc_tdata, cc_tlast, cc_tvalid,
    input  cc_tready,
    output rq_tdata, rq_tlast, rq_tvalid,
    input  rq_tready,
    input  rc_tdata, rc_tlast, rc_tvalid,
    output rc_tready
  );

  modport host (
    output cq_tdata, cq_tlast, cq_tvalid,
    input  cq_tready,
    input  cc_tdata, cc_tlast, cc_tvalid,
    output cc_tready,
    input  rq_tdata, rq_tlast, rq_tvalid,
    output rq_tready,
    output rc_tdata, rc_tlast, rc_tvalid,
    input  rc_tready
  );

  // Read-only view of every signal, for tracing/protocol-checker modules.
  modport monitor (
    input cq_tdata, cq_tlast, cq_tvalid, cq_tready,
    input cc_tdata, cc_tlast, cc_tvalid, cc_tready,
    input rq_tdata, rq_tlast, rq_tvalid, rq_tready,
    input rc_tdata, rc_tlast, rc_tvalid, rc_tready
  );

endinterface
