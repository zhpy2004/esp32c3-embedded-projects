/**
 * @file    buzzer.h
 * @brief   无源蜂鸣器驱动 (ESP32 LEDC, Attach/Detach 模式)
 * @note    适用于 ESP32 Arduino Core 3.x
 *          驱动方式: ledcAttach → ledcWriteTone → ledcDetach
 */

#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数 (使用前按需修改)
 * ============================================================ */
#define BUZZER_PWM_RESOLUTION      8     // PWM 分辨率 8-bit

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化蜂鸣器
 * @param  pin  GPIO 引脚号
 * @note   仅设置 GPIO 为输出低电平，不做 ledcAttach
 *         实际 Attach 在 buzzer_on() 中按需执行
 */
void buzzer_init(uint8_t pin);

/**
 * @brief  以指定频率启动蜂鸣器
 * @param  freq_hz  频率 (Hz), 建议 500~5000
 */
void buzzer_on(uint32_t freq_hz);

/**
 * @brief  关闭蜂鸣器 (Detach LEDC 通道)
 */
void buzzer_off(void);

/**
 * @brief  蜂鸣器是否正在发声
 * @return true=正在响, false=静音
 */
bool buzzer_is_on(void);

/**
 * @brief  间歇鸣叫 (非阻塞, 需在 loop 中反复调用)
 * @param  freq_hz  频率 (Hz)
 * @param  on_ms    鸣叫持续时间 (ms), 0=持续鸣叫
 * @param  off_ms   静音持续时间 (ms), 0=持续鸣叫
 *
 * @example
 *   // 2kHz, 响150ms 停200ms
 *   buzzer_beep(2000, 150, 200);
 *
 *   // 4kHz, 持续鸣叫
 *   buzzer_beep(4000, 0, 0);
 */
void buzzer_beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms);

/**
 * @brief  阻塞式鸣叫 (发出一声后自动停止)
 * @param  freq_hz      频率 (Hz)
 * @param  duration_ms  持续时间 (ms)
 * @note   会阻塞当前线程，不适合在需要实时响应的 loop 中使用
 */
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
