/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include "mcmgr.h"
#include "fsl_mu.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define APP_MU            MU1_MUA
#define APP_MU_IRQn       MU1_IRQn
#define APP_MU_IRQHandler MU1_IRQHandler
/* Channel transmit and receive register */
#define CHN_MU_REG_NUM kMU_MsgReg0

typedef enum {
    MU_CMD_FLASH_IAP_NOTIFY  = 0xA5A50001,  // CM7 -> CM33 notify
    MU_CMD_FLASH_IAP_READY   = 0xA5A50002,  // CM33 -> CM7 ready
    MU_CMD_FLASH_IAP_DONE    = 0xA5A50003,  // CM7 -> CM33 done
} mu_cmd_t;

static bool is_xip_available = true;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

void mu_cm33_init(void)
{
    MU_Init(APP_MU);
    MU_EnableInterrupts(APP_MU, kMU_Rx0FullInterruptEnable);
    NVIC_EnableIRQ(APP_MU_IRQn);
}

__attribute__((section(".ramfunc")))
void cm33_ready_for_flash_iap(void)
{
    /* Disable FlexSPI XIP / AHB buffer */
    FLEXSPI1->MCR0 |= FLEXSPI_MCR0_SWRESET_MASK;
    while (FLEXSPI1->MCR0 & FLEXSPI_MCR0_SWRESET_MASK);

    MU_SendMsgNonBlocking(APP_MU, CHN_MU_REG_NUM, MU_CMD_FLASH_IAP_READY);
    
    is_xip_available = false;
}

void cm33_back_to_flash_xip(void)
{
    //flexspi_nor_init();
    //flexspi_enable_xip();
  
    is_xip_available = true;
}

void APP_MU_IRQHandler(void)
{
    uint32_t flag = 0;
    uint32_t msg = 0;

    flag = MU_GetStatusFlags(APP_MU);
    if ((flag & kMU_Rx0FullFlag) == kMU_Rx0FullFlag)
    {
        msg = MU_ReceiveMsgNonBlocking(APP_MU, CHN_MU_REG_NUM);
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

/*!
 * @brief Main function
 */
int main(void)
{
    /* Initialize MCMGR, install generic event handlers */
    (void)MCMGR_Init();

    /* Init board hardware.*/
    BOARD_InitHardware();

    /* Print the initial banner from Primary core */
    (void)PRINTF("\r\nHello World from the Primary Core!\r\n\n");

#ifdef CORE1_IMAGE_COPY_TO_RAM
    /* This section ensures the secondary core image is copied from flash location to the target RAM memory.
       It consists of several steps: image size calculation, image copying and cache invalidation (optional for some
       platforms/cases). These steps are not required on MCUXpresso IDE which copies the secondary core image to the
       target memory during startup automatically. */
    uint32_t core1_image_size;
    core1_image_size = get_core1_image_size();
    (void)PRINTF("Copy Secondary core image to address: 0x%x, size: %d\r\n", (void *)(char *)CORE1_BOOT_ADDRESS,
                 core1_image_size);

    /* Copy Secondary core application from FLASH to the target memory. */
    (void)memcpy((void *)(char *)CORE1_BOOT_ADDRESS, (void *)CORE1_IMAGE_START, core1_image_size);

#ifdef APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY
    /* Invalidate cache for memory range the secondary core image has been copied to. */
    invalidate_cache_for_core1_image_memory(CORE1_BOOT_ADDRESS, core1_image_size);
#endif /* APP_INVALIDATE_CACHE_FOR_SECONDARY_CORE_IMAGE_MEMORY */
#endif /* CORE1_IMAGE_COPY_TO_RAM */

    /* Boot Secondary core application */
    (void)PRINTF("Starting Secondary core.\r\n");
    (void)MCMGR_StartCore(kMCMGR_Core1, (void *)(char *)CORE1_BOOT_ADDRESS, 2, kMCMGR_Start_Synchronous);
    (void)PRINTF("The secondary core application has been started.\r\n");

    for (;;)
    {
    }
}
