// pcie_tlp_pkg.sv
//
// PCIe Transaction Layer Packet definitions, for the TLP-level variant of
// AXION's PCIe path. Where the AXI4-bridge path (src/dev/rtl/rtl_pci_device.hh,
// examples/pcie_template_accel/) hands the RTL an ordinary AXI4 slave port and
// keeps every trace of PCIe on the gem5 side, this path hands the RTL genuine
// transaction-layer packets and makes it parse and build the headers itself --
// the level a PCIe hard IP block's CQ/CC/RQ/RC interfaces operate at (Xilinx
// UltraScale+ PCIe, Intel's Avalon-ST equivalent).
//
// STREAM PACKING, and the one deliberate simplification.
//
// TLPs travel on a 64-bit stream, two DWs per beat, DW N in bits [31:0] and
// DW N+1 in bits [63:32]. Requests use the 4-DW (64-bit address) header form
// exclusively -- AXION's PcieTlpEngine never emits the 3-DW form, and the DUT
// rejects it -- so a request's payload always starts on a beat boundary.
//
// Completions have a 3-DW header, which would leave their payload straddling
// beats. Rather than build a barrel shifter into a template, this path pads
// the completion header to 4 DW with one reserved DW, keeping payload
// beat-aligned. That is a packing convention on the local stream, not a change
// to any header field: everything the RTL parses and builds -- fmt, type,
// length, requester ID, tag, byte enables, completion status, byte count,
// lower address -- is the real thing, at the real bit positions. A production
// endpoint would have to handle both header sizes and the misalignment they
// imply; a template does not, and saying so here is cheaper than discovering
// it later.

package pcie_tlp_pkg;

  // TLP format field (header DW0 [31:29]). Bit 0 selects a 4-DW header,
  // bit 1 says a data payload follows.
  localparam logic [2:0] FMT_3DW_NODATA = 3'b000;
  localparam logic [2:0] FMT_4DW_NODATA = 3'b001;
  localparam logic [2:0] FMT_3DW_DATA   = 3'b010;
  localparam logic [2:0] FMT_4DW_DATA   = 3'b011;

  // TLP type field (header DW0 [28:24]).
  localparam logic [4:0] TYPE_MEM = 5'b00000;  // MRd (no data) / MWr (data)
  localparam logic [4:0] TYPE_CPL = 5'b01010;  // Cpl (no data) / CplD (data)

  // Completion status (completion header DW1 [15:13]).
  localparam logic [2:0] CPL_SUCCESS = 3'b000;
  localparam logic [2:0] CPL_UR      = 3'b001;  // unsupported request
  localparam logic [2:0] CPL_CA      = 3'b100;  // completer abort

  // --- Header DW0, common to every TLP ---
  function automatic logic [2:0] dw0_fmt(logic [31:0] dw0);
    return dw0[31:29];
  endfunction

  function automatic logic [4:0] dw0_type(logic [31:0] dw0);
    return dw0[28:24];
  endfunction

  // Payload length in DWs. The wire encodes 1024 DW as 0; callers that care
  // about the maximum must special-case it (nothing in this repo issues one).
  function automatic logic [9:0] dw0_length(logic [31:0] dw0);
    return dw0[9:0];
  endfunction

  function automatic logic dw0_has_data(logic [31:0] dw0);
    return dw0[30];
  endfunction

  function automatic logic dw0_is_4dw(logic [31:0] dw0);
    return dw0[29];
  endfunction

  // Same two predicates against a bare fmt field, for code that has
  // already latched fmt out of DW0.
  function automatic logic fmt_has_data(logic [2:0] fmt);
    return fmt[1];
  endfunction

  function automatic logic fmt_is_4dw(logic [2:0] fmt);
    return fmt[0];
  endfunction

  function automatic logic [31:0] make_dw0(logic [2:0] fmt, logic [4:0] ttype,
                                            logic [9:0] length);
    return {fmt, ttype, 14'h0000, length};
  endfunction

  // --- Request header DW1 ---
  function automatic logic [15:0] req_dw1_requester_id(logic [31:0] dw1);
    return dw1[31:16];
  endfunction

  function automatic logic [7:0] req_dw1_tag(logic [31:0] dw1);
    return dw1[15:8];
  endfunction

  function automatic logic [3:0] req_dw1_last_be(logic [31:0] dw1);
    return dw1[7:4];
  endfunction

  function automatic logic [3:0] req_dw1_first_be(logic [31:0] dw1);
    return dw1[3:0];
  endfunction

  function automatic logic [31:0] make_req_dw1(logic [15:0] requester_id,
                                                logic [7:0] tag,
                                                logic [3:0] last_be,
                                                logic [3:0] first_be);
    return {requester_id, tag, last_be, first_be};
  endfunction

  // --- Completion header ---
  function automatic logic [31:0] make_cpl_dw1(logic [15:0] completer_id,
                                                logic [2:0] status,
                                                logic [11:0] byte_count);
    return {completer_id, status, 1'b0, byte_count};
  endfunction

  function automatic logic [31:0] make_cpl_dw2(logic [15:0] requester_id,
                                                logic [7:0] tag,
                                                logic [6:0] lower_address);
    return {requester_id, tag, 1'b0, lower_address};
  endfunction

  function automatic logic [2:0] cpl_dw1_status(logic [31:0] dw1);
    return dw1[15:13];
  endfunction

  function automatic logic [11:0] cpl_dw1_byte_count(logic [31:0] dw1);
    return dw1[11:0];
  endfunction

  function automatic logic [7:0] cpl_dw2_tag(logic [31:0] dw2);
    return dw2[15:8];
  endfunction

endpackage
