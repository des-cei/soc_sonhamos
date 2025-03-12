// Copyright 2024 DES-CEI UPM
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
// Daniel Vazquez (daniel.vazquez@upm.es)

package soc_sonhamos_pkg;

  import addr_map_rule_pkg::*;
  import core_v_mini_mcu_pkg::*;

  // Number of external IPs (MMIO interfaces)
  localparam EXT_NPERIPHERALS = 1;
  // Number of external master ports to access X-HEEP memory
  localparam EXT_XBAR_NMASTER = 1; // Unused
  // Number of external slave ports to access external memories from X-HEEP
  localparam EXT_XBAR_NSLAVE = 1;

  // MMIO interfaces
  localparam logic [31:0] TEMPLATE_IP_PERIPH_START_ADDRESS = core_v_mini_mcu_pkg::EXT_PERIPHERAL_START_ADDRESS + 32'h0000000;
  localparam logic [31:0] TEMPLATE_IP_PERIPH_SIZE = 32'h0001000;
  localparam logic [31:0] TEMPLATE_IP_PERIPH_END_ADDRESS = TEMPLATE_IP_PERIPH_START_ADDRESS + TEMPLATE_IP_PERIPH_SIZE;
  localparam logic [31:0] TEMPLATE_IP_PERIPH_IDX = 32'd0;

  localparam addr_map_rule_t [EXT_NPERIPHERALS-1:0] EXT_PERIPHERALS_ADDR_RULES = '{
      '{
          idx: TEMPLATE_IP_PERIPH_IDX,
          start_addr: TEMPLATE_IP_PERIPH_START_ADDRESS,
          end_addr: TEMPLATE_IP_PERIPH_END_ADDRESS
      }
  };

  localparam int unsigned EXT_PERIPHERALS_PORT_SEL_WIDTH = EXT_NPERIPHERALS > 1 ? $clog2(
      EXT_NPERIPHERALS
  ) : 32'd1;

  // Slave interfaces
  localparam logic [31:0] TEMPLATE_MEMORY_START_ADDRESS = core_v_mini_mcu_pkg::EXT_SLAVE_START_ADDRESS + 32'h0000000;
  localparam logic [31:0] TEMPLATE_MEMORY_SIZE = 32'h2000;
  localparam logic [31:0] TEMPLATE_MEMORY_END_ADDRESS = TEMPLATE_MEMORY_START_ADDRESS + TEMPLATE_MEMORY_SIZE;
  localparam logic [31:0] TEMPLATE_MEMORY_IDX = 32'd0;

  localparam addr_map_rule_t [EXT_XBAR_NSLAVE-1:0] EXT_XBAR_ADDR_RULES = '{
      '{
          idx: TEMPLATE_MEMORY_IDX,
          start_addr: TEMPLATE_MEMORY_START_ADDRESS,
          end_addr: TEMPLATE_MEMORY_END_ADDRESS
      }
  };

endpackage  // soc_sonhamos_pkg
