// System library headers
#include <stdio.h>

// Custom library headers
#include "core_v_mini_mcu.h"
#include "fast_intr_ctrl.h"
#include "simple_cnt.h"
#include "ext_irq.h"
#include "csr.h"

// Main body
// ---------
int main(void)
{
    uint32_t val = 255;
    uint32_t ret = 0;

    // Initialize external interrupts handler(s)
    ext_irq_init();

    // Set the counter threshold
    simple_cnt_set_threshold(val);

    // Read back the counter threshold
    ret = simple_cnt_get_threshold();
    if (val != ret) {
        printf("Error: threshold value mismatch\n");
        return -1;
    }

    // Set the counter value
    val = 42;
    simple_cnt_set_value(val);

    // Read back the counter value
    ret = simple_cnt_get_value();
    if (val != ret) {
        printf("Error: value mismatch 1\n");
        return -1;
    }

    // Clear the counter
    simple_cnt_clear();

    // Read back the counter value
    ret = simple_cnt_get_value();
    if (0 != ret) {
        printf("Error: value mismatch 2\n");
        return -1;
    }

    // Enable the counter
    simple_cnt_enable();

    // Wait for the counter to reach the threshold
    simple_cnt_wait_poll();
    printf("TC set\n");

    // Disable the counter and clear it
    simple_cnt_disable();
    simple_cnt_clear();

    // Install the counter interrupt handler and enable the counter
    simple_cnt_irq_install(); // install the handler in X-HEEP's PLIC
    simple_cnt_enable(); // start counting

    // Wait for the counter interrupt
    simple_cnt_wait_irq(); // put CPU to sleep (clock-gating) until the interrupt is received
    simple_cnt_disable(); // disable the counter to avoid further interrupts
    printf("IRQ received\n");

    // Read back the counter value
    ret = simple_cnt_get_value();
    printf("Done. Counter value: %d\n", ret);
}

