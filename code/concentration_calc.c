/*
 * 浓度解算：把 ADC 原始值转换为实际气体浓度
 *   - 先按 V = raw * 3.3 / 4096 换算电压
 *   - 再减去零点基准 mid_xxx，并按传感器标定系数换算为浓度
 */
#define VREF        3.3f
#define ADC_FULL    4096.0f

extern float mid_O2, mid_GAS, mid_CO, mid_H2S;   /* 零点基准（原始 ADC 值） */

void Concentration_conversion_formula_function(void)
{
    /* 氧气：以空气中 20.5% O2 对应电压为参考点做比例换算 */
    dat_send.oxygen =
        ((((float)dat_send_voltage.oxygen_v * VREF / ADC_FULL) - A0)
         * 20.5f / (mid_O2 * VREF / ADC_FULL - A0));

    /* 可燃气体(LEL)：压差经反相比例放大后按系数换算 */
    dat_send.combustible_gas =
        ((dat_send_voltage.combustible_gas_v - mid_GAS) * VREF / ADC_FULL) / 3.0f * 50.0f;

    /* 一氧化碳：以 H2S 通道电压为源(见硬件映射)，减零点后处理 */
    dat_send.carbon_monoxide =
        (float)(dat_send_voltage.hydrothion_v - mid_CO) * 3.3f / 4096.0f / 3.0f;

    /* 硫化氢 */
    dat_send.hydrothion =
        (float)(dat_send_voltage.hydrothion_v - mid_H2S) * 3.3f / 4096.0f / 3.0f;
}
