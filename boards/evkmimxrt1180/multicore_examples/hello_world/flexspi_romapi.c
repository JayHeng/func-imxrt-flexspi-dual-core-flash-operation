/*
 * Copyright 2020 - 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_romapi.h"
#include "fsl_debug_console.h"
#include "fsl_cache.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define FlexSpiInstance           1U
#define EXAMPLE_FLEXSPI_AMBA_BASE FlexSPI1_AMBA_BASE
#define FLASH_SIZE                0x1000000UL /* 16MBytes */
#define FLASH_PAGE_SIZE           256UL       /* 256Bytes */
#define FLASH_SECTOR_SIZE         0x1000UL    /* 4KBytes */
#define FLASH_BLOCK_SIZE          0x10000UL   /* 64KBytes */

#define BUFFER_LEN FLASH_PAGE_SIZE

/*******************************************************************************
 * Prototypes
 ******************************************************************************/


/*******************************************************************************
 * Variables
 ******************************************************************************/
#define FLASH_DUMMY_CYCLES 0x06
/*! @brief FLEXSPI NOR flash driver Structure */
static flexspi_nor_config_t norConfig;
/*! @brief Buffer for program */
static uint8_t s_buffer[BUFFER_LEN];
/*! @brief Buffer for readback */
static uint8_t s_buffer_rbc[BUFFER_LEN];

/*******************************************************************************
 * Code
 ******************************************************************************/

#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1)                                                              \
    (FLEXSPI_LUT_OPERAND0(op0) | FLEXSPI_LUT_NUM_PADS0(pad0) | FLEXSPI_LUT_OPCODE0(cmd0) | FLEXSPI_LUT_OPERAND1(op1) | \
     FLEXSPI_LUT_NUM_PADS1(pad1) | FLEXSPI_LUT_OPCODE1(cmd1))

/* These settings are related to board design for external QSPI flash memory, please adjust it accordingly based on your design */
status_t FLEXSPI_NorFlash_GetConfig(uint32_t instance,
                                           flexspi_nor_config_t *config)
{  
    config->memConfig.tag = 0x42464346UL;
    config->memConfig.version = 0x56010400UL;
    config->memConfig.readSampleClkSrc = kFLEXSPIReadSampleClk_LoopbackFromDqsPad;
    config->memConfig.csHoldTime = 3;
    config->memConfig.csSetupTime = 3;
    config->memConfig.controllerMiscOption = 0x10;
    config->memConfig.deviceType = kFLEXSPIDeviceType_SerialNOR;
    config->memConfig.sflashPadType = kSerialFlash_4Pads;
    config->memConfig.serialClkFreq = kFLEXSPISerialClk_133MHz;
    config->memConfig.sflashA1Size = 16u * 1024u * 1024u,
    config->memConfig.configModeType[0] = kDeviceConfigCmdType_Generic,
 
    config->memConfig.lookupTable[0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0xEB, RADDR_SDR, FLEXSPI_4PAD, 0x18);
    config->memConfig.lookupTable[1] = FLEXSPI_LUT_SEQ(DUMMY_SDR, FLEXSPI_4PAD, FLASH_DUMMY_CYCLES, READ_SDR, FLEXSPI_4PAD, 0x04);
    config->memConfig.lookupTable[4 * 1 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x05, READ_SDR, FLEXSPI_1PAD, 0x04);
    config->memConfig.lookupTable[4 * 3 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x06, STOP, FLEXSPI_1PAD, 0x0);    
    config->memConfig.lookupTable[4 * 5 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x20, RADDR_SDR, FLEXSPI_1PAD, 0x18);    
    config->memConfig.lookupTable[4 * 8 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0xD8, RADDR_SDR, FLEXSPI_1PAD, 0x18);
    config->memConfig.lookupTable[4 * 9 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x02, RADDR_SDR, FLEXSPI_1PAD, 0x18);
    config->memConfig.lookupTable[4 * 9 + 1] = FLEXSPI_LUT_SEQ(WRITE_SDR, FLEXSPI_1PAD, 0x04, STOP, FLEXSPI_1PAD, 0x0);    
    config->memConfig.lookupTable[4 * 11 + 0] = FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x60, STOP, FLEXSPI_1PAD, 0x0);       

    config->pageSize = 256u;
    config->sectorSize = 4u * 1024u;
    config->blockSize = 64u * 1024u;
    config->ipcmdSerialClkFreq = 0x1;
    config->isUniformBlockSize = false;
 
    return kStatus_Success;
}

int iap_main(void)
{
    status_t status;
    uint32_t i        = 0U;
    uint32_t serialNorAddress;        /* Address of the serial nor device location */
    uint32_t FlexSPISerialNorAddress; /* Address of the serial nor device in FLEXSPI memory */
    uint32_t serialNorTotalSize;
    uint32_t serialNorSectorSize;

    ROM_API_Init();

    /* Clean up FLEXSPI NOR flash driver Structure */
    memset(&norConfig, 0U, sizeof(flexspi_nor_config_t));

#if (__CORTEX_M == 7U)
    /* Disable I cache */
    SCB_DisableICache();
#endif

    /* Setup FLEXSPI NOR Configuration Block */
    status = FLEXSPI_NorFlash_GetConfig(FlexSpiInstance, &norConfig);
    if (status != kStatus_Success)
    {
        return -1;
    }

    /* Initializes the FLEXSPI module for the other FLEXSPI APIs */
    status = ROM_FLEXSPI_NorFlash_Init(FlexSpiInstance, &norConfig);
    if (status != kStatus_Success)
    {
        return -1;
    }

    /* Perform software reset after initializing flexspi module */
    ROM_FLEXSPI_NorFlash_ClearCache(FlexSpiInstance);

    serialNorTotalSize  = norConfig.memConfig.sflashA1Size;
    serialNorSectorSize = norConfig.sectorSize;

#if (__CORTEX_M == 7U)
    /* Enable I cache */
    SCB_EnableICache();
#endif

/*
 * SECTOR_INDEX_FROM_END = 1 means the last sector,
 * SECTOR_INDEX_FROM_END = 2 means (the last sector - 1) ...
 */
#ifndef SECTOR_INDEX_FROM_END
#define SECTOR_INDEX_FROM_END 1U
#endif
    /* Erase a sector from target device dest address */
    serialNorAddress        = serialNorTotalSize - (SECTOR_INDEX_FROM_END * serialNorSectorSize);
    FlexSPISerialNorAddress = EXAMPLE_FLEXSPI_AMBA_BASE + serialNorAddress;
    
    uint8_t pattern = *(uint8_t *)FlexSPISerialNorAddress;

    /* Erase one sector. */
    status = ROM_FLEXSPI_NorFlash_Erase(FlexSpiInstance, &norConfig, serialNorAddress, serialNorSectorSize);
    if (status != kStatus_Success)
    {
        return -1;
    }

    /* Prepare user buffer. */
    for (i = 0; i < BUFFER_LEN; i++)
    {
        s_buffer[i] = pattern + 1;
    }

    /* Program user buffer into FLEXSPI NOR flash */
    status =
        ROM_FLEXSPI_NorFlash_ProgramPage(FlexSpiInstance, &norConfig, serialNorAddress, (const uint32_t *)s_buffer);
    if (status != kStatus_Success)
    {
        return -1;
    }

    DCACHE_InvalidateByRange(FlexSPISerialNorAddress, sizeof(s_buffer_rbc));
    /* Verify programming by reading back from FLEXSPI memory directly */
    memcpy(s_buffer_rbc, (void *)(FlexSPISerialNorAddress), sizeof(s_buffer_rbc));
    if (memcmp(s_buffer_rbc, s_buffer, sizeof(s_buffer)) != 0)
    {
        return -1;
    }

    return 0;
}
