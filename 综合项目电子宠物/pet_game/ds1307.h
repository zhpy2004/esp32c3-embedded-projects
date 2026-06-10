/**
 * @file    ds1307.h
 * @brief   DS1307 I2C RTC 驱动 (无第三方库, 基于 Wire)
 * @note    I2C 地址: 0x68 (7-bit), 最大时钟 100kHz
 *          BCD 编码, 24 小时制
 */

#ifndef DS1307_H
#define DS1307_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数
 * ============================================================ */
#define DS1307_I2C_ADDR     0x68    /* 7-bit Slave Address */

/* ============================================================
 * 时间结构体
 * ============================================================ */
typedef struct {
    uint8_t year;   /* 0~99 (代表 2000~2099) */
    uint8_t month;  /* 1~12 */
    uint8_t date;   /* 1~31 */
    uint8_t hour;   /* 0~23 (24h 模式) */
    uint8_t min;    /* 0~59 */
    uint8_t sec;    /* 0~59 */
} ds1307_time_t;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化 DS1307 - 清除 CH 位启动振荡器
 * @note   调用前需确保 Wire 已 begin
 *         上电默认 CH=1 (振荡器停止), 必须清零
 */
void ds1307_init(void);

/**
 * @brief  读取当前时间 (24 小时制)
 * @param  t  输出时间结构体
 * @note   I2C START 时内部寄存器同步到缓冲区, 避免读取过程中翻转
 */
void ds1307_read_time(ds1307_time_t *t);

/**
 * @brief  设置时间 (24 小时制)
 * @param  t  输入时间结构体
 * @note   写入秒寄存器会复位分频链, 必须在 1 秒内完成所有寄存器写入
 */
void ds1307_set_time(const ds1307_time_t *t);

/**
 * @brief  检查振荡器是否正在运行
 * @return true=运行中, false=已停止 (CH=1)
 */
bool ds1307_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* DS1307_H */
