/*
 * ESP8266 物联网通信：AT 指令接入 WiFi + MQTT 上云
 *
 * 注意：以下连接参数均为占位符，请替换为你自己的配置。
 *   - WIFI_SSID / WIFI_PASSWORD   : 路由器热点账号密码
 *   - MQTT_PRODUCT_KEY            : 云平台产品 ProductKey
 *   - MQTT_DEVICE_NAME            : 设备名
 *   - MQTT_CLIENT_ID / USERNAME / PASSWORD / HOST : 由平台三元组生成
 */
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

/* ---- 占位符：部署时替换 ---- */
static const char *WIFI_SSID     = "YOUR_WIFI_SSID";
static const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

static const char *MQTT_PRODUCT_KEY = "YOUR_PRODUCT_KEY";
static const char *MQTT_DEVICE_NAME = "YOUR_DEVICE_NAME";

static const char *MQTT_CLIENT_ID =
    "YOUR_PRODUCT_KEY.YOUR_DEVICE_NAME|securemode=2,signmethod=hmacsha256,timestamp=<TS>|";
static const char *MQTT_USERNAME = "YOUR_DEVICE_NAME&YOUR_PRODUCT_KEY";
static const char *MQTT_PASSWORD = "YOUR_DEVICE_SECRET";

/* 云平台 MQTT 接入域名（请按实际地域替换） */
static const char *MQTT_HOST = "YOUR_PRODUCT_KEY.iot-as-mqtt.cn-shanghai.aliyuncs.com";

/* 物模型属性标识符 / 发布订阅 Topic（请按平台定义替换） */
static const char *propKey_H2S  = "H2S";
static const char *propKey_CO   = "CO";
static const char *propKey_O2   = "O2Content";
static const char *propKey_GAS  = "CombustibleGasCheck";
static const char *propKey_LON  = "Longitude";
static const char *propKey_LAT  = "Latitude";

static const char *topic_pub =
    "/sys/YOUR_PRODUCT_KEY/YOUR_DEVICE_NAME/thing/event/property/post";
static const char *topic_sub =
    "/sys/YOUR_PRODUCT_KEY/YOUR_DEVICE_NAME/thing/event/property/post_reply";

extern char RECS[];   /* USART1 接收缓冲区 */

/*
 * 初始化：复位 -> 关回显 -> Station 模式 -> 连热点 -> MQTT 用户配置 -> 连接 -> 订阅
 * 注：生产环境应检查每一步 RECS 中的 "OK" 应答；此处为调通流程而简化。
 */
char esp_Init(void)
{
    printf("AT+RST\r\n");
    Delay_ms(3000);                              /* 等待模块启动 */

    printf("ATE0\r\n");                          /* 关闭回显 */
    Delay_ms(100);

    printf("AT+CWMODE=1\r\n");                   /* Station 模式 */
    Delay_ms(500);

    printf("AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    Delay_ms(15000);                             /* 等待 WiFi 连接 */

    printf("AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
           MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    Delay_ms(500);

    printf("AT+MQTTCONN=0,\"%s\",1883,1\r\n", MQTT_HOST);
    Delay_ms(2000);

    printf("AT+MQTTSUB=0,\"%s\",1\r\n", topic_sub);
    Delay_ms(1000);

    return 0;
}

/*
 * 发布数据：把 4 路气体浓度与经纬度打包成 JSON 载荷，经 MQTT 上报
 * AD_flagls 由采集流程置位，避免重复发送
 */
char Esp_PUB(void)
{
    memset(RECS, 0, sizeof(RECS));
    if (AD_flagls == 1) {
        AD_flagls = 0;

        /* 气体浓度（H2S / CO / O2 / 可燃气体） */
        printf("AT+MQTTPUB=0,\"%s\",\"{\\\"method\\\":\\\"thing.event.property.post\\\"\\,"
               "\\\"params\\\":{\\\"%s\\\":%d\\,\\\"%s\\\":%lf\\,\\\"%s\\\":%.1f\\,\\\"%s\\\":%.1f}}\",0,0\r\n",
               topic_pub,
               propKey_H2S, dat_send.hydrothion,
               propKey_CO,  dat_send.carbon_monoxide,
               propKey_O2,  dat_send.oxygen,
               propKey_GAS, dat_send.combustible_gas);

        /* 经纬度坐标 */
        printf("AT+MQTTPUB=0,\"%s\",\"{\\\"method\\\":\\\"thing.event.property.post\\\"\\,"
               "\\\"params\\\":{\\\"%s\\\":%lf\\,\\\"%s\\\":%lf}}\",0,0\r\n",
               topic_pub, propKey_LON, longitude_end, propKey_LAT, latitude_end);
    }
    if (strcmp(RECS, "ERROR") == 0)
        return 1;
    return 0;
}
