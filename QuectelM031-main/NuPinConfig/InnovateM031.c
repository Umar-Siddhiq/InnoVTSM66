/****************************************************************************
 * @file     InnovateM031.c
 * @version  v1.34.0001
 * @Date     Wed Apr 09 2025 19:33:58 GMT+0530 (India Standard Time)
 * @brief    NuMicro generated code file
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2013-2025 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

/********************
MCU:M031LE3AE(LQFP48)
********************/

#include "M031Series.h"

/*
 * @brief This function provides the configured MFP registers
 * @param None
 * @return None
 */
void SYS_Init(void)
{
    //SYS->GPA_MFPH = 0x00000000UL;
    //SYS->GPA_MFPL = 0x00000444UL;
    //SYS->GPB_MFPH = 0x01100000UL;
    //SYS->GPB_MFPL = 0x00DD6611UL;
    //SYS->GPC_MFPH = 0x00000000UL;
    //SYS->GPC_MFPL = 0x00000000UL;
    //SYS->GPF_MFPH = 0x00000000UL;
    //SYS->GPF_MFPL = 0x000033EEUL;

    /* If the macros do not exist in your project, please refer to the corresponding header file in Header folder of the tool package */
    SYS->GPA_MFPH = SYS_GPA_MFPH_PA11MFP_GPIO | SYS_GPA_MFPH_PA9MFP_GPIO | SYS_GPA_MFPH_PA8MFP_GPIO;
    SYS->GPA_MFPL = SYS_GPA_MFPL_PA3MFP_GPIO | SYS_GPA_MFPL_PA2MFP_SPI0_CLK | SYS_GPA_MFPL_PA1MFP_SPI0_MISO | SYS_GPA_MFPL_PA0MFP_SPI0_MOSI;
    SYS->GPB_MFPH = SYS_GPB_MFPH_PB14MFP_ADC0_CH14 | SYS_GPB_MFPH_PB13MFP_ADC0_CH13;
    SYS->GPB_MFPL = SYS_GPB_MFPL_PB7MFP_GPIO | SYS_GPB_MFPL_PB5MFP_UART2_TXD | SYS_GPB_MFPL_PB4MFP_UART2_RXD | SYS_GPB_MFPL_PB3MFP_UART1_TXD | SYS_GPB_MFPL_PB2MFP_UART1_RXD | SYS_GPB_MFPL_PB1MFP_ADC0_CH1 | SYS_GPB_MFPL_PB0MFP_ADC0_CH0;
    SYS->GPC_MFPH = 0x00000000;
    SYS->GPC_MFPL = SYS_GPC_MFPL_PC4MFP_GPIO;
    SYS->GPF_MFPH = SYS_GPF_MFPH_PF15MFP_GPIO;
    SYS->GPF_MFPL = SYS_GPF_MFPL_PF5MFP_GPIO | SYS_GPF_MFPL_PF4MFP_GPIO | SYS_GPF_MFPL_PF3MFP_UART0_TXD | SYS_GPF_MFPL_PF2MFP_UART0_RXD | SYS_GPF_MFPL_PF1MFP_ICE_CLK | SYS_GPF_MFPL_PF0MFP_ICE_DAT;

    return;
}

/*** (C) COPYRIGHT 2013-2025 Nuvoton Technology Corp. ***/
