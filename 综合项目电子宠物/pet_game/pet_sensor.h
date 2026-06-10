/**
 * @file    pet_sensor.h
 * @brief   传感器综合管理 — 定时采样 / 环境更新 / 事件检测
 */

#ifndef PET_SENSOR_H
#define PET_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 事件标志 (sensor_poll 返回的位掩码)
 * ============================================================ */
#define EVT_KEY_PRESS    (1 << 0)   /* TTP229 新按键 */
#define EVT_SHAKE        (1 << 1)   /* MPU6050 摇晃 */
#define EVT_DECAY        (1 << 3)   /* 衰减 tick (60s) */
#define EVT_AUTO_SAVE    (1 << 4)   /* 自动存档 tick (90s) */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化所有传感器 (TTP229/MPU6050/BH1750/DHT11)
 * @note   调用前需确保 Wire 已 begin
 */
void sensor_init(void);

/**
 * @brief  轮询所有传感器, 按各自周期采样, 更新 env_data_t
 * @return 事件位掩码 (EVT_xxx 的组合)
 * @note   在 loop() 中每次调用, 内部自行管理定时
 */
uint16_t sensor_poll(void);

/**
 * @brief  获取最近一次按键事件的键号
 * @return 0~15 键号, 仅在 sensor_poll 返回 EVT_KEY_PRESS 时有效
 */
int8_t sensor_get_key(void);

#ifdef __cplusplus
}
#endif

#endif /* PET_SENSOR_H */
