/**
 * @file    bh1750.h
 * @brief   BH1750FVI I2C 环境光传感器驱动 (无第三方库, 基于 Wire)
 * @note    I2C 地址: 0x23 (ADDR=LOW) 或 0x5C (ADDR=HIGH)
 *          分辨率 1 lx, 测量时间 typ.120ms / max.180ms
 *          使用 One Time H-Resolution Mode, 测量完成后自动 Power Down
 */

#ifndef BH1750_H
#define BH1750_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数
 * ============================================================ */
#define BH1750_ADDR_LOW     0x23    /* ADDR pin = LOW  */
#define BH1750_ADDR_HIGH    0x5C    /* ADDR pin = HIGH */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化 BH1750 (发送 Power On 后立即 Power Down)
 * @param  addr  I2C 从机地址 (0x23 或 0x5C)
 * @note   调用前需确保 Wire 已 begin
 */
void bh1750_init(uint8_t addr);

/**
 * @brief  触发一次高分辨率测量 (非阻塞)
 * @note   调用后需等待 ≥180ms 再调用 bh1750_read_lux() 读取结果
 */
void bh1750_trigger(void);

/**
 * @brief  读取最近一次测量结果
 * @return 光照强度 (lux), 范围 0~65535; 失败返回 -1
 * @note   需在 bh1750_trigger() 后 ≥180ms 调用
 */
int32_t bh1750_read_lux(void);

/**
 * @brief  阻塞式单次测量 (触发 + 等待 + 读取)
 * @return 光照强度 (lux), 失败返回 -1
 * @note   内部 delay(180), 会阻塞约 180ms
 */
int32_t bh1750_read_lux_blocking(void);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */
