/**
 * @file    mpu6050.h
 * @brief   MPU-6050 I2C 姿态传感器驱动 (仅加速度计, 无第三方库, 基于 Wire)
 * @note    I2C 地址: 0x68 (AD0=LOW) 或 0x69 (AD0=HIGH)
 *          本驱动仅使用加速度计用于摇晃检测, 陀螺仪禁用以节省功耗
 *          加速度量程: ±2g, 灵敏度 16384 LSB/g
 */

#ifndef MPU6050_H
#define MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数
 * ============================================================ */
#define MPU6050_ADDR_AD0_LOW    0x68
#define MPU6050_ADDR_AD0_HIGH   0x69

typedef struct {
    int16_t ax;     /* 加速度 X 原始值 (LSB) */
    int16_t ay;     /* 加速度 Y 原始值 (LSB) */
    int16_t az;     /* 加速度 Z 原始值 (LSB) */
} mpu6050_accel_t;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化 MPU6050 (唤醒 + 配置加速度计 ±2g + 禁用陀螺仪)
 * @param  addr  I2C 从机地址 (0x68 或 0x69)
 * @return true=初始化成功 (WHO_AM_I 校验通过), false=失败
 * @note   调用前需确保 Wire 已 begin
 */
bool mpu6050_init(uint8_t addr);

/**
 * @brief  读取加速度计原始数据
 * @param  accel  输出加速度数据
 * @return true=读取成功, false=I2C 通信失败
 */
bool mpu6050_read_accel(mpu6050_accel_t *accel);

/**
 * @brief  摇晃检测轮询 (需在 loop 中周期调用, 建议 200ms)
 * @param  threshold  加速度变化阈值 (raw LSB), 推荐 20000 (~1.2g 变化量)
 * @return true=检测到摇晃事件, false=无事件
 * @note   内部维护上一次加速度值, 计算前后帧的向量差模长
 */
bool mpu6050_check_shake(uint16_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
