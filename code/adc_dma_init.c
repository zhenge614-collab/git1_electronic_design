/*
 * ADC + DMA 四路同步采集初始化
 * 硬件：STM32F103C8T6，ADC1 规则组 4 通道，DMA1 通道 1 循环搬运
 * 通道：PA0 可燃气体 | PA1 CO | PA6 H2S | PA7 O2
 */
#include "stm32f10x.h"

uint16_t ADC_Buffer[4];   /* DMA 目标缓冲区：4 通道原始值 */

void AD_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,  ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);            /* ADC 时钟 = 72MHz/6 = 12MHz */

    /* 4 个通道配置为模拟输入 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);

    /* ADC1：独立模式、连续转换、扫描模式、4 通道、数据右对齐、禁止外部触发 */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 4;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 规则组通道顺序：CH0 -> CH1 -> CH6 -> CH7，采样时间 239.5 周期 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5); // PA0 可燃气体
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_239Cycles5); // PA1 CO
    ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 3, ADC_SampleTime_239Cycles5); // PA6 H2S
    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 4, ADC_SampleTime_239Cycles5); // PA7 O2

    ADC_DMACmd(ADC1, ENABLE);      /* 使能 ADC 的 DMA 请求 */
    ADC_Cmd(ADC1, ENABLE);

    /* ADC 校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*
 * DMA1 通道 1：ADC 规则数据寄存器 -> ADC_Buffer[4]
 * 外设地址固定、存储器地址自增、16-bit、循环模式、高优先级、传输完成中断
 */
void MYDMA_Config(DMA_Channel_TypeDef *DMA_CHx, u32 cpar, u32 cmar, u16 cndtr)
{
    NVIC_InitTypeDef NVIC_InitStruct;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = cpar;                    /* ADC->DR */
    DMA_InitStructure.DMA_MemoryBaseAddr     = cmar;                    /* ADC_Buffer */
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = cndtr;                   /* 4 */
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;       /* 循环模式 */
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);                     /* 传输完成中断 */

    NVIC_InitStruct.NVIC_IRQChannel                   = DMA1_Channel1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 2;
    NVIC_Init(&NVIC_InitStruct);

    DMA_Cmd(DMA1_Channel1, ENABLE);
}
