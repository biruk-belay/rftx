/*
 * Copyright (c) 2015-2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/***** Includes *****/
#include <stdlib.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* Power    */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
/* Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/PIN.h>

/* Board Header files */
#include "Board.h"

#include "smartrf_settings/smartrf_settings.h"

/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;

/*
 * Application LED pin configuration table:
 *   - All LEDs board LEDs are off.
 */
PIN_Config pinTable[] =
{
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    //Board_LED2 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

/***** Defines *****/
#define TX_TASK_STACK_SIZE 2048
#define TX_TASK_PRIORITY   2
const float CLOCK_SPEED = 4000.0;


/* Packet TX Configuration */
#define PAYLOAD_LENGTH      40
/* Set packet interval to 500ms */
#define PACKET_INTERVAL     (uint32_t)(4000000 * 1.0f)

/*Radio Configuration*/
#define SEC(X)  (uint32_t) (CLOCK_SPEED * 1000 * X) //tick for x seconds

/***** Prototypes *****/
static void txTaskFunction(UArg arg0, UArg arg1);
//static void dummyTaskFunction(UArg arg0, UArg arg1);

/***** Variable declarations *****/
static Task_Params txTaskParams;
//static Task_Params dummy_task_params;
Task_Struct txTask;
Task_Struct dummy_task;    /* not static so you can see in ROV */
static uint8_t txTaskStack[TX_TASK_STACK_SIZE];
//static uint8_t dummy_task_stack[TX_TASK_STACK_SIZE];

static RF_Object rfObject;
static RF_Handle rfHandle;

uint32_t time;
static uint8_t packet[PAYLOAD_LENGTH];
static uint16_t seqNumber;
static PIN_Handle pinHandle;


/***** Function definitions *****/
void TxTask_init(PIN_Handle inPinHandle)
{
    pinHandle = inPinHandle;

    Task_Params_init(&txTaskParams);
    txTaskParams.stackSize = TX_TASK_STACK_SIZE;
    txTaskParams.priority = TX_TASK_PRIORITY;
    txTaskParams.stack = &txTaskStack;
    txTaskParams.arg0 = (UInt)1000000;

    Task_construct(&txTask, txTaskFunction, &txTaskParams, NULL);
}

/*
void dummy_Task_init(PIN_Handle inPinHandle)
{
    pinHandle = inPinHandle;
    Task_Params_init(&dummy_task_params);
    dummy_task_params.stackSize = TX_TASK_STACK_SIZE;
    dummy_task_params.priority = 3;
    dummy_task_params.stack = dummy_task_stack;
    dummy_task_params.arg0 = (UInt)1000000;
    Task_construct(&dummy_task, dummyTaskFunction, &dummy_task_params, NULL);
}

static void dummyTaskFunction(UArg arg0, UArg arg1)
{
    while(1) {
        Task_sleep(100000);
        PIN_setOutputValue(pinHandle, Board_LED2,!PIN_getOutputValue(Board_LED2));
    }
}
*/

void initialize_radio()
{
    RF_Params rfParams;
    RF_Params_init(&rfParams);
    rfParams.nInactivityTimeout = 0; //time to Power-down after completing a command

    /* Request access to the radio */
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);

    /* Set the frequency */
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);

}

void change_tx_power(uint16_t tx_power, RF_Handle rfHandle)
{
    RF_yield(rfHandle);
    RF_cmdPropRadioDivSetup.txPower = tx_power;
}

static void txTaskFunction(UArg arg0, UArg arg1)
{
    uint32_t time;
    uint32_t t_start, t_end;
    uint32_t txPower_start, txPower_end = 0;
    uint16_t tx_Power_values [] = {0xA73F /*14dbm*/,
                                   0x58d8 /*10dbm*/,
                                   0x20c8 /*5dbm*/ ,
                                   0x12c3 /*0dbm*/ ,
                                   0x04c0 /*-10dbm*/,
                                    };
    static uint16_t index = 0;

    RF_cmdPropTx.pktLen = PAYLOAD_LENGTH;
    RF_cmdPropTx.pPkt = packet;
    RF_cmdPropTx.startTrigger.triggerType = TRIG_ABSTIME;
    RF_cmdPropTx.startTrigger.pastTrig = 1;
    RF_cmdPropTx.startTime = 0;

    initialize_radio();

    /* Get current time */
    time = RF_getCurrentTime();

    while(1)
    {
        /* Create packet with incrementing sequence number and random payload */
        packet[0] = (uint8_t)(seqNumber >> 8);
        packet[1] = (uint8_t)(seqNumber++);
        uint8_t i;

        for (i = 2; i < PAYLOAD_LENGTH; i++)
        {
            packet[i] = rand();
        }
#ifndef  CHANGE_TX_POWER
#define  CHANGE_TX_POWER 1
#endif

#ifdef CHANGE_TX_POWER
        /*Is change of tx_power is due*/
        txPower_start = RF_getCurrentTime();
        /* After X seconds select the next tx power*/
        if (txPower_start - txPower_end > SEC(3)) {
            if( index < (sizeof(tx_Power_values) / sizeof(uint16_t))) {
                change_tx_power(tx_Power_values [index], rfHandle);
                index ++;
            }
            txPower_end = txPower_start;
        }
#endif

        /* Set absolute TX time to utilize automatic power management */
        time += PACKET_INTERVAL;
        RF_cmdPropTx.startTime = time;

        /* Send packet */
        t_start = RF_getCurrentTime();
        RF_EventMask result = RF_runCmd(rfHandle, (RF_Op*)&RF_cmdPropTx, RF_PriorityNormal, NULL, 0);
        t_end = RF_getCurrentTime();

        /* Print the time it took to transmit pckt*/
        System_printf("MCU slept for %f msec and pckt was transmitted in %f msec\n",
               ((time - t_start) / CLOCK_SPEED),
               ((t_end - time) / CLOCK_SPEED));
        System_flush();

        System_printf("tx power %x \n", RF_cmdPropRadioDivSetup.txPower);
        System_flush();

        if (!(result & RF_EventLastCmdDone))
        {
            /* Error */
            while(1)
                printf("in error \n");
        }


        PIN_setOutputValue(pinHandle, Board_LED1,!PIN_getOutputValue(Board_LED1));
    }
}
/*
 *  ======== main ========
 */
int main(void)
{
    //Power_deepSleepPolicy();

    /* Call board init functions. */
    Board_initGeneral();

    /* Open LED pins */
    ledPinHandle = PIN_open(&ledPinState, pinTable);
    if(!ledPinHandle)
    {
        System_abort("Error initializing board LED pins\n");
    }

    /* Initialize task */
    TxTask_init(ledPinHandle);
//    dummy_Task_init(ledPinHandle);

    /* Start BIOS */
    BIOS_start();

    return (0);
}
