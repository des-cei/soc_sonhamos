#include <stdio.h>
#include <stdint.h>
#include "core_v_mini_mcu.h"
#include "mmio.h"
#include "template_ip_regs.h"
#include "x-heep.h"

#define TEMPLATE_IP_BASE_ADDR EXT_PERIPHERAL_START_ADDRESS

int main(int argc, char *argv[])
{
    uint32_t operand_a = 16;
    uint32_t operand_b = 28;
    uint32_t expected_result = 0;
    volatile uint32_t actual_result = 0;

    mmio_region_t template_ip = mmio_region_from_addr(TEMPLATE_IP_BASE_ADDR);

    // Synchronous IP reset
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_CTRL_REG_OFFSET, 1 << TEMPLATE_IP_CTRL_CLR_BIT);

    // Write operands
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_DIN_0_REG_OFFSET, operand_a);
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_DIN_1_REG_OFFSET, operand_b);

    // Write enable to execute the add operation
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_CTRL_REG_OFFSET, 1 << TEMPLATE_IP_CTRL_EN_BIT);

    // Read result
    actual_result = mmio_region_read32(template_ip, (ptrdiff_t) TEMPLATE_IP_RESULT_REG_OFFSET);

    // CPU execution
    expected_result = operand_a + operand_b;

    if(expected_result != actual_result) {
        printf("FAIL!\r\n");
        return 1;
    }
    
    printf("SUCCESS!\r\n");
    return 0;
}
