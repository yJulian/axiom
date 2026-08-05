// fifo_pio_accel.sv
//
// DUT (Design Under Test) for the AXION worked example: an N-deep FIFO
// queue exposed purely as a PIO/MMIO register interface over full AXI4
// (no DMA master port -- that gets exercised once an RTLDmaDevice
// consumer exists). Adapted from gem5_cva6's accelerator/fifo_accel.sv,
// with the AXI4 master/DMA half dropped and the register interface
// upgraded from AXI4-Lite to full AXI4 (still single-beat per burst --
// this particular DUT doesn't itself need multi-beat bursts, but the
// axi4_if/axi4_pins plumbing around it supports them).
//
// Register map (8-byte aligned, DATA_WIDTH-wide):
//   0x00 STATUS    (RO) [0]=empty [1]=full [N_BITS+1:2]=count
//   0x08 FIFO_DATA (RW) write = push, read = pop

module fifo_pio_accel #(
  parameter int unsigned N          = 4,
  parameter int unsigned DATA_WIDTH = 64,
  parameter int unsigned ID_WIDTH   = axi4_pkg::AXI_ID_WIDTH_DEFAULT
) (
  input  logic  clk_i,
  input  logic  rst_ni,
  axi4_if.slave s_axi
);
  import axi4_pkg::*;

  localparam int unsigned PTR_WIDTH = (N > 1) ? $clog2(N) : 1;

  // -------------------------------------------------------------
  // FIFO storage
  // -------------------------------------------------------------
  logic [DATA_WIDTH-1:0]  mem [N-1:0];
  logic [PTR_WIDTH-1:0]   wr_ptr, rd_ptr;
  logic [PTR_WIDTH:0]     count;
  logic                    full, empty;

  assign full  = (count == (PTR_WIDTH + 1)'(N));
  assign empty = (count == '0);

  logic                   push, pop;
  logic [DATA_WIDTH-1:0]  push_data;
  logic [DATA_WIDTH-1:0]  pop_data;

  assign pop_data = mem[rd_ptr];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      wr_ptr <= '0;
      rd_ptr <= '0;
      count  <= '0;
    end else begin
      if (push && !full) begin
        mem[wr_ptr] <= push_data;
        wr_ptr <= (wr_ptr == PTR_WIDTH'(N - 1)) ? '0 : wr_ptr + 1'b1;
      end
      if (pop && !empty) begin
        rd_ptr <= (rd_ptr == PTR_WIDTH'(N - 1)) ? '0 : rd_ptr + 1'b1;
      end
      case ({push && !full, pop && !empty})
        2'b10: count <= count + 1'b1;
        2'b01: count <= count - 1'b1;
        default: ; // both (straight through) or neither: unchanged
      endcase
    end
  end

  // -------------------------------------------------------------
  // AXI4 write path: AW -> W (single beat) -> B
  // -------------------------------------------------------------
  typedef enum logic [1:0] { W_IDLE, W_DATA, W_RESP } wr_state_t;
  wr_state_t wr_state;
  logic [7:0]           awaddr_q; // only the register-offset byte is needed
  logic [ID_WIDTH-1:0]  awid_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      wr_state       <= W_IDLE;
      s_axi.awready  <= 1'b1;
      s_axi.wready   <= 1'b0;
      s_axi.bvalid   <= 1'b0;
      s_axi.bresp    <= AXI_RESP_OKAY;
      s_axi.bid      <= '0;
      awaddr_q       <= '0;
      awid_q         <= '0;
      push           <= 1'b0;
      push_data      <= '0;
    end else begin
      push <= 1'b0;

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
            if (awaddr_q == 8'h08) begin
              push      <= 1'b1;
              push_data <= s_axi.wdata;
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
  // AXI4 read path: AR -> R (single beat)
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
      pop           <= 1'b0;
    end else begin
      pop <= 1'b0;

      unique case (rd_state)
        R_IDLE: begin
          if (s_axi.arvalid && s_axi.arready) begin
            s_axi.arready <= 1'b0;

            unique case (s_axi.araddr[7:0])
              8'h00: s_axi.rdata <= {{(DATA_WIDTH - PTR_WIDTH - 3){1'b0}},
                                      count, full, empty};
              8'h08: begin
                s_axi.rdata <= pop_data;
                pop         <= 1'b1;
              end
              default: s_axi.rdata <= '0;
            endcase

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

endmodule
