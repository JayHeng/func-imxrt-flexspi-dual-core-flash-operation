/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board.h"
#include "app.h"
#include "mcmgr.h"
#include "fsl_mu.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define APP_MU            MU1_MUB
#define APP_MU_IRQn       MU1_IRQn
#define APP_MU_IRQHandler MU1_IRQHandler
/* Channel transmit and receive register */
#define CHN_MU_REG_NUM kMU_MsgReg0

typedef enum {
    MU_CMD_FLASH_IAP_NOTIFY  = 0xA5A50001,  // CM7 -> CM33 notify
    MU_CMD_FLASH_IAP_READY   = 0xA5A50002,  // CM33 -> CM7 ready
    MU_CMD_FLASH_IAP_DONE    = 0xA5A50003,  // CM7 -> CM33 done
} mu_cmd_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

void mu_cm7_init(void)
{
    MU_Init(APP_MU);
    MU_EnableInterrupts(APP_MU, kMU_Rx0FullInterruptEnable);
    NVIC_EnableIRQ(APP_MU_IRQn);
}

void cm7_notify_for_flash_iap(void)
{
    MU_SendMsgNonBlocking(APP_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_NOTIFY);
}

void cm7_do_flash_iap_task(void)
{
    //flexspi_nor_erase_sector(FLASH_ADDR);
    //flexspi_nor_program_page(FLASH_ADDR, data, len);
}

void APP_MU_IRQHandler(void)
{
    uint32_t flag = 0;
    uint32_t msg = 0;

    flag = MU_GetStatusFlags(APP_MU);
    if ((flag & kMU_Rx0FullFlag) == kMU_Rx0FullFlag)
    {
        msg = MU_ReceiveMsgNonBlocking(APP_MU, CHN_MU_REG_NUM);
        if (msg == MU_CMD_FLASH_IAP_READY)
        {
            cm7_do_flash_iap_task();
            MU_SendMsgNonBlocking(APP_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_DONE);
        }
    }
    SDK_ISR_EXIT_BARRIER;
}

/*!
 * @brief Main function
 */
int main(void)
{
    uint32_t startupData, i;
    mcmgr_status_t status;

    /* Init board hardware.*/
    BOARD_InitHardware();

    /* Initialize MCMGR, install generic event handlers */
    (void)MCMGR_Init();

    /* Get the startup data */
    do
    {
        status = MCMGR_GetStartupData(kMCMGR_Core0, &startupData);
    } while (status != kStatus_MCMGR_Success);

    /* Make a noticable delay after the reset */
    /* Use startup parameter from the master core... */
    for (i = 0; i < startupData; i++)
    {
        SDK_DelayAtLeastUs(1000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    /* Configure LED */
    LED_INIT();

    for (;;)
    {
        SDK_DelayAtLeastUs(500000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
        LED_TOGGLE();
    }
}
