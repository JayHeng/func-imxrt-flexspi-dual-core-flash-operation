/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "fsl_mu.h"
#include "mcmgr.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define APP_USE_MCMGR 1

#define APP_CM33_MU       MU1_MUA
#define APP_CM7_MU        MU1_MUB
#define APP_MU_IRQn       MU1_IRQn
#define APP_MU_IRQHandler MU1_IRQHandler
/* Channel transmit and receive register */
#define CHN_MU_REG_NUM kMU_MsgReg0

typedef enum {
    MU_CMD_FLASH_IAP_NOTIFY  = 0xA5A50001,  // CM7 -> CM33 notify
    MU_CMD_FLASH_IAP_READY   = 0xA5A50002,  // CM33 -> CM7 ready
    MU_CMD_FLASH_IAP_DONE    = 0xA5A50003,  // CM7 -> CM33 done
} mu_cmd_t;

#define MCMGR_EVENT_FLASH_IAP_NOTIFY  (0U)
#define MCMGR_EVENT_FLASH_IAP_READY   (1U)
#define MCMGR_EVENT_FLASH_IAP_DONE    (2U)

#define FLASH_IAP_EVENT_TYPE    kMCMGR_RemoteApplicationEvent

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

////////////////////////////////////////////////////////////////////////////////
#if defined(MC_CORE0)

//static bool is_xip_available = true;

void mc_cm33_init(void)
{
#if !APP_USE_MCMGR
    MU_Init(APP_CM33_MU);
    MU_EnableInterrupts(APP_CM33_MU, kMU_Rx0FullInterruptEnable);
    NVIC_EnableIRQ(APP_MU_IRQn);
#endif
}

//__ramfunc 
void cm33_ready_for_flash_iap(void)
{
    /* Disable FlexSPI XIP / AHB buffer */
    FLEXSPI1->MCR0 |= FLEXSPI_MCR0_SWRESET_MASK;
    while (FLEXSPI1->MCR0 & FLEXSPI_MCR0_SWRESET_MASK);

#if APP_USE_MCMGR
    MCMGR_TriggerEvent(kMCMGR_Core1, FLASH_IAP_EVENT_TYPE, MCMGR_EVENT_FLASH_IAP_READY);
#else
    MU_SendMsgNonBlocking(APP_CM33_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_READY);
#endif
    
    //is_xip_available = false;
}

void cm33_back_to_flash_xip(void)
{
    //flexspi_nor_init();
    //flexspi_enable_xip();
  
    //is_xip_available = true;
}

static void cm33_flash_iap_cb(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    if (eventData == MCMGR_EVENT_FLASH_IAP_NOTIFY)
    {
        (void)PRINTF("Get iap notify from Secondary core.\r\n");
        cm33_ready_for_flash_iap();
        (void)PRINTF("Send iap ready to Secondary core.\r\n");
    }
    if (eventData == MCMGR_EVENT_FLASH_IAP_DONE)
    {
        (void)PRINTF("Get iap done from Secondary core.\r\n");
        cm33_back_to_flash_xip();
    }
}

void mc_cm33_register_cb(void)
{
    MCMGR_RegisterEvent(
        FLASH_IAP_EVENT_TYPE,
        cm33_flash_iap_cb,
        NULL);
}
#if !APP_USE_MCMGR
void APP_MU_IRQHandler(void)
{
    uint32_t flag = 0;
    uint32_t msg = 0;

    flag = MU_GetStatusFlags(APP_CM33_MU);
    if ((flag & kMU_Rx0FullFlag) == kMU_Rx0FullFlag)
    {
        msg = MU_ReceiveMsgNonBlocking(APP_CM33_MU, CHN_MU_REG_NUM);
        if (msg == MU_CMD_FLASH_IAP_NOTIFY)
        {
            cm33_ready_for_flash_iap();
        }
        if (msg == MU_CMD_FLASH_IAP_DONE)
        {
            cm33_back_to_flash_xip();
        }
    }
    SDK_ISR_EXIT_BARRIER;
}
#endif
////////////////////////////////////////////////////////////////////////////////
#elif defined(MC_CORE1)
void mc_cm7_init(void)
{
#if !APP_USE_MCMGR
    MU_Init(APP_CM7_MU);
    MU_EnableInterrupts(APP_CM7_MU, kMU_Rx0FullInterruptEnable);
    NVIC_EnableIRQ(APP_MU_IRQn);
#endif
}

void cm7_notify_for_flash_iap(void)
{
#if APP_USE_MCMGR
    MCMGR_TriggerEvent(kMCMGR_Core0, FLASH_IAP_EVENT_TYPE, MCMGR_EVENT_FLASH_IAP_NOTIFY);
#else
    MU_SendMsgNonBlocking(APP_CM7_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_NOTIFY);
#endif
}

void cm7_do_flash_iap_task(void)
{
    //flexspi_nor_erase_sector(FLASH_ADDR);
    //flexspi_nor_program_page(FLASH_ADDR, data, len);
  
    SDK_DelayAtLeastUs(3000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
}

static void cm7_flash_iap_cb(mcmgr_core_t coreNum, uint16_t eventData, void *context)
{
    if (eventData == MCMGR_EVENT_FLASH_IAP_READY)
    {
        cm7_do_flash_iap_task();
        MCMGR_TriggerEvent(kMCMGR_Core0, FLASH_IAP_EVENT_TYPE, MCMGR_EVENT_FLASH_IAP_DONE);
    }
}

void mc_cm7_register_cb(void)
{
    MCMGR_RegisterEvent(
        FLASH_IAP_EVENT_TYPE,
        cm7_flash_iap_cb,
        NULL);
}

#if !APP_USE_MCMGR
void APP_MU_IRQHandler(void)
{
    uint32_t flag = 0;
    uint32_t msg = 0;

    flag = MU_GetStatusFlags(APP_CM7_MU);
    if ((flag & kMU_Rx0FullFlag) == kMU_Rx0FullFlag)
    {
        msg = MU_ReceiveMsgNonBlocking(APP_CM7_MU, CHN_MU_REG_NUM);
        if (msg == MU_CMD_FLASH_IAP_READY)
        {
            cm7_do_flash_iap_task();
            MU_SendMsgNonBlocking(APP_CM7_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_DONE);
        }
    }
    SDK_ISR_EXIT_BARRIER;
}
#endif
#endif
