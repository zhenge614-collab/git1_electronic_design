/*
 * GPS NMEA-0183 报文解析（GNRMC）
 * 接收方式：USART3 中断逐字节接收，'$' 同步帧首，'\n' 判定帧尾；
 * 解析：按逗号分隔提取时间/纬度/南北半球/经度/东西半球，
 *       度分制(DDMM.MMMM) 转换 -> 度制(DD.DDDDD)，并按半球赋予正负。
 */
#include <string.h>
#include <stdlib.h>

double latitude_u,  longitude_u;    /* 度分制原始值 */
double latitude_end, longitude_end; /* 转换后的十进制度 */

/* 度分制 -> 度制，按半球赋符号 */
void Longitude_latitude_conversion(void)
{
    int intger;
    double minutes;

    intger  = latitude_u / 100;                 /* 取整数度 */
    minutes = latitude_u - intger * 100;        /* 剩余分 */
    latitude_end = intger + minutes / 60.0;     /* 度 + 分/60 */

    intger  = longitude_u / 100;
    minutes = longitude_u - intger * 100;
    longitude_end = intger + minutes / 60.0;
}

/*
 * 解析一帧 GNRMC 缓存：以逗号为分隔符依次取字段。
 * 字段：0=语句ID(RMC) 1=UTC时间 2=状态 3=纬度 4=南北 5=经度 6=东西 ...
 */
void GPRMC_decode(void)
{
    char *cur, *next;
    char  valid;
    int   i;

    if (SaveData_d.Fetch_flag_bit != 1)         /* 没有就绪报文 */
        return;

    SaveData_d.Fetch_flag_bit = 0;
    cur = SaveData_d.GPS_Buffer;

    for (i = 0; i < 7; i++) {
        if (i == 0) {
            cur = strstr(SaveData_d.GPS_Buffer, ",");
        } else {
            cur++;
            next = strstr(cur, ",");
            if (next == NULL)
                break;

            switch (i) {
                case 1: memcpy(SaveData_d.UTC_Time,  cur, next - cur); break; /* 时间 */
                case 2: memcpy(&valid,               cur, next - cur); break; /* 有效位 */
                case 3: memcpy(SaveData_d.latitude,  cur, next - cur); break; /* 纬度 */
                case 4: memcpy(SaveData_d.N_S,       cur, next - cur); break; /* 南北半球 */
                case 5: memcpy(SaveData_d.longitude, cur, next - cur); break; /* 经度 */
                case 6: memcpy(SaveData_d.E_W,       cur, next - cur); break; /* 东西半球 */
                default: break;
            }
            cur = next;
        }
    }

    /* 字符串 -> double 后做度分制转换 */
    latitude_u  = atof(SaveData_d.latitude);
    longitude_u = atof(SaveData_d.longitude);
    Longitude_latitude_conversion();
}

/* 清空接收/解析缓冲 */
void clear(void)
{
    SaveData_d.Fetch_flag_bit = 0;
    SaveData_d.isParseData    = 0;
    memset(SaveData_d.E_W,        0, E_W_Length);
    memset(SaveData_d.GPS_Buffer, 0, GPS_Buffer_Length);
    memset(SaveData_d.latitude,   0, latitude_Length);
    memset(SaveData_d.longitude,  0, longitude_Length);
    memset(SaveData_d.N_S,        0, N_S_Length);
    memset(SaveData_d.UTC_Time,   0, UTCTime_Length);
}
