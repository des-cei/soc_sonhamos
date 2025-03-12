#include <stdio.h>
#include <stdint.h>
#include "core_v_mini_mcu.h"
#include "mmio.h"
#include "template_ip_regs.h"
#include "x-heep.h"

#define TEMPLATE_IP_BASE_ADDR EXT_PERIPHERAL_START_ADDRESS

int main(int argc, char *argv[]) {

    int32_t operand_a = 16;
    int32_t operand_b = 28;

    mmio_region_t template_ip = mmio_region_from_addr(TEMPLATE_IP_BASE_ADDR);

    // Synchronous IP reset
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_CTRL_REG_OFFSET, 1 << TEMPLATE_IP_CTRL_CLR_BIT);

    // Write operands
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_DIN_0_REG_OFFSET, operand_a);
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_DIN_1_REG_OFFSET, operand_b);

    // Write enable to execute the add operation
    mmio_region_write32(template_ip, (ptrdiff_t) TEMPLATE_IP_CTRL_REG_OFFSET, 1 << TEMPLATE_IP_CTRL_EN_BIT);
    
    volatile uint32_t *ext_memory =  (volatile uint32_t *) EXT_SLAVE_START_ADDRESS;

    int32_t i;

    for(i = 0; i < 8192; i++) ext_memory[i] = i;

    for(i = 0; i < 8192; i++) {
        if(ext_memory[i] != i * (operand_a + operand_b)) {
            printf("FAIL!\r\n");
            return 1;
        }
    }
    
    printf("SUCCESS!\r\n");
    return 0;
}