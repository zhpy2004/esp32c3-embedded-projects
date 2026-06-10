/**
 * @file    dht11.h
 * @brief   DHT11 单总线温湿度传感器驱动 (无第三方库)
 * @note    单总线协议, 40bit 数据 (湿度整数+湿度小数+温度整数+温度小数+校验)
 *          采样间隔需 ≥2 秒, 上电后需等待 ≥1 秒
 *          DATA 引脚需外接 4.7kΩ 上拉电阻
 */

#ifndef DHT11_H
#define DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t hum_int;    /* 湿度整数部分 (%RH) */
    uint8_t hum_dec;    /* 湿度小数部分 (DHT11 恒为 0) */
    uint8_t temp_int;   /* 温度整数部分 (°C) */
    uint8_t temp_dec;   /* 温度小数部分, bit7=1 表示负温度 */
} dht11_raw_t;

/**
 * @brief  初始化 DHT11
 * @param  pin  DATA 引脚号
 * @note   调用后需等待 ≥1 秒再进行首次读取
 */
void dht11_init(uint8_t pin);

/**
 * @brief  读取温湿度 (阻塞式, 约 5~25ms)
 * @param  raw  输出原始数据 (可为 NULL)
 * @return true=读取成功且校验通过, false=失败
 * @note   两次读取间隔需 ≥2 秒
 */
bool dht11_read(dht11_raw_t *raw);

/**
 * @brief  获取最近一次成功读取的温度 (°C)
 * @return 温度值 (含小数, 支持负温度), 如 24.3 或 -10.1
 *         若从未成功读取返回 -999.0
 */
float dht11_get_temp(void);

/**
 * @brief  获取最近一次成功读取的湿度 (%RH)
 * @return 湿度值 (整数), 如 53.0
 *         若从未成功读取返回 -1.0
 */
float dht11_get_humidity(void);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
