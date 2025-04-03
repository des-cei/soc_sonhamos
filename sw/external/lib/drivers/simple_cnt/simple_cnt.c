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
// File: simple-cnt.c
// Author(s):
//   Michele Caon
// Date: 10/11/2024
// Description: Driver for the simple counter.

#include <stdint.h>

#include "simple_cnt.h"
#include "soc_sonhamos.h"
#include "cnt_control_reg.h"
#include "hart.h" // for wait_for_interrupt()
#include "csr.h"
#include "rv_plic.h"

// Interrupt flag
static volatile uint8_t simple_cnt_irq_flag = 0;

__attribute__((inline)) void simple_cnt_enable() {
    *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_CONTROL_REG_OFFSET) |= (1 << CNT_CONTROL_CONTROL_ENABLE_BIT);
}

__attribute__((inline)) void simple_cnt_disable() {
    *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_CONTROL_REG_OFFSET) &= ~(1 << CNT_CONTROL_CONTROL_ENABLE_BIT);
}

__attribute__((inline)) void simple_cnt_clear() {
    *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_CONTROL_REG_OFFSET) |= (1 << CNT_CONTROL_CONTROL_CLEAR_BIT);
}

void simple_cnt_set_threshold(uint32_t threshold) {
    *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_THRESHOLD_REG_OFFSET) = threshold;
}

uint32_t simple_cnt_get_threshold() {
    return *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_THRESHOLD_REG_OFFSET);
}

__attribute__((inline)) uint8_t simple_cnt_tc() {
    return (*(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_STATUS_REG_OFFSET) & (1 << CNT_CONTROL_STATUS_TC_BIT)) != 0;
}

__attribute__((inline)) void simple_cnt_clear_tc() {
    *(volatile uint32_t *)(SIMPLE_CNT_PERIPH_START_ADDRESS + CNT_CONTROL_STATUS_REG_OFFSET) |= (1 << CNT_CONTROL_STATUS_TC_BIT);
}

void simple_cnt_set_value(uint32_t value) {
    *(volatile uint32_t *)(SIMPLE_CNT_START_ADDRESS) = value;
}

uint32_t simple_cnt_get_value() {
    return *(volatile uint32_t *)(SIMPLE_CNT_START_ADDRESS);
}

void simple_cnt_irq_handler(uint32_t id) {
    // Set IRQ flag
    simple_cnt_irq_flag = 1;

    // Clear the TC bit
    simple_cnt_clear_tc();
}

void simple_cnt_irq_install() {
    // Simple counter interrupt handler (EXT_INTR_0)
    if (plic_irq_set_priority(EXT_INTR_0, 1) != kPlicOk)
        return -1;
    if (plic_irq_set_enabled(EXT_INTR_0, kPlicToggleEnabled) != kPlicOk)
        return -1;
    if (plic_assign_external_irq_handler(EXT_INTR_0, &simple_cnt_irq_handler) != kPlicOk)
        return -1;
}

void simple_cnt_irq_clear() {
    // Mask interrupts before clearing previous pending IRQs
    CSR_CLEAR_BITS(CSR_REG_MSTATUS, 0x8);
    simple_cnt_irq_flag = 0;
    CSR_SET_BITS(CSR_REG_MSTATUS, 0x8);
}

void simple_cnt_wait_poll() {
    // Wait for the TC status bit to be set
    while (!simple_cnt_tc()) {
        continue; // Busy waiting
    }
    simple_cnt_clear_tc(); // clear TC bit
}

void simple_cnt_wait_irq() {
    // Wait for the counter interrupt
    while (simple_cnt_irq_flag == 0) {
        CSR_CLEAR_BITS(CSR_REG_MSTATUS, 0x8);
        wait_for_interrupt();
        CSR_SET_BITS(CSR_REG_MSTATUS, 0x8);
    }
    simple_cnt_irq_clear();
}

