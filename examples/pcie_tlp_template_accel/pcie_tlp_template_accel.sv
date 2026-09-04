// pcie_tlp_template_accel.sv
//
// DUT for AXION's TLP-level PCIe example. Same register set and same
// behavior as examples/pcie_template_accel/pcie_template_accel.sv -- and
// deliberately so, since the point of having both is to compare the two
// abstraction levels on identical functionality. What differs is
// everything about how a request arrives: instead of an AXI4 slave port
// with an address and a byte-enable, this DUT receives raw PCIe
// transaction-layer packets on a completer-request stream, parses the
// header itself (fmt, type, length, requester ID, tag, byte enables,
// 64-bit address), and builds the completion header itself (completer ID,
// status, byte count, lower address) on the completion stream. Its own
// DMA works the same way in reverse, on the requester streams.
//
// See hw/pcie/pcie_tlp_pkg.sv for the header layout and stream packing,
// including the one documented simplification (requests are 4-DW-header
// only; completion headers are padded to 4 DW so payload stays
// beat-aligned).
//
// BAR0 window (offsets are the low bits of the TLP's address, which is
// already BAR-relative -- src/pcie/pcie_tlp_engine.cc translates it, the
// same way the AXI4 path does):
//
//   0x0000 ID       RO 32b  0x50434945 ("PCIE")
//   0x0008 VERSION  RO 32b  0x00020000  (2 = the TLP-level variant)
//   0x0010 SCRATCH  RW 32b
//   0x0018 COUNTER  RO 32b  free-running cycle counter
//   0x0020 DMA_SRC  RW 64b
//   0x0028 DMA_DST  RW 64b
//   0x0030 DMA_LEN  RW 32b
//   0x0038 CTRL     RW 32b  [0]=START (pulse) [1]=IRQ_EN [2]=IRQ_FORCE (pulse)
//   0x0040 STATUS   RO 32b  [0]=BUSY [1]=DONE [2]=IRQ_PENDING
//   0x0048 IRQ_ACK  WO 32b  write 1 to bit[0]
//   0x1000..0x1FFF  RW      4 KiB scratchpad RAM
//
// Registers stay on an 8-byte stride for the same reason as in the AXI4
// variant, though for a different mechanism: a TLP carries its payload as
// DWs with per-DW byte enables, and keeping every register beat-aligned
// means a 64-bit access is one beat and a 32-bit access is the low DW of
// one beat, with no DW-level shifting anywhere in this file.

module pcie_tlp_template_accel #(
  parameter int unsigned RAM_WORDS = 512,     // 512 * 8 B = 4 KiB
  parameter logic [15:0] COMPLETER_ID = 16'h0008  // bus 0, dev 1, func 0
) (
  input  logic       clk_i,
  input  logic       rst_ni,
  pcie_tlp_if.endpoint tif,
  output logic       irq_o,
  output logic       busy_o
);
  import pcie_tlp_pkg::*;

  localparam int unsigned RAM_IDX_W = $clog2(RAM_WORDS);

  localparam logic [31:0] ID_MAGIC   = 32'h5043_4945;  // "PCIE"
  localparam logic [31:0] ID_VERSION = 32'h0002_0000;

  localparam logic [3:0] REGION_REGS = 4'h0;
  localparam logic [3:0] REGION_RAM  = 4'h1;

  localparam logic [7:0] REG_ID      = 8'h00;
  localparam logic [7:0] REG_VERSION = 8'h08;
  localparam logic [7:0] REG_SCRATCH = 8'h10;
  localparam logic [7:0] REG_COUNTER = 8'h18;
  localparam logic [7:0] REG_DMA_SRC = 8'h20;
  localparam logic [7:0] REG_DMA_DST = 8'h28;
  localparam logic [7:0] REG_DMA_LEN = 8'h30;
  localparam logic [7:0] REG_CTRL    = 8'h38;
  localparam logic [7:0] REG_STATUS  = 8'h40;
  localparam logic [7:0] REG_IRQ_ACK = 8'h48;

  // Our own DMA tags. Two distinct values so a reader can see which
  // completion belongs to which outstanding read; the engine echoes them
  // back in the completion header.
  localparam logic [7:0] DMA_TAG = 8'h20;

  // ---------------------------------------------------------------
  // State
  // ---------------------------------------------------------------
  logic [31:0] scratch_q;
  logic [31:0] counter_q;
  logic [63:0] dma_src_q;
  logic [63:0] dma_dst_q;
  logic [31:0] dma_len_q;
  logic        irq_en_q;

  logic [63:0] ram [RAM_WORDS];

  logic        dma_done_q;
  logic        irq_pending_q;

  assign irq_o = irq_pending_q & irq_en_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) counter_q <= '0;
    else         counter_q <= counter_q + 32'd1;
  end

  // ---------------------------------------------------------------
  // Completer request parser (CQ) + completion builder (CC).
  //
  // Beat 0 carries header DW1:DW0, beat 1 carries DW3:DW2 (the 64-bit
  // address). For a MemWr, payload beats follow; for a MemRd, beat 1 is
  // tlast and the DUT turns around a CplD.
  // ---------------------------------------------------------------
  typedef enum logic [2:0] {
    CQ_HDR0, CQ_HDR1, CQ_WDATA, CC_HDR0, CC_HDR1, CC_DATA
  } cq_state_t;

  cq_state_t             cq_state;
  logic [2:0]            req_fmt;
  logic [9:0]            req_len_dw;      // payload length in DWs
  logic [15:0]           req_requester;
  logic [7:0]            req_tag;
  logic [3:0]            req_first_be;
  logic [63:0]           req_addr;        // full 64-bit, byte-granular
  logic [9:0]            rd_dw_left;      // completion DWs still to send
  logic [63:0]           rd_addr;         // walks the read as it streams out
  logic                  wr_first_beat;   // gates req_first_be, see CQ_WDATA

  // Byte enables in force for the beat currently on CQ.
  logic [3:0] cur_be;
  assign cur_be = wr_first_beat ? req_first_be : 4'b1111;

  // Register/RAM read mux, combinational off rd_addr -- same shape as the
  // AXI4 variant's, so the two DUTs' register decode is comparable
  // line-for-line.
  logic [63:0] rd_data_mux;

  always_comb begin
    rd_data_mux = '0;
    if (rd_addr[15:12] == REGION_RAM) begin
      rd_data_mux = ram[rd_addr[RAM_IDX_W+2:3]];
    end else begin
      unique case (rd_addr[7:0])
        REG_ID:      rd_data_mux = 64'(ID_MAGIC);
        REG_VERSION: rd_data_mux = 64'(ID_VERSION);
        REG_SCRATCH: rd_data_mux = 64'(scratch_q);
        REG_COUNTER: rd_data_mux = 64'(counter_q);
        REG_DMA_SRC: rd_data_mux = dma_src_q;
        REG_DMA_DST: rd_data_mux = dma_dst_q;
        REG_DMA_LEN: rd_data_mux = 64'(dma_len_q);
        REG_CTRL:    rd_data_mux = 64'({30'd0, irq_en_q, 1'b0});
        REG_STATUS:  rd_data_mux = 64'({29'd0, irq_pending_q, dma_done_q,
                                         busy_o});
        default:     rd_data_mux = '0;
      endcase
    end
  end

  // Byte count reported in the completion header: DW length converted to
  // bytes, trimmed by the first-DW byte enables of the original request.
  logic [11:0] cpl_byte_count;
  assign cpl_byte_count = 12'(req_len_dw) << 2;

  assign tif.cq_tready = (cq_state == CQ_HDR0) || (cq_state == CQ_HDR1) ||
                          (cq_state == CQ_WDATA);

  always_comb begin
    tif.cc_tvalid = 1'b0;
    tif.cc_tdata  = '0;
    tif.cc_tlast  = 1'b0;
    unique case (cq_state)
      CC_HDR0: begin
        tif.cc_tvalid = 1'b1;
        tif.cc_tdata  = {make_cpl_dw1(COMPLETER_ID, CPL_SUCCESS,
                                       cpl_byte_count),
                         make_dw0(FMT_3DW_DATA, TYPE_CPL, req_len_dw)};
      end
      CC_HDR1: begin
        // DW3 is the pad that keeps payload beat-aligned; see
        // pcie_tlp_pkg.sv's header comment.
        tif.cc_tvalid = 1'b1;
        tif.cc_tdata  = {32'h0000_0000,
                         make_cpl_dw2(req_requester, req_tag,
                                       req_addr[6:0])};
      end
      CC_DATA: begin
        tif.cc_tvalid = 1'b1;
        tif.cc_tdata  = rd_data_mux;
        tif.cc_tlast  = (rd_dw_left <= 10'd2);
      end
      default: ;
    endcase
  end

  // Register-write pulses, consumed by the DMA/IRQ block below. Valid only
  // during the CQ payload handshake that carries the written DW.
  logic reg_wr_en;
  logic dma_start_pulse;
  logic irq_force_pulse;
  logic irq_ack_pulse;

  assign reg_wr_en = (cq_state == CQ_WDATA) && tif.cq_tvalid &&
                     tif.cq_tready && (req_addr[15:12] == REGION_REGS);
  assign dma_start_pulse = reg_wr_en && (req_addr[7:0] == REG_CTRL) &&
                           tif.cq_tdata[0];
  assign irq_force_pulse = reg_wr_en && (req_addr[7:0] == REG_CTRL) &&
                           tif.cq_tdata[2];
  assign irq_ack_pulse   = reg_wr_en && (req_addr[7:0] == REG_IRQ_ACK) &&
                           tif.cq_tdata[0];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      cq_state      <= CQ_HDR0;
      req_fmt       <= '0;
      req_len_dw    <= '0;
      req_requester <= '0;
      req_tag       <= '0;
      req_first_be  <= '0;
      req_addr      <= '0;
      rd_dw_left    <= '0;
      rd_addr       <= '0;
      wr_first_beat <= 1'b1;
      scratch_q     <= '0;
      dma_src_q     <= '0;
      dma_dst_q     <= '0;
      dma_len_q     <= '0;
      irq_en_q      <= 1'b0;
    end else begin
      unique case (cq_state)
        CQ_HDR0: begin
          if (tif.cq_tvalid && tif.cq_tready) begin
            req_fmt       <= dw0_fmt(tif.cq_tdata[31:0]);
            req_len_dw    <= dw0_length(tif.cq_tdata[31:0]);
            req_requester <= req_dw1_requester_id(tif.cq_tdata[63:32]);
            req_tag       <= req_dw1_tag(tif.cq_tdata[63:32]);
            req_first_be  <= req_dw1_first_be(tif.cq_tdata[63:32]);
            cq_state      <= CQ_HDR1;
          end
        end

        CQ_HDR1: begin
          if (tif.cq_tvalid && tif.cq_tready) begin
            // DW2 = address[63:32], DW3 = {address[31:2], 2'b00}.
            req_addr <= {tif.cq_tdata[31:0], tif.cq_tdata[63:32]};
            rd_addr  <= {tif.cq_tdata[31:0], tif.cq_tdata[63:32]};
            if (fmt_has_data(req_fmt)) begin
              wr_first_beat <= 1'b1;
              cq_state      <= CQ_WDATA;
            end else begin
              rd_dw_left <= req_len_dw;
              cq_state   <= CC_HDR0;
            end
          end
        end

        CQ_WDATA: begin
          if (tif.cq_tvalid && tif.cq_tready) begin
            wr_first_beat <= 1'b0;
            if (req_addr[15:12] == REGION_RAM) begin
              // Byte enables are a property of the request's *first* DW
              // only; every later DW of a contiguous write is fully
              // enabled (PCIe base spec 2.2.5). cur_be below is
              // req_first_be on the opening beat and 4'b1111 after it.
              if (cur_be[0]) ram[req_addr[RAM_IDX_W+2:3]][7:0]
                                 <= tif.cq_tdata[7:0];
              if (cur_be[1]) ram[req_addr[RAM_IDX_W+2:3]][15:8]
                                 <= tif.cq_tdata[15:8];
              if (cur_be[2]) ram[req_addr[RAM_IDX_W+2:3]][23:16]
                                 <= tif.cq_tdata[23:16];
              if (cur_be[3]) ram[req_addr[RAM_IDX_W+2:3]][31:24]
                                 <= tif.cq_tdata[31:24];
              // The header's DW count says whether this beat carries a
              // second DW; see hw/pcie/pcie_tlp_if.sv on why there is no
              // TKEEP to ask instead.
              if (req_len_dw >= 10'd2)
                ram[req_addr[RAM_IDX_W+2:3]][63:32] <= tif.cq_tdata[63:32];
            end else begin
              unique case (req_addr[7:0])
                REG_SCRATCH: scratch_q <= tif.cq_tdata[31:0];
                REG_DMA_SRC: dma_src_q <= tif.cq_tdata;
                REG_DMA_DST: dma_dst_q <= tif.cq_tdata;
                REG_DMA_LEN: dma_len_q <= tif.cq_tdata[31:0];
                REG_CTRL:    irq_en_q  <= tif.cq_tdata[1];
                default: ; // read-only, or handled by a pulse above
              endcase
            end

            if (tif.cq_tlast) begin
              cq_state <= CQ_HDR0;
            end else begin
              req_addr   <= req_addr + 64'd8;
              // Guarded: a request whose last beat carries a single DW
              // ends on tlast above, so this can only run with >= 2 left,
              // but an underflow here would silently corrupt the decode.
              req_len_dw <= (req_len_dw >= 10'd2) ? req_len_dw - 10'd2
                                                   : 10'd0;
            end
          end
        end

        CC_HDR0: if (tif.cc_tready) cq_state <= CC_HDR1;

        CC_HDR1: if (tif.cc_tready) cq_state <= CC_DATA;

        CC_DATA: begin
          if (tif.cc_tready) begin
            if (rd_dw_left <= 10'd2) begin
              cq_state <= CQ_HDR0;
            end else begin
              rd_dw_left <= rd_dw_left - 10'd2;
              rd_addr    <= rd_addr + 64'd8;
            end
          end
        end

        default: cq_state <= CQ_HDR0;
      endcase
    end
  end

  // ---------------------------------------------------------------
  // Bus-master DMA on the requester streams. Copies DMA_LEN bytes
  // SRC->DST as single-beat 8-byte read/write pairs: a MemRd TLP on RQ,
  // wait for its CplD on RC, then a MemWr TLP on RQ.
  // ---------------------------------------------------------------
  typedef enum logic [2:0] {
    D_IDLE, D_RD_HDR0, D_RD_HDR1, D_WAIT_CPL,
    D_WR_HDR0, D_WR_HDR1, D_WR_DATA
  } dma_state_t;

  dma_state_t  dma_state;
  logic [63:0] dma_cur_src;
  logic [63:0] dma_cur_dst;
  logic [31:0] dma_rem;
  logic [63:0] dma_buf;

  assign busy_o = (dma_state != D_IDLE);

  // Whole 8-byte beats, plus a possibly-partial tail.
  // Last-DW byte enables. Must be 0 for a single-DW request (PCIe base
  // spec 2.2.5); otherwise the tail DW's valid bytes.
  logic [3:0] dma_tail_be;
  assign dma_tail_be = (dma_rem <= 32'd4)  ? 4'b0000
                     : (dma_rem >= 32'd8)  ? 4'b1111
                                           : 4'((1 << (dma_rem - 32'd4)) - 1);

  // 2 DWs when at least 8 bytes remain, else 1.
  logic [9:0] dma_len_dw;
  assign dma_len_dw = (dma_rem > 32'd4) ? 10'd2 : 10'd1;

  always_comb begin
    tif.rq_tvalid = 1'b0;
    tif.rq_tdata  = '0;
    tif.rq_tlast  = 1'b0;
    unique case (dma_state)
      D_RD_HDR0: begin
        tif.rq_tvalid = 1'b1;
        tif.rq_tdata  = {make_req_dw1(COMPLETER_ID, DMA_TAG, dma_tail_be,
                                       4'b1111),
                         make_dw0(FMT_4DW_NODATA, TYPE_MEM, dma_len_dw)};
      end
      D_RD_HDR1: begin
        tif.rq_tvalid = 1'b1;
        tif.rq_tdata  = {dma_cur_src[31:0] & 32'hFFFF_FFFC,
                         dma_cur_src[63:32]};
        tif.rq_tlast  = 1'b1;
      end
      D_WR_HDR0: begin
        tif.rq_tvalid = 1'b1;
        tif.rq_tdata  = {make_req_dw1(COMPLETER_ID, DMA_TAG, dma_tail_be,
                                       4'b1111),
                         make_dw0(FMT_4DW_DATA, TYPE_MEM, dma_len_dw)};
      end
      D_WR_HDR1: begin
        tif.rq_tvalid = 1'b1;
        tif.rq_tdata  = {dma_cur_dst[31:0] & 32'hFFFF_FFFC,
                         dma_cur_dst[63:32]};
      end
      D_WR_DATA: begin
        tif.rq_tvalid = 1'b1;
        tif.rq_tdata  = dma_buf;
        tif.rq_tlast  = 1'b1;
      end
      default: ;
    endcase
  end

  // The engine sends a completion for our MemRd as a padded 4-DW header
  // followed by payload, matching what this DUT emits on CC.
  typedef enum logic [1:0] { RC_HDR0, RC_HDR1, RC_DATA } rc_state_t;
  rc_state_t rc_state;

  assign tif.rc_tready = 1'b1;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      dma_state     <= D_IDLE;
      dma_cur_src   <= '0;
      dma_cur_dst   <= '0;
      dma_rem       <= '0;
      dma_buf       <= '0;
      dma_done_q    <= 1'b0;
      irq_pending_q <= 1'b0;
      rc_state      <= RC_HDR0;
    end else begin
      if (irq_ack_pulse) begin
        irq_pending_q <= 1'b0;
        dma_done_q    <= 1'b0;
      end
      if (irq_force_pulse)
        irq_pending_q <= 1'b1;

      // Drain the requester-completion stream regardless of state; only
      // the payload beat is of interest, and only while a read is
      // outstanding.
      if (tif.rc_tvalid && tif.rc_tready) begin
        unique case (rc_state)
          RC_HDR0: rc_state <= RC_HDR1;
          RC_HDR1: rc_state <= RC_DATA;
          RC_DATA: begin
            dma_buf <= tif.rc_tdata;
            if (tif.rc_tlast) rc_state <= RC_HDR0;
          end
          default: rc_state <= RC_HDR0;
        endcase
      end

      unique case (dma_state)
        D_IDLE: begin
          if (dma_start_pulse && dma_len_q != 32'd0) begin
            dma_cur_src <= dma_src_q;
            dma_cur_dst <= dma_dst_q;
            dma_rem     <= dma_len_q;
            dma_done_q  <= 1'b0;
            dma_state   <= D_RD_HDR0;
          end
        end

        D_RD_HDR0: if (tif.rq_tready) dma_state <= D_RD_HDR1;
        D_RD_HDR1: if (tif.rq_tready) dma_state <= D_WAIT_CPL;

        D_WAIT_CPL: begin
          if (tif.rc_tvalid && tif.rc_tready && rc_state == RC_DATA &&
              tif.rc_tlast)
            dma_state <= D_WR_HDR0;
        end

        D_WR_HDR0: if (tif.rq_tready) dma_state <= D_WR_HDR1;
        D_WR_HDR1: if (tif.rq_tready) dma_state <= D_WR_DATA;

        D_WR_DATA: begin
          if (tif.rq_tready) begin
            if (dma_rem <= 32'd8) begin
              dma_rem       <= '0;
              dma_done_q    <= 1'b1;
              irq_pending_q <= 1'b1;
              dma_state     <= D_IDLE;
            end else begin
              dma_cur_src <= dma_cur_src + 64'd8;
              dma_cur_dst <= dma_cur_dst + 64'd8;
              dma_rem     <= dma_rem - 32'd8;
              dma_state   <= D_RD_HDR0;
            end
          end
        end

        default: dma_state <= D_IDLE;
      endcase
    end
  end

endmodule
