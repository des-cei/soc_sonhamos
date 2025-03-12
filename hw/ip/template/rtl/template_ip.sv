// Copyright 2024 CEI-UPM
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// Daniel Vazquez (daniel.vazquez@upm.es)

module template_ip
  import reg_pkg::*;
  import obi_pkg::*;
#(
    parameter int unsigned NumWords = 32'd1024,  // Number of Words in data array
    parameter int unsigned DataWidth = 32'd32,  // Data signal width
    // DEPENDENT PARAMETERS, DO NOT OVERWRITE!
    parameter int unsigned AddrWidth = (NumWords > 32'd1) ? $clog2(NumWords) : 32'd1
) (
    // Clock and reset
    input logic clk_i,
    input logic rst_ni,

    // MMIO interface
    input  reg_req_t reg_req_i,
    output reg_rsp_t reg_rsp_o,

    // Slave interface input ports
    input  obi_req_t  slave_req_i,
    output obi_resp_t slave_resp_o
);

  logic [31:0] din_0, din_1, result;

  logic [AddrWidth-1:0] mem_addr;
  logic [         31:0] mem_wdata;
  logic                 rvalid_q;


  assign mem_addr = slave_req_i.addr[AddrWidth+1:2];
  assign mem_wdata = slave_req_i.wdata * result;
  assign slave_resp_o.gnt = slave_req_i.req;
  assign slave_resp_o.rvalid = rvalid_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      rvalid_q <= 1'b0;
    end else begin
      rvalid_q <= slave_req_i.req;
    end
  end

  tc_sram #(
      .NumWords (NumWords),
      .DataWidth(DataWidth),
      .NumPorts (32'd1)
  ) tc_ram_i (
      .clk_i  (clk_i),
      .rst_ni (rst_ni),
      .req_i  (slave_req_i.req),
      .we_i   (slave_req_i.we),
      .addr_i (mem_addr),
      .wdata_i(mem_wdata),
      .be_i   (slave_req_i.be),
      // output ports
      .rdata_o(slave_resp_o.rdata)
  );

  mmio_interface mmio_interface_i (
      .clk_i,
      .rst_ni,
      .reg_req_i,
      .reg_rsp_o,
      .din_0_o (din_0),
      .din_1_o (din_1),
      .result_i(result)
  );

  assign result = din_0 + din_1;

endmodule
