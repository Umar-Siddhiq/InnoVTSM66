/****************************************************************************
 * @file     pin_conf.c
 * @version  V0.42
 * @Date     2023/01/18-00:22:54 
 * @brief    NuMicro generated code file
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2016-2023 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

/********************
MCU:M031LE3AE(LQFP48)
Pin Configuration:
Pin1:UART2_TXD
Pin2:UART2_RXD
Pin3:PB.3
Pin4:ADC0_CH2
Pin5:PB.1
Pin6:ADC0_CH0
Pin8:PA.10
Pin9:PA.9
Pin10:PA.8
Pin11:PF.5
Pin12:PF.4
Pin13:UART0_TXD
Pin14:UART0_RXD
Pin15:PA.7
Pin16:PA.6
Pin17:PA.5
Pin18:PA.4
Pin19:PA.3
Pin20:SPI0_CLK
Pin21:SPI0_MISO
Pin22:SPI0_MOSI
Pin23:PF.15
Pin25:ICE_DAT
Pin26:ICE_CLK
Pin27:PC.5
Pin29:PC.3
Pin30:PC.2
Pin31:I2C0_SCL
Pin32:I2C0_SDA
Pin41:PB.15
Pin42:PB.14
Pin43:PB.13
Pin44:PB.12
Pin47:PB.7
Module Configuration:
UART2_RXD(Pin:2)
UART2_TXD(Pin:1)
PB.1(Pin:5)
PB.3(Pin:3)
PB.7(Pin:47)
PB.12(Pin:44)
PB.13(Pin:43)
PB.14(Pin:42)
PB.15(Pin:41)
ADC0_CH0(Pin:6)
ADC0_CH2(Pin:4)
PA.3(Pin:19)
PA.4(Pin:18)
PA.5(Pin:17)
PA.6(Pin:16)
PA.7(Pin:15)
PA.8(Pin:10)
PA.9(Pin:9)
PA.10(Pin:8)
PF.4(Pin:12)
PF.5(Pin:11)
PF.15(Pin:23)
UART0_RXD(Pin:14)
UART0_TXD(Pin:13)
SPI0_CLK(Pin:20)
SPI0_MISO(Pin:21)
SPI0_MOSI(Pin:22)
ICE_CLK(Pin:26)
ICE_DAT(Pin:25)
PC.2(Pin:30)
PC.3(Pin:29)
PC.5(Pin:27)
I2C0_SCL(Pin:31)
I2C0_SDA(Pin:32)
GPIO Configuration:
PA.0:SPI0_MOSI(Pin:22)
PA.1:SPI0_MISO(Pin:21)
PA.2:SPI0_CLK(Pin:20)
PA.3:PA.3(Pin:19)
PA.4:PA.4(Pin:18)
PA.5:PA.5(Pin:17)
PA.6:PA.6(Pin:16)
PA.7:PA.7(Pin:15)
PA.8:PA.8(Pin:10)
PA.9:PA.9(Pin:9)
PA.10:PA.10(Pin:8)
PB.0:ADC0_CH0(Pin:6)
PB.1:PB.1(Pin:5)
PB.2:ADC0_CH2(Pin:4)
PB.3:PB.3(Pin:3)
PB.4:UART2_RXD(Pin:2)
PB.5:UART2_TXD(Pin:1)
PB.7:PB.7(Pin:47)
PB.12:PB.12(Pin:44)
PB.13:PB.13(Pin:43)
PB.14:PB.14(Pin:42)
PB.15:PB.15(Pin:41)
PC.0:I2C0_SDA(Pin:32)
PC.1:I2C0_SCL(Pin:31)
PC.2:PC.2(Pin:30)
PC.3:PC.3(Pin:29)
PC.5:PC.5(Pin:27)
PF.0:ICE_DAT(Pin:25)
PF.1:ICE_CLK(Pin:26)
PF.2:UART0_RXD(Pin:14)
PF.3:UART0_TXD(Pin:13)
PF.4:PF.4(Pin:12)
PF.5:PF.5(Pin:11)
PF.15:PF.15(Pin:23)
********************/

#include "NuCodeGenProj.h"
#include "pin_conf.h"

void NuCodeGenProj_init_adc0(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB2MFP_Msk | SYS_GPB_MFPL_PB0MFP_Msk);
    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB2MFP_ADC0_CH2 | SYS_GPB_MFPL_PB0MFP_ADC0_CH0);

    return;
}

void NuCodeGenProj_deinit_adc0(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB2MFP_Msk | SYS_GPB_MFPL_PB0MFP_Msk);

    return;
}

void NuCodeGenProj_init_i2c0(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC1MFP_Msk | SYS_GPC_MFPL_PC0MFP_Msk);
    SYS->GPC_MFPL |= (SYS_GPC_MFPL_PC1MFP_I2C0_SCL | SYS_GPC_MFPL_PC0MFP_I2C0_SDA);

    return;
}

void NuCodeGenProj_deinit_i2c0(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC1MFP_Msk | SYS_GPC_MFPL_PC0MFP_Msk);

    return;
}

void NuCodeGenProj_init_ice(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk);
    SYS->GPF_MFPL |= (SYS_GPF_MFPL_PF1MFP_ICE_CLK | SYS_GPF_MFPL_PF0MFP_ICE_DAT);

    return;
}

void NuCodeGenProj_deinit_ice(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk);

    return;
}

void NuCodeGenProj_init_pa(void)
{
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA10MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk | SYS_GPA_MFPH_PA8MFP_Msk);
    SYS->GPA_MFPH |= (SYS_GPA_MFPH_PA10MFP_GPIO | SYS_GPA_MFPH_PA9MFP_GPIO | SYS_GPA_MFPH_PA8MFP_GPIO);
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA7MFP_Msk | SYS_GPA_MFPL_PA6MFP_Msk | SYS_GPA_MFPL_PA5MFP_Msk | SYS_GPA_MFPL_PA4MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk);
    SYS->GPA_MFPL |= (SYS_GPA_MFPL_PA7MFP_GPIO | SYS_GPA_MFPL_PA6MFP_GPIO | SYS_GPA_MFPL_PA5MFP_GPIO | SYS_GPA_MFPL_PA4MFP_GPIO | SYS_GPA_MFPL_PA3MFP_GPIO);

    return;
}

void NuCodeGenProj_deinit_pa(void)
{
    SYS->GPA_MFPH &= ~(SYS_GPA_MFPH_PA10MFP_Msk | SYS_GPA_MFPH_PA9MFP_Msk | SYS_GPA_MFPH_PA8MFP_Msk);
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA7MFP_Msk | SYS_GPA_MFPL_PA6MFP_Msk | SYS_GPA_MFPL_PA5MFP_Msk | SYS_GPA_MFPL_PA4MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk);

    return;
}

void NuCodeGenProj_init_pb(void)
{
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB15MFP_Msk | SYS_GPB_MFPH_PB14MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk | SYS_GPB_MFPH_PB12MFP_Msk);
    SYS->GPB_MFPH |= (SYS_GPB_MFPH_PB15MFP_GPIO | SYS_GPB_MFPH_PB14MFP_GPIO | SYS_GPB_MFPH_PB13MFP_GPIO | SYS_GPB_MFPH_PB12MFP_GPIO);
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB7MFP_Msk | SYS_GPB_MFPL_PB3MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk);
    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB7MFP_GPIO | SYS_GPB_MFPL_PB3MFP_GPIO | SYS_GPB_MFPL_PB1MFP_GPIO);

    return;
}

void NuCodeGenProj_deinit_pb(void)
{
    SYS->GPB_MFPH &= ~(SYS_GPB_MFPH_PB15MFP_Msk | SYS_GPB_MFPH_PB14MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk | SYS_GPB_MFPH_PB12MFP_Msk);
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB7MFP_Msk | SYS_GPB_MFPL_PB3MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk);

    return;
}

void NuCodeGenProj_init_pc(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC3MFP_Msk | SYS_GPC_MFPL_PC2MFP_Msk);
    SYS->GPC_MFPL |= (SYS_GPC_MFPL_PC5MFP_GPIO | SYS_GPC_MFPL_PC3MFP_GPIO | SYS_GPC_MFPL_PC2MFP_GPIO);

    return;
}

void NuCodeGenProj_deinit_pc(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC3MFP_Msk | SYS_GPC_MFPL_PC2MFP_Msk);

    return;
}

void NuCodeGenProj_init_pf(void)
{
    SYS->GPF_MFPH &= ~(SYS_GPF_MFPH_PF15MFP_Msk);
    SYS->GPF_MFPH |= (SYS_GPF_MFPH_PF15MFP_GPIO);
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF5MFP_Msk | SYS_GPF_MFPL_PF4MFP_Msk);
    SYS->GPF_MFPL |= (SYS_GPF_MFPL_PF5MFP_GPIO | SYS_GPF_MFPL_PF4MFP_GPIO);

    return;
}

void NuCodeGenProj_deinit_pf(void)
{
    SYS->GPF_MFPH &= ~(SYS_GPF_MFPH_PF15MFP_Msk);
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF5MFP_Msk | SYS_GPF_MFPL_PF4MFP_Msk);

    return;
}

void NuCodeGenProj_init_spi0(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA2MFP_Msk | SYS_GPA_MFPL_PA1MFP_Msk | SYS_GPA_MFPL_PA0MFP_Msk);
    SYS->GPA_MFPL |= (SYS_GPA_MFPL_PA2MFP_SPI0_CLK | SYS_GPA_MFPL_PA1MFP_SPI0_MISO | SYS_GPA_MFPL_PA0MFP_SPI0_MOSI);

    return;
}

void NuCodeGenProj_deinit_spi0(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA2MFP_Msk | SYS_GPA_MFPL_PA1MFP_Msk | SYS_GPA_MFPL_PA0MFP_Msk);

    return;
}

void NuCodeGenProj_init_uart0(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF3MFP_Msk | SYS_GPF_MFPL_PF2MFP_Msk);
    SYS->GPF_MFPL |= (SYS_GPF_MFPL_PF3MFP_UART0_TXD | SYS_GPF_MFPL_PF2MFP_UART0_RXD);

    return;
}

void NuCodeGenProj_deinit_uart0(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF3MFP_Msk | SYS_GPF_MFPL_PF2MFP_Msk);

    return;
}

void NuCodeGenProj_init_uart2(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk);
    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB5MFP_UART2_TXD | SYS_GPB_MFPL_PB4MFP_UART2_RXD);

    return;
}

void NuCodeGenProj_deinit_uart2(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk);

    return;
}

void Pin_Init(void)
{
    //SYS->GPA_MFPH = 0x00000000UL;
    //SYS->GPA_MFPL = 0x00000444UL;
    //SYS->GPB_MFPH = 0x00000000UL;
    //SYS->GPB_MFPL = 0x00DD0101UL;
    //SYS->GPC_MFPH = 0x00000000UL;
    //SYS->GPC_MFPL = 0x00000099UL;
    //SYS->GPF_MFPH = 0x00000000UL;
    //SYS->GPF_MFPL = 0x000033EEUL;

    NuCodeGenProj_init_adc0();
    NuCodeGenProj_init_i2c0();
    NuCodeGenProj_init_ice();
    NuCodeGenProj_init_pa();
    NuCodeGenProj_init_pb();
    NuCodeGenProj_init_pc();
    NuCodeGenProj_init_pf();
    NuCodeGenProj_init_spi0();
    NuCodeGenProj_init_uart0();
    NuCodeGenProj_init_uart2();

    return;
}

void Pin_Deinit(void)
{
    NuCodeGenProj_deinit_adc0();
    NuCodeGenProj_deinit_i2c0();
    NuCodeGenProj_deinit_ice();
    NuCodeGenProj_deinit_pa();
    NuCodeGenProj_deinit_pb();
    NuCodeGenProj_deinit_pc();
    NuCodeGenProj_deinit_pf();
    NuCodeGenProj_deinit_spi0();
    NuCodeGenProj_deinit_uart0();
    NuCodeGenProj_deinit_uart2();

    return;
}

/*** (C) COPYRIGHT 2016-2023 Nuvoton Technology Corp. ***/
