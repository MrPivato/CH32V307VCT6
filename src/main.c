#include <ch32v30x.h>

#include <debug.h>
#include "stdio.h"
#include "stdbool.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#define ADC_READ_TO_VOLTS (3.3f / 4095.0f)

uint16_t TxBuf[1024];
int16_t Calibrattion_Val = 0;

void ADC_Function_Init(void)
{
	ADC_InitTypeDef ADC_InitStructure = {0};
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	ADC_DeInit(ADC1);
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);

	ADC_BufferCmd(ADC1, DISABLE); // disable buffer
	ADC_ResetCalibration(ADC1);
	while (ADC_GetResetCalibrationStatus(ADC1))
		;
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1))
		;
	Calibrattion_Val = Get_CalibrationValue(ADC1);
}

uint16_t Get_ADC_Val(uint8_t ch)
{
	uint16_t val;

	ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_239Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);

	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
		;
	val = ADC_GetConversionValue(ADC1);

	return val;
}

void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr, u16 bufsize)
{
	DMA_InitTypeDef DMA_InitStructure = {0};

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	DMA_DeInit(DMA_CHx);
	DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
	DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = bufsize;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA_CHx, &DMA_InitStructure);
}

uint16_t Get_ConversionVal(int16_t val)
{
	if ((val + Calibrattion_Val) < 0)
		return 0;
	if ((Calibrattion_Val + val) > 4095 || val == 4095)
		return 4095;
	return (val + Calibrattion_Val);
}

float adcReadingToVolts(uint16_t reading)
{
	return (float)(reading * ADC_READ_TO_VOLTS);
}

int main(void)
{
	uint16_t i;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	SystemCoreClockUpdate();
	Delay_Init();
	USART_Printf_Init(115200);
	// printf("SystemClk:%d\r\n",SystemCoreClock);
	// printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );

	ADC_Function_Init();
	printf("CalibrattionValue:%d\n", Calibrattion_Val);

	DMA_Tx_Init(DMA1_Channel1, (u32)&ADC1->RDATAR, (u32)TxBuf, 1024);
	DMA_Cmd(DMA1_Channel1, ENABLE);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	Delay_Ms(50);
	ADC_SoftwareStartConvCmd(ADC1, DISABLE);

	// for(i=0; i<1024; i++)
	// {
	// 	printf( "%04d\r\n", Get_ConversionVal(TxBuf[i]));
	// 	Delay_Ms(10);
	// }
	uint16_t adcVal = 0, adcValCal = 0;
	double volts = 0.0f;

	while (1)
	{
		adcVal = Get_ADC_Val(ADC_Channel_1);
		adcValCal = Get_ConversionVal(adcVal);
		volts = adcReadingToVolts(adcVal);

		printf("| ADC: %d; ADC (Cal): %d |\n", adcVal, adcValCal);
		printf("| Volts: %f |\n", volts);

		Delay_Ms(100);
	}
}

void NMI_Handler(void) {}
void HardFault_Handler(void)
{
	while (1)
	{
	}
}
