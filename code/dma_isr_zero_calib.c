/*
 * DMA 传输完成中断 + 零点校准（中值 + 均值滤波）
 * 每轮 4 通道扫描完成后触发中断，将原始值送入结构体；
 * 校准模式下累计 5 组(20 个)原始值，排序后去掉最大/最小，取中间 3 个均值作为零点基准。
 */
#include "stm32f10x.h"
#include <stdlib.h>

extern uint16_t ADC_Buffer[4];
extern float    ADC_Buffer_Zero[5 * 4];

#define ADC_ZERO_GROUPS 5

volatile u8  zero_flag  = 0;   /* 1 = 正在采集零点 */
volatile u8  zero_index = 0;   /* 已采集组数 */

float mid_GAS, mid_CO, mid_H2S, mid_O2;   /* 各通道零点基准（原始 ADC 值） */

/* 浮点比较：供 qsort 使用 */
int compare(const void *a, const void *b)
{
    return (*(const float *)a > *(const float *)b) - (*(const float *)a < *(const float *)b);
}

/* 零点设置：index 累计到 5 时计算各通道基准 */
void zero_set(u8 index, float *data)
{
    float dat_GAS[ADC_ZERO_GROUPS], dat_CO[ADC_ZERO_GROUPS];
    float dat_H2S[ADC_ZERO_GROUPS], dat_O2[ADC_ZERO_GROUPS];
    u8 x;

    /* 数据每 4 个为一组的同一时刻 4 通道，按通道拆分 */
    for (x = 0; x < ADC_ZERO_GROUPS; x++) {
        dat_GAS[x] = data[4 * x];
        dat_CO[x]  = data[4 * x + 1];
        dat_H2S[x] = data[4 * x + 2];
        dat_O2[x]  = data[4 * x + 3];
    }

    if (index == ADC_ZERO_GROUPS) {
        zero_flag  = 0;   /* 结束本次校准，避免重复运行 */
        zero_index = 0;

        /* 每通道 5 个值排序 */
        qsort(dat_GAS, ADC_ZERO_GROUPS, sizeof(float), compare);
        qsort(dat_CO,  ADC_ZERO_GROUPS, sizeof(float), compare);
        qsort(dat_H2S, ADC_ZERO_GROUPS, sizeof(float), compare);
        qsort(dat_O2,  ADC_ZERO_GROUPS, sizeof(float), compare);

        /* 中值 + 均值：去掉最小[0]、最大[4]，取 [1][2][3] 平均 */
        mid_GAS = (dat_GAS[1] + dat_GAS[2] + dat_GAS[3]) / 3.0;
        mid_CO  = (dat_CO[1]  + dat_CO[2]  + dat_CO[3])  / 3.0;
        mid_H2S = (dat_H2S[1] + dat_H2S[2] + dat_H2S[3]) / 3.0;
        mid_O2  = (dat_O2[1]  + dat_O2[2]  + dat_O2[3])  / 3.0;
    }
}

/* DMA1 通道 1 传输完成中断 */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        DMA_Cmd(DMA1_Channel1, DISABLE);   /* 读取期间暂停搬运 */

        /* 从缓冲区取出 4 通道原始 ADC 值 */
        dat_send_voltage.combustible_gas_v = ADC_Buffer[0];  /* PA0 */
        dat_send_voltage.carbon_monoxide_v = ADC_Buffer[1];  /* PA1 */
        dat_send_voltage.hydrothion_v      = ADC_Buffer[2];  /* PA6 */
        dat_send_voltage.oxygen_v          = ADC_Buffer[3];  /* PA7 */

        if (zero_flag == 1) {              /* 校准采集模式 */
            ADC_Buffer_Zero[4 * zero_index]     = dat_send_voltage.combustible_gas_v;
            ADC_Buffer_Zero[4 * zero_index + 1] = dat_send_voltage.carbon_monoxide_v;
            ADC_Buffer_Zero[4 * zero_index + 2] = dat_send_voltage.hydrothion_v;
            ADC_Buffer_Zero[4 * zero_index + 3] = dat_send_voltage.oxygen_v;
            zero_index++;
            zero_set(zero_index, ADC_Buffer_Zero);
        }

        DMA_Cmd(DMA1_Channel1, ENABLE);
    }
    DMA_ClearITPendingBit(DMA1_IT_TC1);
}
