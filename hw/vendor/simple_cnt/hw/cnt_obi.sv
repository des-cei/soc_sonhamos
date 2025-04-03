// Copyright 2024 Politecnico di Torino.
// Copyright and related rights are licensed under the Solderpad Hardware
// License, Version 2.0 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License at
// http://solderpad.org/licenses/SHL-2.0. Unless required by applicable law
// or agreed to in writing, software, hardware and materials distributed under
// this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// File: cnt_obi.sv
// Author(s):
//   Michele Caon
// Date: 07/11/2024
// Description: OBI bus wrapper for the simple counter

module cnt_obi #(
  parameter int unsigned W = 32  // counter bitwidth (max: 32)
) (
  input logic clk_i,
  input logic rst_ni,

  // OBI interface (counter value)
  input  cnt_obi_pkg::obi_req_t  obi_req_i,
  output cnt_obi_pkg::obi_resp_t obi_rsp_o,

  // Register Interface (configuration registers)
  input  cnt_reg_pkg::reg_req_t  reg_req_i,
  output cnt_reg_pkg::reg_resp_t reg_rsp_o,

  // Terminal count interrupt
  output logic tc_int_o  // interrupt to host system
);
  // Address mask for the OBI counter value request
  localparam OBI_ADDR_MASK = 32'h0000_0001;

  // INTERNAL SIGNALS
  // ----------------
  // Bus request and response
  logic         obi_gnt;
  logic         obi_rvalid_q;
  logic [W-1:0] obi_rdata_q;

  // Registers <--> Hanrdware counter
  logic         cnt_en;
  logic         cnt_clr;
  logic         cnt_ld;
  logic [W-1:0] cnt_ld_val;
  logic [W-1:0] cnt_val;
  logic [ 31:0] cnt_thr;
  logic         cnt_tc;

  // --------------
  // COUNTER MODULE
  // --------------
  // Counter instance
  cnt #(
    .W(W)
  ) u_cnt (
    .clk_i   (clk_i),
    .rst_ni  (rst_ni),
    .en_i    (cnt_en),
    .clr_i   (cnt_clr),
    .ld_i    (cnt_ld),
    .ld_val_i(cnt_ld_val),
    .thr_i   (cnt_thr[W-1:0]),
    .cnt_o   (cnt_val),
    .tc_o    (cnt_tc)
  );

  // Interrupt to host system
  assign tc_int_o = cnt_tc;

  // OBI bridge to counter value
  // ---------------------------
  // Bus write request logic
  assign cnt_ld     = obi_req_i.req & obi_req_i.we & (&obi_req_i.be) & ~(|(obi_req_i.addr & OBI_ADDR_MASK));
  assign cnt_ld_val = obi_req_i.wdata[W-1:0];

  // Bus response logic
  assign obi_gnt = obi_req_i.req & ~cnt_clr;  // accept a load request if not being cleared
  always_ff @(posedge clk_i or negedge rst_ni) begin : rvalid_ff
    if (!rst_ni) begin
      obi_rvalid_q <= 1'b0;
      obi_rdata_q  <= '0;
    end else begin
      obi_rvalid_q <= obi_gnt;  // always one cycle after the request transaction
      obi_rdata_q  <= cnt_val;  // always one cycle after the request transaction
    end
  end

  // Bus signals
  assign obi_rsp_o = '{gnt: obi_gnt, rvalid: obi_rvalid_q, rdata: {{32 - W{1'b0}}, obi_rdata_q}};

  // -----------------
  // CONTROL REGISTERS
  // -----------------
  // Control registers
  cnt_control_reg u_cnt_control_reg (
    .clk_i    (clk_i),
    .rst_ni   (rst_ni),
    .req_i    (reg_req_i),
    .rsp_o    (reg_rsp_o),
    .cnt_tc_i (cnt_tc),
    .cnt_en_o (cnt_en),
    .cnt_clr_o(cnt_clr),
    .cnt_thr_o(cnt_thr)
  );

  // ----------
  // ASSERTIONS
  // ----------
`ifndef SYNTHESIS
  initial begin
    assert (W > 0 && W <= 32)
    else $error("Counter width must be in [1,32]");
  end
`endif  /* SYNTHESIS */
endmodule
