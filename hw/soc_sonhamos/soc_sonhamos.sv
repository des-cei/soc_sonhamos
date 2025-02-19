// Copyright 2024 CEI UPM
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// Daniel Vazquez (daniel.vazquez@upm.es)

module soc_sonhamos #(
    parameter COREV_PULP = 0,
    parameter FPU = 0,
    parameter ZFINX = 0,
    parameter X_EXT = 0
) (
    inout logic clk_i,
    inout logic rst_ni,

    inout logic boot_select_i,
    inout logic execute_from_flash_i,

    inout logic jtag_tck_i,
    inout logic jtag_tms_i,
    inout logic jtag_trst_ni,
    inout logic jtag_tdi_i,
    inout logic jtag_tdo_o,

    inout logic uart_rx_i,
    inout logic uart_tx_o,

    output logic [31:0] exit_value_o,
    inout  logic        exit_valid_o,

    inout logic       spi_flash_sck_io,
    inout logic [1:0] spi_flash_csb_io,
    inout logic [3:0] spi_flash_sd_io,

    inout logic       spi_sck_io,
    inout logic [1:0] spi_csb_io,
    inout logic [3:0] spi_sd_io,

    inout logic [31:0] gpio_io
);

  import obi_pkg::*;
  import reg_pkg::*;
  import soc_sonhamos_pkg::*;
  import core_v_mini_mcu_pkg::*;

  localparam AO_SPC_NUM = 1;

  // External master and peripheral ports
  reg_req_t ext_xbar_slave_req;
  reg_rsp_t ext_xbar_slave_resp;
  reg_req_t ext_periph_slave_req;
  reg_rsp_t ext_periph_slave_resp;
  obi_req_t [soc_sonhamos_pkg::LOG_EXT_XBAR_NMASTER-1:0] ext_master_req;
  obi_resp_t [soc_sonhamos_pkg::LOG_EXT_XBAR_NMASTER-1:0] ext_master_resp;
  obi_req_t [soc_sonhamos_pkg::LOG_EXT_XBAR_NMASTER-1:0] heep_slave_req;
  obi_resp_t [soc_sonhamos_pkg::LOG_EXT_XBAR_NMASTER-1:0] heep_slave_resp;

  // Unconnected signals
  obi_req_t heep_core_instr_req;
  obi_resp_t heep_core_instr_resp;
  obi_req_t heep_core_data_req;
  obi_resp_t heep_core_data_resp;
  obi_req_t heep_debug_master_req;
  obi_resp_t heep_debug_master_resp;
  obi_req_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_read_req;
  obi_resp_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_read_resp;
  obi_req_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_write_req;
  obi_resp_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_write_resp;
  obi_req_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_addr_req;
  obi_resp_t [core_v_mini_mcu_pkg::DMA_NUM_MASTER_PORTS-1:0] heep_dma_addr_resp;

  // External DMA slots
  logic [core_v_mini_mcu_pkg::DMA_CH_NUM-1:0] ext_dma_slot_tx;
  logic [core_v_mini_mcu_pkg::DMA_CH_NUM-1:0] ext_dma_slot_rx;
  logic [core_v_mini_mcu_pkg::DMA_CH_NUM-1:0] dma_done;

  // External SPC interface signals
  reg_req_t [AO_SPC_NUM-1:0] ext_ao_peripheral_req;
  reg_rsp_t [AO_SPC_NUM-1:0] ext_ao_peripheral_resp;

  // DMA stop
  // logic [core_v_mini_mcu_pkg::DMA_CH_NUM-1:0] ext_dma_stop;

  // External interrupts
  logic [core_v_mini_mcu_pkg::NEXT_INT-1:0] intr_vector_ext;


  // External subsystems
  logic external_subsystem_rst_n;
  logic external_ram_banks_set_retentive_n;
  /* verilator lint_off unused */
  logic external_subsystem_clkgate_en_n;
  logic external_subsystem_powergate_switch_n;
  logic external_subsystem_powergate_switch_ack_n;
  logic external_subsystem_powergate_iso_n;

  // External bus
  // ----------------------
  // The external bus connects the external peripherals among them and to
  // the corresponding X-HEEP slave port (to the internal system bus).
  ext_bus #(
      .EXT_XBAR_NMASTER(LOG_EXT_XBAR_NMASTER),
      .EXT_XBAR_NSLAVE (LOG_EXT_XBAR_NSLAVE)
  ) ext_bus_i (
      .clk_i                   (clk_i),
      .rst_ni                  (rst_ni),
      .addr_map_i              (EXT_PERIPHERALS_ADDR_RULES),
      .default_idx_i           ('0),
      .heep_core_instr_req_i   (heep_core_instr_req),
      .heep_core_instr_resp_o  (heep_core_instr_resp),
      .heep_core_data_req_i    (heep_core_data_req),
      .heep_core_data_resp_o   (heep_core_data_resp),
      .heep_debug_master_req_i (heep_debug_master_req),
      .heep_debug_master_resp_o(heep_debug_master_resp),
      .heep_dma_read_req_i     (heep_dma_read_req),
      .heep_dma_read_resp_o    (heep_dma_read_resp),
      .heep_dma_write_req_i    (heep_dma_write_req),
      .heep_dma_write_resp_o   (heep_dma_write_resp),
      .heep_dma_addr_req_i     (heep_dma_addr_req),
      .heep_dma_addr_resp_o    (heep_dma_addr_resp),
      .ext_master_req_i        (ext_master_req),
      .ext_master_resp_o       (ext_master_resp),
      .heep_slave_req_o        (heep_slave_req),
      .heep_slave_resp_i       (heep_slave_resp),
      .ext_slave_req_o         (ext_xbar_slave_req),
      .ext_slave_resp_i        (ext_xbar_slave_resp)
  );

  assign ext_master_req = '0;

  template_ip template_ip_i (
      .clk_i,
      .rst_ni(rst_ni || external_subsystem_rst_n),
      .reg_req_i(ext_periph_slave_req),
      .reg_rsp_o(ext_periph_slave_resp)
  );

  // eXtension Interface
  if_xif #() ext_if ();

  x_heep_system #(
      .COREV_PULP(COREV_PULP),
      .FPU(FPU),
      .ZFINX(ZFINX),
      .EXT_XBAR_NMASTER(LOG_EXT_XBAR_NMASTER),
      .X_EXT(X_EXT)
  ) x_heep_system_i (
      .clk_i,
      .rst_ni,
      .boot_select_i,
      .execute_from_flash_i,
      .jtag_tck_i,
      .jtag_tms_i,
      .jtag_trst_ni,
      .jtag_tdi_i,
      .jtag_tdo_o,
      .uart_rx_i,
      .uart_tx_o,
      .exit_valid_o,
      .gpio_0_io(gpio_io[0]),
      .gpio_1_io(gpio_io[1]),
      .gpio_2_io(gpio_io[2]),
      .gpio_3_io(gpio_io[3]),
      .gpio_4_io(gpio_io[4]),
      .gpio_5_io(gpio_io[5]),
      .gpio_6_io(gpio_io[6]),
      .gpio_7_io(gpio_io[7]),
      .gpio_8_io(gpio_io[8]),
      .gpio_9_io(gpio_io[9]),
      .gpio_10_io(gpio_io[10]),
      .gpio_11_io(gpio_io[11]),
      .gpio_12_io(gpio_io[12]),
      .gpio_13_io(gpio_io[13]),
      .gpio_14_io(gpio_io[14]),
      .gpio_15_io(gpio_io[15]),
      .gpio_16_io(gpio_io[16]),
      .gpio_17_io(gpio_io[17]),
      .spi_flash_sck_io,
      .spi_flash_cs_0_io(spi_flash_csb_io[0]),
      .spi_flash_cs_1_io(spi_flash_csb_io[1]),
      .spi_flash_sd_0_io(spi_flash_sd_io[0]),
      .spi_flash_sd_1_io(spi_flash_sd_io[1]),
      .spi_flash_sd_2_io(spi_flash_sd_io[2]),
      .spi_flash_sd_3_io(spi_flash_sd_io[3]),
      .spi_sck_io,
      .spi_cs_0_io(spi_csb_io[0]),
      .spi_cs_1_io(spi_csb_io[1]),
      .spi_sd_0_io(spi_sd_io[0]),
      .spi_sd_1_io(spi_sd_io[1]),
      .spi_sd_2_io(spi_sd_io[2]),
      .spi_sd_3_io(spi_sd_io[3]),
      .pdm2pcm_pdm_io(gpio_io[18]),
      .pdm2pcm_clk_io(gpio_io[19]),
      .i2s_sck_io(gpio_io[20]),
      .i2s_ws_io(gpio_io[21]),
      .i2s_sd_io(gpio_io[22]),
      .spi2_cs_0_io(gpio_io[23]),
      .spi2_cs_1_io(gpio_io[24]),
      .spi2_sck_io(gpio_io[25]),
      .spi2_sd_0_io(gpio_io[26]),
      .spi2_sd_1_io(gpio_io[27]),
      .spi2_sd_2_io(gpio_io[28]),
      .spi2_sd_3_io(gpio_io[29]),
      .i2c_sda_io(gpio_io[30]),
      .i2c_scl_io(gpio_io[31]),
      .exit_value_o,
      .intr_vector_ext_i(intr_vector_ext),
      .xif_compressed_if(ext_if),
      .xif_issue_if(ext_if),
      .xif_commit_if(ext_if),
      .xif_mem_if(ext_if),
      .xif_mem_result_if(ext_if),
      .xif_result_if(ext_if),
      .ext_xbar_master_req_i(heep_slave_req),
      .ext_xbar_master_resp_o(heep_slave_resp),
      .ext_core_instr_req_o(heep_core_instr_req),
      .ext_core_instr_resp_i(heep_core_instr_resp),
      .ext_core_data_req_o(heep_core_data_req),
      .ext_core_data_resp_i(heep_core_data_resp),
      .ext_debug_master_req_o(heep_debug_master_req),
      .ext_debug_master_resp_i(heep_debug_master_resp),
      .ext_dma_read_req_o(heep_dma_read_req),
      .ext_dma_read_resp_i(heep_dma_read_resp),
      .ext_dma_write_req_o(heep_dma_write_req),
      .ext_dma_write_resp_i(heep_dma_write_resp),
      .ext_dma_addr_req_o(heep_dma_addr_req),
      .ext_dma_addr_resp_i(heep_dma_addr_resp),
      .ext_peripheral_slave_req_o(ext_periph_slave_req),
      .ext_peripheral_slave_resp_i(ext_periph_slave_resp),
      .ext_ao_peripheral_req_i(ext_ao_peripheral_req),
      .ext_ao_peripheral_resp_o(ext_ao_peripheral_resp),
      .external_subsystem_powergate_switch_no(external_subsystem_powergate_switch_n),
      .external_subsystem_powergate_switch_ack_ni(external_subsystem_powergate_switch_ack_n),
      .external_subsystem_powergate_iso_no(external_subsystem_powergate_iso_n),
      .external_subsystem_rst_no(external_subsystem_rst_n),
      .external_ram_banks_set_retentive_no(external_ram_banks_set_retentive_n),
      .external_subsystem_clkgate_en_no(external_subsystem_clkgate_en_n),
      .dma_done_o(dma_done),
      .ext_dma_slot_tx_i(ext_dma_slot_tx),
      .ext_dma_slot_rx_i(ext_dma_slot_rx),
      .ext_dma_stop_i('0)
  );

  // Asign undriven signals
  assign intr_vector_ext = '0;
  assign ext_xbar_slave_resp = '0;
  assign ext_dma_slot_tx = '0;
  assign ext_dma_slot_rx = '0;
  assign ext_ao_peripheral_req = '0;
  assign external_subsystem_powergate_switch_ack_n = '1;

endmodule  // soc_sonhamos
