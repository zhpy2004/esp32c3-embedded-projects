/**
 * @file    led_matrix.h
 * @brief   16×16 LED 点阵驱动 (双 HT16K33 I2C, 共阴极)
 * @note    使用 Adafruit_LEDBackpack 库 (与已验证代码一致)
 */

#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  初始化 LED 点阵 (双 HT16K33)
 * @param  addr_upper  上半部分 I2C 地址 (0x70)
 * @param  addr_lower  下半部分 I2C 地址 (0x71)
 */
void led_matrix_init(uint8_t addr_upper, uint8_t addr_lower);

/** @brief  设置亮度 (0~15) */
void led_matrix_brightness(uint8_t brightness);

/** @brief  清屏 */
void led_matrix_clear(void);

/**
 * @brief  显示 16×16 点阵数据 (横置横排, 32 字节)
 * @param  data  点阵数据 (来自 GT30L32S4W)
 */
void led_matrix_show_16x16(const uint8_t *data);

/**
 * @brief  启动滚动显示多个汉字
 * @param  gb_codes  GB2312 编码数组
 * @param  num       汉字个数
 */
void led_matrix_start_scroll(const uint16_t *gb_codes, int num);

/** @brief  在 loop 中调用, 更新滚动 (非阻塞) */
void led_matrix_update_scroll(void);

/** @brief  是否正在滚动 */
bool led_matrix_is_scrolling(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_MATRIX_H */
