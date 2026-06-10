/**
 * @file    mpu6050.cpp
 * @brief   MPU-6050 I2C 姿态传感器驱动 (仅加速度计)
 *
 * 寄存器参考 MPU-6000/MPU-6050 Register Map (RM-MPU-6000A-00 Rev 4.2)
 *   PWR_MGMT_1  (0x6B): bit6=SLEEP, bits[2:0]=CLKSEL
 *   PWR_MGMT_2  (0x6C): bits[5:3]=禁用陀螺仪各轴
 *   ACCEL_CONFIG(0x1C): bits[4:3]=AFS_SEL (0=±2g)
 *   ACCEL_XOUT_H(0x3B): 加速度数据起始地址, 连续 6 字节 (XH,XL,YH,YL,ZH,ZL)
 *   WHO_AM_I    (0x75): 应返回 0x68
 */

#include <Arduino.h>
#include <Wire.h>
#include "mpu6050.h"

/* 寄存器地址 */
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_XOUT_H   0x3B
#define REG_PWR_MGMT_1      0x6B
#define REG_PWR_MGMT_2      0x6C
#define REG_WHO_AM_I        0x75

static uint8_t s_addr = 0;
static mpu6050_accel_t s_prev = {0, 0, 0};
static bool s_prev_valid = false;

static void mpu_write_reg(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(s_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t mpu_read_reg(uint8_t reg)
{
    Wire.beginTransmission(s_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(s_addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

bool mpu6050_init(uint8_t addr)
{
    s_addr = addr;
    s_prev_valid = false;

    /* 复位设备 */
    mpu_write_reg(REG_PWR_MGMT_1, 0x80);
    delay(100);

    /* 唤醒, CLKSEL=1 (PLL with X Gyro reference, 更稳定) */
    mpu_write_reg(REG_PWR_MGMT_1, 0x01);
    delay(10);

    /* WHO_AM_I 校验 */
    uint8_t who = mpu_read_reg(REG_WHO_AM_I);
    if (who != 0x68) return false;

    /* 加速度计配置: ±2g (AFS_SEL=0) */
    mpu_write_reg(REG_ACCEL_CONFIG, 0x00);

    /* DLPF 配置: 带宽 44Hz, 延迟 4.9ms (DLPF_CFG=3) */
    mpu_write_reg(REG_CONFIG, 0x03);

    /* 采样率分频: 1kHz / (1+4) = 200Hz */
    mpu_write_reg(REG_SMPLRT_DIV, 0x04);

    /* 禁用陀螺仪全部三轴以省电: STBY_XG=1, STBY_YG=1, STBY_ZG=1 */
    mpu_write_reg(REG_PWR_MGMT_2, 0x07);

    return true;
}

bool mpu6050_read_accel(mpu6050_accel_t *accel)
{
    Wire.beginTransmission(s_addr);
    Wire.write(REG_ACCEL_XOUT_H);
    Wire.endTransmission(false);

    Wire.requestFrom(s_addr, (uint8_t)6);
    if (Wire.available() < 6) return false;

    uint8_t buf[6];
    for (uint8_t i = 0; i < 6; i++) {
        buf[i] = Wire.read();
    }

    accel->ax = (int16_t)((buf[0] << 8) | buf[1]);
    accel->ay = (int16_t)((buf[2] << 8) | buf[3]);
    accel->az = (int16_t)((buf[4] << 8) | buf[5]);

    return true;
}

bool mpu6050_check_shake(uint16_t threshold)
{
    mpu6050_accel_t cur;
    if (!mpu6050_read_accel(&cur)) return false;

    if (!s_prev_valid) {
        s_prev = cur;
        s_prev_valid = true;
        return false;
    }

    int32_t dx = (int32_t)cur.ax - s_prev.ax;
    int32_t dy = (int32_t)cur.ay - s_prev.ay;
    int32_t dz = (int32_t)cur.az - s_prev.az;

    s_prev = cur;

    /* 向量差模长的平方 (避免开方运算) */
    uint32_t mag_sq = (uint32_t)(dx * dx + dy * dy + dz * dz);
    uint32_t thr_sq = (uint32_t)threshold * threshold;

    return mag_sq > thr_sq;
}
