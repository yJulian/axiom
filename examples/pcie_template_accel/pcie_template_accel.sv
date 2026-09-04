// pcie_template_accel.sv
//
// DUT for AXION's PCIe worked example: a deliberately minimal "does it
// work at all" endpoint application. Like every real FPGA PCIe design
// built on a hard IP block (Xilinx PCIe-to-AXI Bridge / XDMA, Intel
// Avalon/AXI bridge), this RTL knows *nothing* about PCIe itself -- the
// hard block terminates the link, config space and BARs, and hands the
// user logic two ordinary AXI4 ports:
//
//   s_axi  -- the BAR0 window, DUT as AXI4 slave. Addresses arriving here
//             are already BAR-relative offsets; gem5's RTLPciDevice does
//             that translation with PciDevice::getBAR(), exactly as the
//             hard block does in silicon.
//   m_axi  -- bus-master DMA, DUT as AXI4 master, reaching host memory.
//
// plus two sideband outputs: irq_o (drives an INTx assert/deassert
// through gem5's PCI host into the PLIC) and busy_o (feeds
// RTLPciDevice::isIdle()'s clock gating).
//
// BAR0 window (64 KiB).
//
// WHY EVERY REGISTER SITS ON AN 8-BYTE STRIDE even though most hold only
// 32 bits: AXION's Axi4SlaveEngine (src/axi/axi4_slave_engine.cc) is
// LSB-aligned, not byte-lane-aligned -- an n-byte access always travels
// in wdata/rdata bits [n*8-1:0] with strobe (1<<n)-1, whatever the
// address's offset within the 8-byte beat. A strict AXI4 requester (the
// cocotb testbench's AxiMaster, and real hardware) instead places a
// 4-byte access at offset 4 on byte lanes 4..7. The two conventions only
// agree when every access starts at a beat boundary -- so keeping the
// register file on an 8-byte stride makes this DUT behave identically
// under gem5 and under cocotb, with no lane-shifting logic and no
// convention baked into the register decode. fifo_pio_accel.sv (regs at
// 0x00/0x08) and dma_memcopy.sv (0x20 stride) already do the same thing.
// The RAM region is different: it implements true per-lane WSTRB
// semantics, so sub-beat writes there are correct under either requester.
//
//   0x0000 ID       RO 32b  0x50434945 ("PCIE") -- proves enumeration + BAR mapping
//   0x0008 VERSION  RO 32b  0x00010000
//   0x0010 SCRATCH  RW 32b  read/write round-trip
//   0x0018 COUNTER  RO 32b  free-running cycle counter -- proves the RTL clock ticks
//   0x0020 DMA_SRC  RW 64b  host-memory source address
//   0x0028 DMA_DST  RW 64b  host-memory destination address
//   0x0030 DMA_LEN  RW 32b  bytes to copy
//   0x0038 CTRL     RW 32b  [0]=START (pulse) [1]=IRQ_EN [2]=IRQ_FORCE (pulse)
//   0x0040 STATUS   RO 32b  [0]=BUSY [1]=DONE [2]=IRQ_PENDING
//   0x0048 IRQ_ACK  WO 32b  write 1 to bit[0]: clears IRQ_PENDING and DONE
//   0x1000..0x1FFF  RW      4 KiB scratchpad RAM -- multi-beat bursts + byte strobes
//
// The DMA engine copies DMA_LEN bytes SRC->DST as a sequence of
// single-beat 8-byte read/write pairs, tagging reads with AXI ID 0 and
// writes with AXI ID 1 so both of Axi4MasterEngine's per-ID queues are
// genuinely exercised. A trailing partial word (DMA_LEN not a multiple of
// 8) is written with a masked WSTRB.

module pcie_template_accel #(
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT,
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT,
  parameter int unsigned RAM_WORDS  = 512   // 512 * 8 B = 4 KiB
) (
  input  logic   clk_i,
  input  logic   rst_ni,
  axi4_if.slave  s_axi,    // BAR0 window
  axi4_if.master m_axi,    // bus-master DMA
  output logic   irq_o,
  output logic   busy_o
);
  import axi4_pkg::*;

  localparam int unsigned BEAT_BYTES = DATA_WIDTH / 8;
  localparam axi_size_t   BEAT_SIZE  = axi_size_t'($clog2(BEAT_BYTES));
  localparam int unsigned RAM_IDX_W  = $clog2(RAM_WORDS);

  localparam logic [31:0] ID_MAGIC   = 32'h5043_4945;  // "PCIE"
  localparam logic [31:0] ID_VERSION = 32'h0001_0000;

  // Region select: BAR0 offset bits [15:12]. 0 = registers, 1 = RAM.
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

  // ---------------------------------------------------------------
  // Register / RAM state.
  //
  // Ownership is split strictly by always_ff block so no signal has two
  // drivers (same discipline as dma_memcopy.sv): the write-channel block
  // owns scratch/dma_src/dma_dst/dma_len/irq_en and the RAM; the DMA
  // block owns everything about a transfer in flight plus irq_pending.
  // The write block asks the DMA block for things via the combinational
  // pulses below rather than by writing its state directly.
  // ---------------------------------------------------------------
  logic [31:0] scratch_q;
  logic [31:0] counter_q;
  logic [63:0] dma_src_q;
  logic [63:0] dma_dst_q;
  logic [31:0] dma_len_q;
  logic        irq_en_q;

  logic [DATA_WIDTH-1:0] ram [RAM_WORDS];

  // DMA-block-owned
  typedef enum logic [2:0] {
    D_IDLE, D_AR, D_WAIT_R, D_AW, D_WAIT_B
  } dma_state_t;

  dma_state_t            dma_state;
  logic [63:0]           dma_cur_src;
  logic [63:0]           dma_cur_dst;
  logic [31:0]           dma_rem;        // bytes left to copy
  logic [DATA_WIDTH-1:0] dma_buf;
  logic                  dma_done_q;
  logic                  irq_pending_q;

  assign busy_o = (dma_state != D_IDLE);
  assign irq_o  = irq_pending_q & irq_en_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) counter_q <= '0;
    else         counter_q <= counter_q + 32'd1;
  end

  // ---------------------------------------------------------------
  // Write channel: AW -> W(n beats) -> B. Honors awlen: wr_addr walks
  // forward one beat (BEAT_BYTES) per W handshake, which is what makes
  // the RAM region usable with real multi-beat INCR bursts.
  // ---------------------------------------------------------------
  typedef enum logic [1:0] { W_IDLE, W_DATA, W_RESP } wr_state_t;
  wr_state_t             wr_state;
  logic [ADDR_WIDTH-1:0] wr_addr;
  logic [7:0]            wr_beats_left;
  logic [ID_WIDTH-1:0]   awid_q;

  // Combinational "this beat is a register write to <offset>" pulses,
  // consumed by the DMA block. Valid only during the W handshake cycle.
  logic reg_wr_en;
  logic dma_start_pulse;
  logic irq_force_pulse;
  logic irq_ack_pulse;

  assign reg_wr_en = (wr_state == W_DATA) && s_axi.wvalid && s_axi.wready &&
                     (wr_addr[15:12] == REGION_REGS) && s_axi.wstrb[0];
  assign dma_start_pulse = reg_wr_en && (wr_addr[7:0] == REG_CTRL) &&
                           s_axi.wdata[0];
  assign irq_force_pulse = reg_wr_en && (wr_addr[7:0] == REG_CTRL) &&
                           s_axi.wdata[2];
  assign irq_ack_pulse   = reg_wr_en && (wr_addr[7:0] == REG_IRQ_ACK) &&
                           s_axi.wdata[0];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      wr_state      <= W_IDLE;
      wr_addr       <= '0;
      wr_beats_left <= '0;
      awid_q        <= '0;
      s_axi.awready <= 1'b1;
      s_axi.wready  <= 1'b0;
      s_axi.bvalid  <= 1'b0;
      s_axi.bresp   <= AXI_RESP_OKAY;
      s_axi.bid     <= '0;
      scratch_q     <= '0;
      dma_src_q     <= '0;
      dma_dst_q     <= '0;
      dma_len_q     <= '0;
      irq_en_q      <= 1'b0;
    end else begin
      unique case (wr_state)
        W_IDLE: begin
          s_axi.bvalid <= 1'b0;
          if (s_axi.awvalid && s_axi.awready) begin
            wr_addr       <= s_axi.awaddr;
            wr_beats_left <= s_axi.awlen;
            awid_q        <= s_axi.awid;
            s_axi.awready <= 1'b0;
            s_axi.wready  <= 1'b1;
            wr_state      <= W_DATA;
          end
        end

        W_DATA: begin
          if (s_axi.wvalid && s_axi.wready) begin
            if (wr_addr[15:12] == REGION_RAM) begin
              for (int b = 0; b < BEAT_BYTES; b++)
                if (s_axi.wstrb[b])
                  ram[wr_addr[RAM_IDX_W+2:3]][b*8 +: 8] <=
                      s_axi.wdata[b*8 +: 8];
            end else if (s_axi.wstrb[0]) begin
              unique case (wr_addr[7:0])
                REG_SCRATCH: scratch_q <= s_axi.wdata[31:0];
                REG_DMA_SRC: dma_src_q <= 64'(s_axi.wdata);
                REG_DMA_DST: dma_dst_q <= 64'(s_axi.wdata);
                REG_DMA_LEN: dma_len_q <= s_axi.wdata[31:0];
                REG_CTRL:    irq_en_q  <= s_axi.wdata[1];
                default: ; // ID/VERSION/COUNTER/STATUS are read-only;
                           // IRQ_ACK is handled by the DMA block's pulse
              endcase
            end

            if (wr_beats_left == 8'd0) begin
              s_axi.wready <= 1'b0;
              s_axi.bid    <= awid_q;
              s_axi.bresp  <= AXI_RESP_OKAY;
              s_axi.bvalid <= 1'b1;
              wr_state     <= W_RESP;
            end else begin
              wr_addr       <= wr_addr + ADDR_WIDTH'(BEAT_BYTES);
              wr_beats_left <= wr_beats_left - 8'd1;
            end
          end
        end

        W_RESP: begin
          if (s_axi.bvalid && s_axi.bready) begin
            s_axi.bvalid  <= 1'b0;
            s_axi.awready <= 1'b1;
            wr_state      <= W_IDLE;
          end
        end

        default: wr_state <= W_IDLE;
      endcase
    end
  end

  // ---------------------------------------------------------------
  // Read channel: AR -> R(n beats). rdata is driven combinationally off
  // the registered per-beat address rd_addr, so it is always valid
  // whenever rvalid is high, with no extra pipeline stage to get wrong.
  // ---------------------------------------------------------------
  typedef enum logic { R_IDLE, R_DATA } rd_state_t;
  rd_state_t             rd_state;
  logic [ADDR_WIDTH-1:0] rd_addr;
  logic [7:0]            rd_beats_left;
  logic [DATA_WIDTH-1:0] rd_data_mux;

  always_comb begin
    rd_data_mux = '0;
    if (rd_addr[15:12] == REGION_RAM) begin
      rd_data_mux = ram[rd_addr[RAM_IDX_W+2:3]];
    end else begin
      unique case (rd_addr[7:0])
        REG_ID:      rd_data_mux = DATA_WIDTH'(ID_MAGIC);
        REG_VERSION: rd_data_mux = DATA_WIDTH'(ID_VERSION);
        REG_SCRATCH: rd_data_mux = DATA_WIDTH'(scratch_q);
        REG_COUNTER: rd_data_mux = DATA_WIDTH'(counter_q);
        REG_DMA_SRC: rd_data_mux = DATA_WIDTH'(dma_src_q);
        REG_DMA_DST: rd_data_mux = DATA_WIDTH'(dma_dst_q);
        REG_DMA_LEN: rd_data_mux = DATA_WIDTH'(dma_len_q);
        REG_CTRL:    rd_data_mux = DATA_WIDTH'({30'd0, irq_en_q, 1'b0});
        REG_STATUS:  rd_data_mux = DATA_WIDTH'({29'd0, irq_pending_q,
                                                 dma_done_q, busy_o});
        default:     rd_data_mux = '0;
      endcase
    end
  end

  assign s_axi.rdata = rd_data_mux;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      rd_state      <= R_IDLE;
      rd_addr       <= '0;
      rd_beats_left <= '0;
      s_axi.arready <= 1'b1;
      s_axi.rvalid  <= 1'b0;
      s_axi.rlast   <= 1'b0;
      s_axi.rresp   <= AXI_RESP_OKAY;
      s_axi.rid     <= '0;
    end else begin
      unique case (rd_state)
        R_IDLE: begin
          if (s_axi.arvalid && s_axi.arready) begin
            rd_addr       <= s_axi.araddr;
            rd_beats_left <= s_axi.arlen;
            s_axi.rid     <= s_axi.arid;
            s_axi.rresp   <= AXI_RESP_OKAY;
            s_axi.rlast   <= (s_axi.arlen == 8'd0);
            s_axi.arready <= 1'b0;
            s_axi.rvalid  <= 1'b1;
            rd_state      <= R_DATA;
          end
        end

        R_DATA: begin
          if (s_axi.rvalid && s_axi.rready) begin
            if (rd_beats_left == 8'd0) begin
              s_axi.rvalid  <= 1'b0;
              s_axi.rlast   <= 1'b0;
              s_axi.arready <= 1'b1;
              rd_state      <= R_IDLE;
            end else begin
              rd_addr       <= rd_addr + ADDR_WIDTH'(BEAT_BYTES);
              rd_beats_left <= rd_beats_left - 8'd1;
              s_axi.rlast   <= (rd_beats_left == 8'd1);
            end
          end
        end

        default: rd_state <= R_IDLE;
      endcase
    end
  end

  // ---------------------------------------------------------------
  // Bus-master DMA: DMA_LEN bytes SRC->DST as single-beat 8-byte
  // read/write pairs. Reads carry AXI ID 0, writes AXI ID 1 -- two
  // distinct IDs, so Axi4MasterEngine's per-ID queues and its
  // same-ID-in-order/cross-ID-out-of-order pick logic are both on the
  // path rather than being trivially bypassed by a single-ID stream.
  // A trailing partial word is masked off with WSTRB.
  // ---------------------------------------------------------------
  localparam logic [ID_WIDTH-1:0] DMA_RD_ID = ID_WIDTH'(0);
  localparam logic [ID_WIDTH-1:0] DMA_WR_ID = ID_WIDTH'(1);

  logic [31:0] dma_this_beat;   // bytes moved by the beat in flight
  assign dma_this_beat = (dma_rem > 32'(BEAT_BYTES)) ? 32'(BEAT_BYTES)
                                                     : dma_rem;

  assign m_axi.arvalid  = (dma_state == D_AR);
  assign m_axi.arid     = DMA_RD_ID;
  assign m_axi.araddr   = ADDR_WIDTH'(dma_cur_src);
  assign m_axi.arlen    = '0;
  assign m_axi.arsize   = BEAT_SIZE;
  assign m_axi.arburst  = AXI_BURST_INCR;
  assign m_axi.arlock   = '0;
  assign m_axi.arcache  = '0;
  assign m_axi.arprot   = '0;
  assign m_axi.arqos    = '0;
  assign m_axi.arregion = '0;
  assign m_axi.rready   = 1'b1;

  assign m_axi.awvalid  = (dma_state == D_AW);
  assign m_axi.awid     = DMA_WR_ID;
  assign m_axi.awaddr   = ADDR_WIDTH'(dma_cur_dst);
  assign m_axi.awlen    = '0;
  assign m_axi.awsize   = BEAT_SIZE;
  assign m_axi.awburst  = AXI_BURST_INCR;
  assign m_axi.awlock   = '0;
  assign m_axi.awcache  = '0;
  assign m_axi.awprot   = '0;
  assign m_axi.awqos    = '0;
  assign m_axi.awregion = '0;
  assign m_axi.wdata    = dma_buf;
  assign m_axi.wstrb    = (dma_this_beat >= 32'(BEAT_BYTES))
                            ? {BEAT_BYTES{1'b1}}
                            : BEAT_BYTES'((1 << dma_this_beat) - 1);
  assign m_axi.wlast    = 1'b1;
  assign m_axi.wvalid   = (dma_state == D_AW);
  assign m_axi.bready   = 1'b1;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      dma_state     <= D_IDLE;
      dma_cur_src   <= '0;
      dma_cur_dst   <= '0;
      dma_rem       <= '0;
      dma_buf       <= '0;
      dma_done_q    <= 1'b0;
      irq_pending_q <= 1'b0;
    end else begin
      // Software-driven side effects, independent of the transfer FSM.
      if (irq_ack_pulse) begin
        irq_pending_q <= 1'b0;
        dma_done_q    <= 1'b0;
      end
      if (irq_force_pulse)
        irq_pending_q <= 1'b1;

      unique case (dma_state)
        D_IDLE: begin
          if (dma_start_pulse && dma_len_q != 32'd0) begin
            dma_cur_src <= dma_src_q;
            dma_cur_dst <= dma_dst_q;
            dma_rem     <= dma_len_q;
            dma_done_q  <= 1'b0;
            dma_state   <= D_AR;
          end
        end

        D_AR: if (m_axi.arready) dma_state <= D_WAIT_R;

        D_WAIT_R: begin
          if (m_axi.rvalid && m_axi.rready) begin
            dma_buf   <= m_axi.rdata;
            dma_state <= D_AW;
          end
        end

        D_AW: if (m_axi.awready && m_axi.wready) dma_state <= D_WAIT_B;

        D_WAIT_B: begin
          if (m_axi.bvalid && m_axi.bready) begin
            if (dma_rem <= 32'(BEAT_BYTES)) begin
              dma_rem       <= '0;
              dma_done_q    <= 1'b1;
              irq_pending_q <= 1'b1;
              dma_state     <= D_IDLE;
            end else begin
              dma_cur_src <= dma_cur_src + 64'(BEAT_BYTES);
              dma_cur_dst <= dma_cur_dst + 64'(BEAT_BYTES);
              dma_rem     <= dma_rem - 32'(BEAT_BYTES);
              dma_state   <= D_AR;
            end
          end
        end

        default: dma_state <= D_IDLE;
      endcase
    end
  end

endmodule
