
#include "Hardware.h"
#include <stdbool.h>
#include <string.h>

void SYS_Init(void)
{
	/* Unlock protected registers */
	//SYS_UnlockReg();

	/* Enable HIRC clock (Internal RC 48MHz) */
	CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);

	/* Wait for HIRC clock ready */
	CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

	/* Select HCLK clock source as HIRC and HCLK source divider as 1 */
	CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

	/* Enable UART0 clock */
	CLK_EnableModuleClock(UART0_MODULE);

	/* Enable peripheral clocks */
	CLK_EnableModuleClock(SPI0_MODULE);
	CLK_SetModuleClock(SPI0_MODULE, CLK_CLKSEL2_SPI0SEL_PCLK1, MODULE_NoMsk);

	/* Switch UART0 clock source to HIRC */
	CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

	/* Update System Core Clock */
	SystemCoreClockUpdate();

	
	/* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
	SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk))    |       \
									(SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);
	/* Configure multi-function pins */
		SYS->GPA_MFPH = SYS_GPA_MFPH_PA11MFP_GPIO | SYS_GPA_MFPH_PA9MFP_GPIO | SYS_GPA_MFPH_PA8MFP_GPIO;
		SYS->GPA_MFPL = SYS_GPA_MFPL_PA3MFP_GPIO | SYS_GPA_MFPL_PA2MFP_SPI0_CLK | SYS_GPA_MFPL_PA1MFP_SPI0_MISO | SYS_GPA_MFPL_PA0MFP_SPI0_MOSI;
		SYS->GPB_MFPH = SYS_GPB_MFPH_PB14MFP_ADC0_CH14 | SYS_GPB_MFPH_PB13MFP_ADC0_CH13;
		SYS->GPB_MFPL = SYS_GPB_MFPL_PB7MFP_GPIO | SYS_GPB_MFPL_PB5MFP_UART2_TXD | SYS_GPB_MFPL_PB4MFP_UART2_RXD | SYS_GPB_MFPL_PB3MFP_UART1_TXD | SYS_GPB_MFPL_PB2MFP_UART1_RXD | SYS_GPB_MFPL_PB1MFP_ADC0_CH1 | SYS_GPB_MFPL_PB0MFP_ADC0_CH0;
		SYS->GPC_MFPH = 0x00000000;
		SYS->GPC_MFPL = SYS_GPC_MFPL_PC4MFP_GPIO;
		SYS->GPF_MFPH = SYS_GPF_MFPH_PF15MFP_GPIO;
		SYS->GPF_MFPL = SYS_GPF_MFPL_PF5MFP_GPIO | SYS_GPF_MFPL_PF4MFP_GPIO | SYS_GPF_MFPL_PF3MFP_UART0_TXD | SYS_GPF_MFPL_PF2MFP_UART0_RXD | SYS_GPF_MFPL_PF1MFP_ICE_CLK | SYS_GPF_MFPL_PF0MFP_ICE_DAT;
	CLK_EnableModuleClock(UART0_MODULE);
	CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_PCLK0, CLK_CLKDIV0_UART0(1));
	//SendString("SYS: UART0 clock enabled\r\n");

	CLK_EnableModuleClock(UART1_MODULE);
	CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART1SEL_PCLK1, CLK_CLKDIV0_UART1(1));
	//SendString("SYS: UART1 clock enabled\r\n");

	CLK_EnableModuleClock(UART2_MODULE);
	CLK_SetModuleClock(UART2_MODULE, CLK_CLKSEL3_UART2SEL_PCLK0, CLK_CLKDIV4_UART2(1));
	//SendString("SYS: UART2 clock enabled\r\n");

	CLK_EnableSysTick(CLK_CLKSEL0_STCLKSEL_HCLK, CLK_GetHCLKFreq() / 1000);
	/* Lock protected registers */
 // SYS_LockReg();
}

void RS232Init(void)
{
	SYS_ResetModule(RS232_UART_SYS_RST);
	UART_Open(RS232_UART, RS232_UART_BAUD);
	NVIC_EnableIRQ(RS232_UART_IRQ);
	UART_EnableInt(RS232_UART, (UART_INTEN_RDAIEN_Msk));
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
	
void RS232SendString(char* src)
{
	RS232SendData((uint8_t*)src,strlen(src));
}