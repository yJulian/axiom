// dma_memcopy.sv
//
// DUT for the AXION DMA worked example: NUM_CH independent copy channels,
// each started via a control/status AXI4 slave port and each performing
// one src->dst memcpy (single AXI4 beat, DATA_WIDTH bytes) over a shared
// AXI4 master (DMA) port. Every channel uses its own channel index as its
// AXI ID (AWID/ARID), so with more than one channel started concurrently
// the DUT genuinely has multiple outstanding read/write transactions in
// flight at once, tagged by distinct IDs -- this is what lets
// Axi4MasterEngine's per-ID-in-order/cross-ID-out-of-order completion
// logic (src/axi/axi4_master_engine.cc) actually get exercised end to end
// against real RTL, which no other worked example in this repo does (the
// PIO-only fifo_pio_accel has no DMA master port at all).
//
// AR/AW issuance among channels wanting to start a burst is arbitrated
// fixed-priority (lowest channel index wins), one grant per cycle -- that
// only serializes *issuing* a burst; multiple channels can still have
// bursts outstanding simultaneously (each waiting on its own R/B
// response), which is the actual property being exercised here. R/B
// responses are demultiplexed back to the owning channel purely by ID,
// since a channel's AXI ID always equals its own channel index.
//
// Control/status register map (per channel, NUM_CH channels, 0x20-byte
// stride starting at 0x00):
//   +0x00 SRC    (RW) source address
//   +0x08 DST    (RW) destination address
//   +0x10 CTRL   (WO) write bit[0]=1 to start (ignored while busy)
//   +0x18 STATUS (RO) bit[0]=busy, bit[1]=done (reading clears done)

module dma_memcopy #(
  parameter int unsigned NUM_CH     = 4,
  parameter int unsigned DATA_WIDTH = axi4_pkg::AXI_DATA_WIDTH_DEFAULT,
  parameter int unsigned ADDR_WIDTH = axi4_pkg::AXI_ADDR_WIDTH_DEFAULT,
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT
) (
  input  logic   clk_i,
  input  logic   rst_ni,
  axi4_if.slave  s_axi,   // control/status register port
  axi4_if.master m_axi    // DMA port
);
  import axi4_pkg::*;

  localparam int unsigned CH_WIDTH   = (NUM_CH > 1) ? $clog2(NUM_CH) : 1;
  localparam int unsigned BEAT_BYTES = DATA_WIDTH / 8;
  localparam axi_size_t   BEAT_SIZE  = axi_size_t'($clog2(BEAT_BYTES));

  typedef enum logic [2:0] {
    CH_IDLE, CH_WANT_AR, CH_WAIT_R, CH_WANT_AW, CH_WAIT_B
  } ch_state_t;

  ch_state_t             ch_state [NUM_CH];
  logic [ADDR_WIDTH-1:0] ch_src   [NUM_CH];
  logic [ADDR_WIDTH-1:0] ch_dst   [NUM_CH];
  logic [DATA_WIDTH-1:0] ch_buf   [NUM_CH];
  logic                   ch_busy [NUM_CH];
  logic                   ch_done [NUM_CH];

  // -------------------------------------------------------------
  // Control/status slave port: single-beat register reads/writes,
  // same AW->W->B / AR->R shape as fifo_pio_accel.sv. Owns ch_src/ch_dst
  // outright; ch_busy/ch_done/ch_state/ch_buf belong to the channel FSM
  // block below (single-writer per signal) -- this block only *requests*
  // a start/status-clear via the purely-combinational pulses declared
  // just below it, so the two always_ff blocks never race the same
  // signal.
  // -------------------------------------------------------------
  typedef enum logic [1:0] { W_IDLE, W_DATA, W_RESP } wr_state_t;
  wr_state_t            wr_state;
  logic [7:0]            awaddr_q;
  logic [ID_WIDTH-1:0]   awid_q;

  logic ch_start_pulse [NUM_CH];
  logic status_read_pulse [NUM_CH];

  always_comb begin
    for (int i = 0; i < NUM_CH; i++) begin
      ch_start_pulse[i] =
          (wr_state == W_DATA) && s_axi.wvalid && s_axi.wready &&
          (awaddr_q == ((8'(i) * 8'h20) | 8'h10)) && s_axi.wdata[0];
      status_read_pulse[i] =
          (rd_state == R_IDLE) && s_axi.arvalid && s_axi.arready &&
          (s_axi.araddr[7:0] == ((8'(i) * 8'h20) | 8'h18));
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      wr_state      <= W_IDLE;
      s_axi.awready <= 1'b1;
      s_axi.wready  <= 1'b0;
      s_axi.bvalid  <= 1'b0;
      s_axi.bresp   <= AXI_RESP_OKAY;
      s_axi.bid     <= '0;
      awaddr_q      <= '0;
      awid_q        <= '0;
      for (int i = 0; i < NUM_CH; i++) begin
        ch_src[i] <= '0;
        ch_dst[i] <= '0;
      end
    end else begin
      unique case (wr_state)
        W_IDLE: begin
          s_axi.bvalid <= 1'b0;
          if (s_axi.awvalid && s_axi.awready) begin
            awaddr_q      <= s_axi.awaddr[7:0];
            awid_q        <= s_axi.awid;
            s_axi.awready <= 1'b0;
            s_axi.wready  <= 1'b1;
            wr_state      <= W_DATA;
          end
        end

        W_DATA: begin
          if (s_axi.wvalid && s_axi.wready) begin
            s_axi.wready <= 1'b0;
            for (int i = 0; i < NUM_CH; i++) begin
              if (awaddr_q == ((8'(i) * 8'h20) | 8'h00))
                ch_src[i] <= ADDR_WIDTH'(s_axi.wdata);
              else if (awaddr_q == ((8'(i) * 8'h20) | 8'h08))
                ch_dst[i] <= ADDR_WIDTH'(s_axi.wdata);
            end
            s_axi.bid    <= awid_q;
            s_axi.bresp  <= AXI_RESP_OKAY;
            s_axi.bvalid <= 1'b1;
            wr_state     <= W_RESP;
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

  // -------------------------------------------------------------
  // Control/status read path: AR -> R (single beat). SRC/DST read back
  // whatever was last written; STATUS reads {done, busy}.
  // -------------------------------------------------------------
  typedef enum logic { R_IDLE, R_DATA } rd_state_t;
  rd_state_t rd_state;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      rd_state      <= R_IDLE;
      s_axi.arready <= 1'b1;
      s_axi.rvalid  <= 1'b0;
      s_axi.rlast   <= 1'b1; // every response here is single-beat
      s_axi.rresp   <= AXI_RESP_OKAY;
      s_axi.rid     <= '0;
      s_axi.rdata   <= '0;
    end else begin
      unique case (rd_state)
        R_IDLE: begin
          if (s_axi.arvalid && s_axi.arready) begin
            s_axi.arready <= 1'b0;
            s_axi.rdata   <= '0;
            for (int i = 0; i < NUM_CH; i++) begin
              if (s_axi.araddr[7:0] == ((8'(i) * 8'h20) | 8'h00))
                s_axi.rdata <= DATA_WIDTH'(ch_src[i]);
              else if (s_axi.araddr[7:0] == ((8'(i) * 8'h20) | 8'h08))
                s_axi.rdata <= DATA_WIDTH'(ch_dst[i]);
              else if (s_axi.araddr[7:0] == ((8'(i) * 8'h20) | 8'h18))
                s_axi.rdata <= DATA_WIDTH'({ch_done[i], ch_busy[i]});
            end
            s_axi.rid    <= s_axi.arid;
            s_axi.rresp  <= AXI_RESP_OKAY;
            s_axi.rvalid <= 1'b1;
            rd_state     <= R_DATA;
          end
        end

        R_DATA: begin
          if (s_axi.rvalid && s_axi.rready) begin
            s_axi.rvalid  <= 1'b0;
            s_axi.arready <= 1'b1;
            rd_state      <= R_IDLE;
          end
        end

        default: rd_state <= R_IDLE;
      endcase
    end
  end

  // -------------------------------------------------------------
  // Per-channel copy FSM + DMA master port. Sole writer of ch_state,
  // ch_busy, ch_done and ch_buf. AR/AW issuance is fixed-priority
  // (lowest channel index) among channels currently wanting one; R/B
  // responses are routed back to their owning channel purely by ID
  // (m_axi.rid/m_axi.bid), since a channel's own index is its AXI ID.
  // -------------------------------------------------------------
  logic [CH_WIDTH-1:0] ar_pick, aw_pick;
  logic                ar_pick_valid, aw_pick_valid;

  always_comb begin
    ar_pick_valid = 1'b0;
    ar_pick       = '0;
    aw_pick_valid = 1'b0;
    aw_pick       = '0;
    for (int i = NUM_CH - 1; i >= 0; i--) begin
      if (ch_state[i] == CH_WANT_AR) begin
        ar_pick       = CH_WIDTH'(i);
        ar_pick_valid = 1'b1;
      end
      if (ch_state[i] == CH_WANT_AW) begin
        aw_pick       = CH_WIDTH'(i);
        aw_pick_valid = 1'b1;
      end
    end
  end

  assign m_axi.arvalid  = ar_pick_valid;
  assign m_axi.arid     = ID_WIDTH'(ar_pick);
  assign m_axi.araddr   = ar_pick_valid ? ch_src[ar_pick] : '0;
  assign m_axi.arlen    = '0;
  assign m_axi.arsize   = BEAT_SIZE;
  assign m_axi.arburst  = AXI_BURST_INCR;
  assign m_axi.arlock   = '0;
  assign m_axi.arcache  = '0;
  assign m_axi.arprot   = '0;
  assign m_axi.arqos    = '0;
  assign m_axi.arregion = '0;
  assign m_axi.rready   = 1'b1;

  assign m_axi.awvalid  = aw_pick_valid;
  assign m_axi.awid     = ID_WIDTH'(aw_pick);
  assign m_axi.awaddr   = aw_pick_valid ? ch_dst[aw_pick] : '0;
  assign m_axi.awlen    = '0;
  assign m_axi.awsize   = BEAT_SIZE;
  assign m_axi.awburst  = AXI_BURST_INCR;
  assign m_axi.awlock   = '0;
  assign m_axi.awcache  = '0;
  assign m_axi.awprot   = '0;
  assign m_axi.awqos    = '0;
  assign m_axi.awregion = '0;
  assign m_axi.wdata    = aw_pick_valid ? ch_buf[aw_pick] : '0;
  assign m_axi.wstrb    = {(DATA_WIDTH / 8){1'b1}};
  assign m_axi.wlast    = 1'b1;
  assign m_axi.wvalid   = aw_pick_valid;
  assign m_axi.bready   = 1'b1;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      for (int i = 0; i < NUM_CH; i++) begin
        ch_state[i] <= CH_IDLE;
        ch_busy[i]  <= 1'b0;
        ch_done[i]  <= 1'b0;
        ch_buf[i]   <= '0;
      end
    end else begin
      for (int i = 0; i < NUM_CH; i++) begin
        if (ch_start_pulse[i] && ch_state[i] == CH_IDLE) begin
          ch_state[i] <= CH_WANT_AR;
          ch_busy[i]  <= 1'b1;
        end
        if (status_read_pulse[i])
          ch_done[i] <= 1'b0;
      end

      if (ar_pick_valid && m_axi.arready)
        ch_state[ar_pick] <= CH_WAIT_R;

      if (aw_pick_valid && m_axi.awready && m_axi.wready)
        ch_state[aw_pick] <= CH_WAIT_B;

      // Channels only ever use IDs [0, NUM_CH) (their own index), so the
      // low CH_WIDTH bits of the echoed-back RID/BID are all that's ever
      // meaningful here.
      if (m_axi.rvalid && m_axi.rready &&
          ch_state[m_axi.rid[CH_WIDTH-1:0]] == CH_WAIT_R) begin
        ch_buf[m_axi.rid[CH_WIDTH-1:0]]   <= m_axi.rdata;
        ch_state[m_axi.rid[CH_WIDTH-1:0]] <= CH_WANT_AW;
      end

      if (m_axi.bvalid && m_axi.bready &&
          ch_state[m_axi.bid[CH_WIDTH-1:0]] == CH_WAIT_B) begin
        ch_state[m_axi.bid[CH_WIDTH-1:0]] <= CH_IDLE;
        ch_busy[m_axi.bid[CH_WIDTH-1:0]]  <= 1'b0;
        ch_done[m_axi.bid[CH_WIDTH-1:0]]  <= 1'b1;
      end
    end
  end

endmodule
