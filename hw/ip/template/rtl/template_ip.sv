// Copyright 2024 CEI-UPM
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// Daniel Vazquez (daniel.vazquez@upm.es)

module template_ip
  import reg_pkg::*;
  import obi_pkg::*;
(
    // Clock and reset
    input logic clk_i,
    input logic rst_ni,

    // MMIO interface
    input  reg_req_t reg_req_i,
    output reg_rsp_t reg_rsp_o
);

  logic [31:0] din_0, din_1, result;

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
