// Copyright 2024 CEI-UPM
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// Daniel Vazquez (daniel.vazquez@upm.es)

module mmio_interface
  import reg_pkg::*;
  import obi_pkg::*;
(
    // Clock and reset
    input logic clk_i,
    input logic rst_ni,

    // MMIO interface
    input  reg_req_t reg_req_i,
    output reg_rsp_t reg_rsp_o,

    // IP signals
    output logic [31:0] din_0_o,
    output logic [31:0] din_1_o,

    input logic [31:0] result_i
);

  import template_ip_reg_pkg::*;

  template_ip_reg2hw_t reg2hw;
  template_ip_hw2reg_t hw2reg;

  template_ip_reg_top #(
      .reg_req_t(reg_req_t),
      .reg_rsp_t(reg_rsp_t)
  ) template_ip_reg_top_i (
      .clk_i,
      .rst_ni,
      .reg_req_i,
      .reg_rsp_o,
      .hw2reg,
      .reg2hw,
      .devmode_i(1'b1)
  );

  // Control signals
  assign hw2reg.ctrl.en.de = reg2hw.ctrl.en.q;
  assign hw2reg.ctrl.en.d  = 1'b0;
  assign hw2reg.ctrl.clr.de   = reg2hw.ctrl.clr.q;
  assign hw2reg.ctrl.clr.d    = 1'b0;

  // Data signals
  assign din_0_o              = reg2hw.din_0.q;
  assign hw2reg.din_0.de      = reg2hw.ctrl.clr.q;
  assign hw2reg.din_0.d       = '0;

  assign din_1_o              = reg2hw.din_1.q;
  assign hw2reg.din_1.de      = reg2hw.ctrl.clr.q;
  assign hw2reg.din_1.d       = '0;

  assign hw2reg.result.de     = reg2hw.ctrl.clr.q || reg2hw.ctrl.en.q;
  assign hw2reg.result.d      = reg2hw.ctrl.clr.q ? '0 : result_i;

endmodule
