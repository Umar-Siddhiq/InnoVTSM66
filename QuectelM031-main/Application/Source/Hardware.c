//*************************************************************************
// 												Hardware declaration
// 								Author : ARMAN
//								Dated  : 1st Jan 2022 
//*************************************************************************

/********************
MCU:M031LE3AE(LQFP48)
Base Clocks:
LIRC:38.4kHz
HIRC:48MHz
PLL:96MHz
HCLK:48MHz
PCLK0:48MHz
PCLK1:48MHz
Enabled-Module Frequencies:
I2C0=Bus Clock(PCLK0):48MHz
ISP=Bus Clock(HCLK):48MHz/Engine Clock:48MHz
SPI0=Bus Clock(PCLK1):48MHz/Engine Clock:48MHz
SYSTICK=Bus Clock(HCLK):48MHz/Engine Clock:24MHz
TMR0=Bus Clock(PCLK0):48MHz/Engine Clock:48MHz
UART0=Bus Clock(PCLK0):48MHz/Engine Clock:48MHz
UART1=Bus Clock(PCLK1):48MHz/Engine Clock:48MHz
UART2=Bus Clock(PCLK0):48MHz/Engine Clock:48MHz
WDT=Bus Clock(PCLK0):48MHz/Engine Clock:38.4kHz
WWDT=Bus Clock(PCLK0):48MHz/Engine Clock:23.4375kHz
********************/


#include "Hardware.h"
#include "MComm.h"
#include "LSM6DSOX.h"
#include <stdbool.h>
#include <stdio.h>



//#define UART_DEBUG
PepheralTypedef PeriPheralVal;
uint8_t IsRTCSet;
uint16_t ADCVal[4];

void SendChar(char ch) {
		while(UART0->FIFOSTS & UART_FIFOSTS_TXFULL_Msk);
		UART0->DAT = ch;
}

void SendString(const char *str) {
	#ifdef UART_DEBUG
		while(*str) SendChar(*str++);
	#endif
}


void SYS_Init(void)
{
		/* Unlock protected registers */
		SYS_UnlockReg();
		SendString("SYS: Registers unlocked\r\n");

		/* Enable HIRC */
		CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk | CLK_PWRCTL_HIRCEN_Msk);
		CLK_DisableXtalRC(CLK_PWRCTL_LXTEN_Msk);
		SendString("SYS: HIRC + LIRC enabled, LXT disabled\r\n");

		/* Waiting for HIRC clock ready */
		CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk | CLK_STATUS_HIRCSTB_Msk);
		SendString("SYS: HIRC & LIRC stable\r\n");

		// PLL Setting --------
		CLK_DisablePLL();
		CLK->PLLCTL = (CLK->PLLCTL & ~(0x000FFFFFUL)) | 0x0008C03EUL;
		CLK_WaitClockReady(CLK_STATUS_PLLSTB_Msk);
		SendString("SYS: PLL configured and stable\r\n");
		// ---------------------

		/* Switch HCLK clock source to HIRC */
		CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));
		SendString("SYS: HCLK switched to HIRC\r\n");

		/* Set both PCLK0 and PCLK1 as HCLK/1 */
		CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV1 | CLK_PCLKDIV_APB1DIV_DIV1);
		SendString("SYS: PCLK0 & PCLK1 set\r\n");

		/* Enable peripheral clocks */
		CLK_EnableModuleClock(SPI0_MODULE);
		CLK_SetModuleClock(SPI0_MODULE, CLK_CLKSEL2_SPI0SEL_PCLK1, MODULE_NoMsk);
		SendString("SYS: SPI0 clock enabled\r\n");

		CLK_EnableSysTick(CLK_CLKSEL0_STCLKSEL_HCLK, CLK_GetHCLKFreq() / 1000);
		SendString("SYS: SysTick enabled\r\n");

		CLK_EnableModuleClock(UART0_MODULE);
		CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_PCLK0, CLK_CLKDIV0_UART0(1));
		SendString("SYS: UART0 clock enabled\r\n");

		CLK_EnableModuleClock(UART1_MODULE);
		CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_PCLK1, CLK_CLKDIV0_UART1(1));
		SendString("SYS: UART1 clock enabled\r\n");

		CLK_EnableModuleClock(UART2_MODULE);
		CLK_SetModuleClock(UART2_MODULE, CLK_CLKSEL3_UART2SEL_PCLK0, CLK_CLKDIV4_UART2(1));
		SendString("SYS: UART2 clock enabled\r\n");

		CLK_EnableModuleClock(ISP_MODULE);
		SendString("SYS: ISP module clock enabled\r\n");

		CLK_EnableModuleClock(ADC_MODULE);
		CLK_SetModuleClock(ADC_MODULE, CLK_CLKSEL2_ADCSEL_PCLK1, CLK_CLKDIV0_ADC(1));
		SendString("SYS: ADC module clock enabled\r\n");

		/* Update System Core Clock */
		SystemCoreClockUpdate();
		SendString("SYS: SystemCoreClock updated\r\n");

		/* Configure multi-function pins */
		SYS->GPA_MFPH = SYS_GPA_MFPH_PA11MFP_GPIO | SYS_GPA_MFPH_PA9MFP_GPIO | SYS_GPA_MFPH_PA8MFP_GPIO;
		SYS->GPA_MFPL = SYS_GPA_MFPL_PA3MFP_GPIO | SYS_GPA_MFPL_PA2MFP_SPI0_CLK | SYS_GPA_MFPL_PA1MFP_SPI0_MISO | SYS_GPA_MFPL_PA0MFP_SPI0_MOSI;
		SYS->GPB_MFPH = SYS_GPB_MFPH_PB14MFP_ADC0_CH14 | SYS_GPB_MFPH_PB13MFP_ADC0_CH13;
		SYS->GPB_MFPL = SYS_GPB_MFPL_PB7MFP_GPIO | SYS_GPB_MFPL_PB5MFP_UART2_TXD | SYS_GPB_MFPL_PB4MFP_UART2_RXD | SYS_GPB_MFPL_PB3MFP_UART1_TXD | SYS_GPB_MFPL_PB2MFP_UART1_RXD | SYS_GPB_MFPL_PB1MFP_ADC0_CH1 | SYS_GPB_MFPL_PB0MFP_ADC0_CH0;
		SYS->GPC_MFPH = 0x00000000;
		SYS->GPC_MFPL = SYS_GPC_MFPL_PC4MFP_GPIO;
		SYS->GPF_MFPH = SYS_GPF_MFPH_PF15MFP_GPIO;
		SYS->GPF_MFPL = SYS_GPF_MFPL_PF5MFP_GPIO | SYS_GPF_MFPL_PF4MFP_GPIO | SYS_GPF_MFPL_PF3MFP_UART0_TXD | SYS_GPF_MFPL_PF2MFP_UART0_RXD | SYS_GPF_MFPL_PF1MFP_ICE_CLK | SYS_GPF_MFPL_PF0MFP_ICE_DAT;
		SendString("SYS: Multi-function pins configured\r\n");

		/* Disable digital path for ADC pins */
		GPIO_DISABLE_DIGITAL_PATH(ADC1_PORT, ADC1_PIN);
		GPIO_DISABLE_DIGITAL_PATH(ADC2_PORT, ADC2_PIN);
		GPIO_DISABLE_DIGITAL_PATH(VMAIN_PORT, VMAIN_PIN);
		GPIO_DISABLE_DIGITAL_PATH(VSEN_PORT, VSEN_PIN);
		SendString("SYS: ADC digital paths disabled\r\n");

		/* Lock protected registers */
		SYS_LockReg();
		SendString("SYS: Registers locked, SYS_Init complete\r\n");
}


uint8_t RS485RxBuff[RS485_RX_BUFF_SIZE];
volatile uint16_t RS485RxCount = 0;
volatile bool RS485RxActive = false;
volatile uint16_t RS485RxTimeout =0;

uint8_t RS232RxBuff[RS232_RX_BUFF_SIZE];
volatile uint16_t RS232RxCount = 0;
volatile bool RS232RxActive = false;
volatile uint16_t RS232RxTimeout =0;

static void IOInit(void)
{
	GPIO_SetMode(IP1_PORT, IP1_PIN, GPIO_MODE_INPUT);
	GPIO_SetMode(IP2_PORT, IP2_PIN, GPIO_MODE_INPUT);
}


void UART13_IRQHandler(void)
{
	uint8_t tmp = 0xFF;
	if (UART_GET_INT_FLAG(RS232_UART, UART_INTSTS_RDAINT_Msk))
		{
				while (UART_IS_RX_READY(RS232_UART))
				{
						tmp = UART_READ(RS232_UART);
						RS232_RxByteHandler(tmp);  
				}
		}
}

static void ADCInit(void)
{
	ADC_POWER_ON(ADC);
	ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_CONTINUOUS, ADC1_PIN|ADC2_PIN|VMAIN_PIN|VSEN_PIN);
	ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
	
}

void UpdateADC(void)
{
	uint16_t i;
	ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
	ADC_START_CONV(ADC);
	while (ADC_GET_INT_FLAG(ADC, ADC_ADF_INT)==0);
	ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT); 

	ADCVal[0]=ADC_GET_CONVERSION_DATA(ADC, 1);
	ADCVal[1]=ADC_GET_CONVERSION_DATA(ADC, 0);
	ADCVal[2]=ADC_GET_CONVERSION_DATA(ADC, 13);
	ADCVal[3]=ADC_GET_CONVERSION_DATA(ADC, 14);

	ADC_STOP_CONV(ADC);
}

void RS485Init(void)
{
	SYS_ResetModule(RS485_UART_SYS_RST);
	UART_Open(RS485_UART, RS485_UART_BAUD);
	NVIC_EnableIRQ(RS485_UART_IRQ);
	UART_EnableInt(RS485_UART, (UART_INTEN_RDAIEN_Msk));
	GPIO_SetMode(RS485_DIR_PORT, RS485_DIR_PIN, GPIO_MODE_OUTPUT);
	RS485_DIR_DIS;
}

void RS232Init(void)
{
	SYS_ResetModule(RS232_UART_SYS_RST);
	UART_Open(RS232_UART, RS232_UART_BAUD);
	NVIC_EnableIRQ(RS232_UART_IRQ);
	UART_EnableInt(RS232_UART, (UART_INTEN_RDAIEN_Msk));
}



void RS485_RxByteHandler(uint8_t byte)
{
		if (RS485RxCount < RS485_RX_BUFF_SIZE)
				RS485RxBuff[RS485RxCount++] = byte;

		RS485RxActive = true;
		RS485RxTimeout=10;
}

void RS232_RxByteHandler(uint8_t byte)
{
	if (RS232RxCount < RS232_RX_BUFF_SIZE)
		RS232RxBuff[RS232RxCount++] = byte;

	RS232RxActive = true;
	RS232RxTimeout=10;
}

void RS485SendData(uint8_t *src, size_t len)
{
	RS485_DIR_EN;
	while(len)
	{
		while(UART_IS_TX_FULL(RS485_UART));
		UART_WRITE(RS485_UART,*src);
		src++;
		len--;
	}
	RS485_DIR_DIS;
}

void RS232SendData(uint8_t *src, size_t len)
{
	while(len)
	{
		while(UART_IS_TX_FULL(RS232_UART));
		UART_WRITE(RS232_UART,*src);
		src++;
		len--;
	}
}

void RS232SendString(char* str)
{
	RS232SendData((uint8_t*)str,strlen(str));
}

void PeripheralInit(void)
{
	IOInit();
	ADCInit();
	RS485Init();
	RS232Init();
	LSM6DSOX_Init();
}



void ProcessSerialData(void)
{
	if(RS485RxActive && RS485RxTimeout==0)
	{
		MCOMM_SendSerialData(1, RS485RxBuff, RS485RxCount);
		RS485RxActive = false;
		RS485RxCount=0;
	}
	if(RS232RxActive && RS232RxTimeout==0)
	{
		MCOMM_SendSerialData(0, RS232RxBuff, RS232RxCount);
		RS232RxActive = false;
		RS232RxCount=0;
	}
}


void ProcessPeripheral(void)
{
	UpdateADC();
	PeriPheralVal.IP1=IP1_VAL;
	PeriPheralVal.IP2=IP2_VAL;
	PeriPheralVal.ADCVal1=ADCVal[0];
	PeriPheralVal.ADCVal2=ADCVal[1];
	PeriPheralVal.MainVolt=ADCVal[2];
	PeriPheralVal.BattVolt=ADCVal[3];
	ProcessSerialData();
}



