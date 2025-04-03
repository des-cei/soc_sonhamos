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
// File: cnt_tb.cpp
// Author(s):
//   Luigi Giuffrida
//   Michele Caon
// Date: 08/11/2024
// Description: TB for the OBI counter

#include <iostream>
#include <getopt.h>
#include <random>
#include <time.h>

// Verilator libraries
#include <verilated.h>
#include <verilated_fst_c.h>

// DUT header
#include "Vcnt_tb_wrapper.h"
#include "cnt_control_reg.h"

// Testbench components
#include "tb_macros.hh"
#include "tb_components.hh"

// Defines
// -------
#define FST_FILENAME "logs/waves.fst"
#define END_OF_RESET_TIME 5
#define MAX_SIM_CYCLES 2e6
#define MAX_SIM_TIME (MAX_SIM_CYCLES * 2)
#define WATCHDOG_TIMEOUT 100 // cycles to wait for a program step to complete
#define END_OF_TEST_TIMEOUT 10 // cycles between done assertion and simulation end
#define RUN_CYCLES 500

// Generate clock and reset
void clkGen(Vcnt_tb_wrapper *dut);
void rstDut(Vcnt_tb_wrapper *dut, vluint64_t sim_time);

// Generate OBI transactions
ObiReqTx *genObiWriteReqTx(const vluint32_t addr_offs, const vluint32_t wdata, vluint8_t be);
ObiReqTx *genObiReadReqTx(const vluint32_t addr_offs);
RegReqTx *genRegWriteReqTx(const vluint32_t addr_offs, const vluint32_t wdata, vluint8_t wstrb);
RegReqTx *genRegReadReqTx(const vluint32_t addr_offs);

// Run a number of cycles
void runCycles(unsigned int ncycles, Vcnt_tb_wrapper *dut, uint8_t gen_waves, VerilatedFstC *trace);

// Global variables
vluint64_t sim_cycles = 0;
TbLogger logger;    // testbench logger

int main(int argc, char *argv[])
{
    // Define command-line options
    const option longopts[] = {
        {"log_level", required_argument, NULL, 'l'},
        {"gen_waves", required_argument, NULL, 'w'},
        {"seed", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };

    // Process command-line options
    // ----------------------------
    int opt; // current option
    int prg_seed = time(NULL);
    bool gen_waves = true;
    while ((opt = getopt_long(argc, argv, "l:w:", longopts, NULL)) >= 0)
    {
        switch (opt)
        {
        case 'l': // set the log level
            logger.setLogLvl(optarg);
            TB_CONFIG("Log level set to %s", optarg);
            break;
        case 'w': // generate waves
            if (!strcmp(optarg, "true")) {
                gen_waves = 1;
                TB_CONFIG("Waves enabled");
            }
            else {
                gen_waves = 0;
                TB_CONFIG("Waves disabled");
            }
            break;
        case 's': // set the seed
            prg_seed = atoi(optarg);
            TB_CONFIG("Seed set to %d", prg_seed);
            break;
        default:
            TB_ERR("ERROR: unrecognised option %c.\n", opt);
            exit(EXIT_FAILURE);
        }
    }

    // Create Verilator simulation context
    VerilatedContext *cntx = new VerilatedContext;

    // Pass simulation context to the logger
    logger.setSimContext(cntx);

    if (gen_waves)
    {
        Verilated::mkdir("logs");
        cntx->traceEverOn(true);
    }

    // Instantiate DUT
    Vcnt_tb_wrapper *dut = new Vcnt_tb_wrapper(cntx);

    // Set the file to store the waveforms in
    VerilatedFstC *trace = NULL;
    if (gen_waves)
    {
        trace = new VerilatedFstC;
        dut->trace(trace, 10);
        trace->open(FST_FILENAME);
    }

    // TB components
    Drv *drv = new Drv(dut);
    Scb *scb = new Scb();
    ReqMonitor *reqMon = new ReqMonitor(dut, scb);
    RspMonitor *rspMon = new RspMonitor(dut, scb);

    // Initialize PRG
    srand(prg_seed);

    // Simulation program
    // ------------------
    unsigned int step_cnt = 0;
    unsigned int prev_step_cnt = 0; // previous test program step counter
    unsigned int watchdog = 0; // watchdog counter
    bool end_of_test = false;
    unsigned int exit_timer = 0; // exit timer
    bool obi_accepted = false; // OBI request accepted flag
    bool reg_accepted = false; // OBI request accepted flag
    bool irq_received = false; // interrupt received flag
    bool obi_check_req = false;
    vluint32_t obi_check_data = 0;
    bool reg_check_req = false;
    vluint32_t reg_check_data = 0;
    vluint32_t data = 0;
    vluint32_t obi_rdata = 0;
    vluint32_t reg_rdata = 0;
    vluint32_t thr = rand() % 63 + 1;
    ObiReqTx *obi_req = NULL;
    RegReqTx *reg_req = NULL;

    TB_LOG(LOG_LOW, "Starting simulation...");
    while (!cntx->gotFinish() && cntx->time() < MAX_SIM_TIME)
    {
        // Generate clock and reset
        rstDut(dut, cntx->time());
        clkGen(dut);

        // Evaluate simulation step
        dut->eval();

        if (dut->clk_i == 1 && cntx->time() > END_OF_RESET_TIME)
        {
            // Reset check schedule requests
            obi_check_req = false;
            reg_check_req = false;

            switch (step_cnt)
            {
            // Set the counter threshold
            case 0:
                if (!reg_accepted) {
                    data = thr;
                    TB_LOG(LOG_HIGH, "## Writing counter threshold to '%u'...", data);
                    reg_req = genRegWriteReqTx(CNT_CONTROL_THRESHOLD_REG_OFFSET, data, 0xf);
                    break;
                }
                reg_accepted = false;
                step_cnt++;
            
            // Read back the threshold value
            case 1:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter threshold...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_THRESHOLD_REG_OFFSET);
                    reg_check_req = true;
                    reg_check_data = data;
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through
            
            // Read the current counter value
            case 2:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = 0;
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Read the TC bit
            case 3:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading TC bit...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_STATUS_REG_OFFSET);
                    reg_check_req = true;
                    reg_check_data = 0;
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Set the counter enable bit
            case 4:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Enabling counter...");
                    data = 1 << CNT_CONTROL_CONTROL_ENABLE_BIT;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_CONTROL_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Read back the control register
            case 5:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading control register...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_CONTROL_REG_OFFSET);
                    reg_check_req = true;
                    reg_check_data = data;
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Wait some cycles
            case 6 ... 10:
                step_cnt++;
                break;

            // Read counter value
            case 11:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Wait for interrupt
            case 12:
                if (!irq_received) break;
                TB_LOG(LOG_LOW, "## Interrupt received!");
                step_cnt++;

            // Read the TC bit
            case 13:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading TC bit...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_STATUS_REG_OFFSET);
                    reg_check_req = true;
                    reg_check_data = 1;
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Read counter value
            case 14:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = 1;
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Disable the counter
            case 15:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Disabling counter...");
                    data = 0;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_CONTROL_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Wait some cycles
            case 16:
                step_cnt++;
                break;

            // Set the counter value
            case 17:
                if (!obi_accepted) {
                    thr = rand() % 63 + 1;
                    TB_LOG(LOG_HIGH, "## Writing counter value to '%u'...", thr);
                    obi_req = genObiWriteReqTx(0x0, thr, 0xf);
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Read the counter value
            case 18:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = thr;
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Enable the counter
            case 19:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Enabling counter...");
                    data = 1 << CNT_CONTROL_CONTROL_ENABLE_BIT;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_CONTROL_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Wait one cycle
            case 20:
                step_cnt++;
                break;

            // Read the counter value
            case 21:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = thr + 1;
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Clear the counter
            case 22:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Clearing counter...");
                    data = 1 << CNT_CONTROL_CONTROL_CLEAR_BIT;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_CONTROL_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Wait one cycle
            case 23:
                step_cnt++;
                break;

            // Read the counter value
            case 24:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = 0;
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Wait some cycles
            case 25 ... 29:
                step_cnt++;
                break;

            // Set a new threshold
            case 30:
                if (!reg_accepted) {
                    thr = rand() % 63 + 1;
                    TB_LOG(LOG_HIGH, "## Writing counter threshold to '%u'...", thr);
                    reg_req = genRegWriteReqTx(CNT_CONTROL_THRESHOLD_REG_OFFSET, thr, 0xf);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Read back the threshold value
            case 31:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter threshold...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_THRESHOLD_REG_OFFSET);
                    reg_check_req = true;
                    reg_check_data = thr;
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Restart the counter
            case 32:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Enabling counter...");
                    data = 1 << CNT_CONTROL_CONTROL_ENABLE_BIT;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_CONTROL_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            // Wait for TC in polling
            case 33:
                if (!reg_accepted) {
                    TB_LOG(LOG_FULL, "## Polling TC bit...");
                    reg_req = genRegReadReqTx(CNT_CONTROL_STATUS_REG_OFFSET);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through
            
            // Wait for TC in polling
            case 34:
                if (reg_rdata & (1 << CNT_CONTROL_STATUS_TC_BIT)) {
                    TB_LOG(LOG_LOW, "## TC bit set!");
                    step_cnt++;
                    break;
                }
                TB_LOG(LOG_FULL, "## Polling TC bit...");
                reg_req = genRegReadReqTx(CNT_CONTROL_STATUS_REG_OFFSET);
                break;

            // Read the counter value
            case 35:
                if (!obi_accepted) {
                    TB_LOG(LOG_HIGH, "## Reading counter value...");
                    obi_req = genObiReadReqTx(0x0);
                    obi_check_req = true;
                    obi_check_data = 2; // counter resets, but it takes 2 cycles for SW to see TC in polling
                    break;
                }
                obi_accepted = false;
                step_cnt++; // and fall through

            // Clear TC bit
            case 36:
                if (!reg_accepted) {
                    TB_LOG(LOG_HIGH, "## Clearing TC bit...");
                    data = 1 << CNT_CONTROL_STATUS_TC_BIT;
                    reg_req = genRegWriteReqTx(CNT_CONTROL_STATUS_REG_OFFSET, data, 0x1);
                    break;
                }
                reg_accepted = false;
                step_cnt++; // and fall through

            default:
                // Set exit flag
                end_of_test = true;
                break;
            }
            
            // Drive DUT inputs
            drv->drive(obi_req, reg_req);
            delete obi_req;
            delete reg_req;
            obi_req = NULL;
            reg_req = NULL;

            // Update input signals
            dut->eval();

            // Monitor DUT signals
            reqMon->monitor();
            rspMon->monitor();
            
            irq_received = rspMon->irq();
            obi_accepted = reqMon->acceptedObi();
            reg_accepted = reqMon->acceptedReg();

            // Schedule checks
            if (reg_accepted) {
                reg_rdata = rspMon->getRegData();
                if (reg_check_req) scb->scheduleRegCheck(reg_check_data);
            }
            if (rspMon->isDataReadyObi()) obi_rdata = rspMon->getObiData();
            if (obi_accepted && obi_check_req) scb->scheduleObiCheck(obi_check_data);

            // Trigger scheduled checks
            if (scb->checkData() != 0) end_of_test = true;

            // Check for exit conditions
            if (prev_step_cnt != step_cnt) watchdog = 0;
            else watchdog++;
            if (watchdog > WATCHDOG_TIMEOUT) {
                TB_WARN("Watchdog timeout reached: terminating simulation.");
                scb->notifyError();
                break;
            }
            prev_step_cnt = step_cnt;
            if (end_of_test)
            {
                if (exit_timer++ == END_OF_TEST_TIMEOUT) {
                    TB_LOG(LOG_MEDIUM, "End of simulation reached: terminating.");
                    break;
                }
            }
        }

        // Dump waveforms and advance simulation time
        if (gen_waves) trace->dump(cntx->time());
        if (dut->clk_i == 1) sim_cycles++;
        cntx->timeInc(1);
    }

    // Simulation complete
    dut->final();

    // Print simulation summary
    if (scb->getErrNum() > 0)
    {
        TB_ERR("CHECKS FAILED > errors: %u/%u", scb->getErrNum(), scb->getTxNum());
        if (gen_waves) trace->close();
        exit(EXIT_SUCCESS);
    }
    else if (!scb->isDone())
    {
        TB_ERR("CHECKS PENDING > errors: %u/%u", scb->getErrNum(), scb->getTxNum());
        if (gen_waves) trace->close();
        exit(EXIT_SUCCESS);
    }
    TB_SUCCESS(LOG_LOW, "CHECKS PASSED > errors: %u (checked %u transactions)", scb->getErrNum(), scb->getTxNum());

    // Clean up and exit
    if (gen_waves) trace->close();
    delete dut;
    delete cntx;
    delete obi_req;
    delete reg_req;

    return 0;
}

void clkGen(Vcnt_tb_wrapper *dut)
{
    dut->clk_i ^= 1;
}

void rstDut(Vcnt_tb_wrapper *dut, vluint64_t sim_time)
{
    dut->rst_ni = 1;
    if (sim_time > 1 && sim_time < END_OF_RESET_TIME)
    {
        dut->rst_ni = 0;
    }
}

void runCycles(unsigned int ncycles, Vcnt_tb_wrapper *dut, uint8_t gen_waves, VerilatedFstC *trace)
{
    VerilatedContext *cntx = dut->contextp();
    for (unsigned int i = 0; i < (2 * ncycles); i++)
    {
        // Generate clock
        clkGen(dut);

        // Evaluate the DUT
        dut->eval();

        // Save waveforms
        if (gen_waves)
            trace->dump(cntx->time());
        if (dut->clk_i == 1)
            sim_cycles++;
        cntx->timeInc(1);
    }
}

// Issue write OBI transaction
ObiReqTx *genObiWriteReqTx(const vluint32_t addr_offs, const vluint32_t wdata, vluint8_t be)
{
    ObiReqTx *req = new ObiReqTx;

    // OBI write request
    req->obi_req.req = 1;
    req->obi_req.we = 1;
    req->obi_req.be = be;
    req->obi_req.addr = addr_offs;
    req->obi_req.wdata = wdata;

    return req;
}

// Issue read OBI transaction
ObiReqTx *genObiReadReqTx(const vluint32_t addr_offs)
{
    ObiReqTx *req = new ObiReqTx;

    // OBI read request
    req->obi_req.req = 1;
    req->obi_req.we = 0;
    req->obi_req.be = 0xf;
    req->obi_req.addr = addr_offs;
    req->obi_req.wdata = 0;

    return req;
}

// Issue write register interface transaction
RegReqTx *genRegWriteReqTx(const vluint32_t addr_offs, const vluint32_t wdata, vluint8_t wstrb)
{
    RegReqTx *req = new RegReqTx;

    // OBI write request
    req->reg_req.valid = 1;
    req->reg_req.write = 1;
    req->reg_req.wstrb = wstrb;
    req->reg_req.addr = addr_offs;
    req->reg_req.wdata = wdata;

    return req;
}

// Issue read register interface transaction
RegReqTx *genRegReadReqTx(const vluint32_t addr_offs)
{
    RegReqTx *req = new RegReqTx;

    // OBI read request
    req->reg_req.valid = 1;
    req->reg_req.write = 0;
    req->reg_req.wstrb = 0xf;
    req->reg_req.addr = addr_offs;
    req->reg_req.wdata = 0;

    return req;
}
