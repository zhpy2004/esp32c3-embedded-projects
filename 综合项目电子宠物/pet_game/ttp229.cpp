/**
 * @file    ttp229.cpp
 * @brief   TTP229-LSF 16 键电容触摸 I2C 驱动实现 (轮询模式)
 *
 * @note    硬件位映射 → 丝印键号 对照表:
 *
 *          丝印:  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
 *          TP:    7  6  5  4  3  2  1  0  15 14 13 12 11 10  9  8
 *
 *          即 buf[0] 的 bit7=丝印0, bit6=丝印1 ... bit0=丝印7
 *             buf[1] 的 bit7=丝印8, bit6=丝印9 ... bit0=丝印F
 */

extern "C" {
#include "ttp229.h"
}

#include <Arduino.h>
#include <Wire.h>

/* ============================================================
 * 内部状态
 * ============================================================ */
static int8_t         s_last_key  = -1;     /* 上一次按键 */
static unsigned long  s_last_poll = 0;      /* 上一次轮询时间 */

/* ============================================================
 * 内部: 从原始 2 字节数据解析丝印键号
 *       buf[0] bit7~bit0 → 丝印 0~7
 *       buf[1] bit7~bit0 → 丝印 8~F
 * ============================================================ */
static int8_t raw_to_silk(uint8_t byte0, uint8_t byte1)
{
    /* 扫描 buf[0]: bit7(丝印0) → bit0(丝印7) */
    for (int8_t i = 0; i < 8; i++) {
        if (byte0 & (0x80 >> i)) return i;       /* 丝印 0~7 */
    }
    /* 扫描 buf[1]: bit7(丝印8) → bit0(丝印F) */
    for (int8_t i = 0; i < 8; i++) {
        if (byte1 & (0x80 >> i)) return i + 8;   /* 丝印 8~F */
    }
    return -1;
}

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" void ttp229_init(void)
{
    s_last_key  = -1;
    s_last_poll = 0;
}

extern "C" uint16_t ttp229_read_keys_raw(void)
{
    uint8_t buf[2] = {0, 0};

    Wire.requestFrom((uint8_t)TTP229_I2C_ADDR, (size_t)2);
    if (Wire.available() >= 2) {
        buf[0] = Wire.read();
        buf[1] = Wire.read();
    }

    return ((uint16_t)buf[1] << 8) | buf[0];
}

extern "C" int8_t ttp229_get_key(void)
{
    uint8_t buf[2] = {0, 0};

    Wire.requestFrom((uint8_t)TTP229_I2C_ADDR, (size_t)2);
    if (Wire.available() >= 2) {
        buf[0] = Wire.read();
        buf[1] = Wire.read();
    }

    return raw_to_silk(buf[0], buf[1]);
}

extern "C" int8_t ttp229_poll(void)
{
    unsigned long now = millis();

    if (now - s_last_poll < TTP229_POLL_INTERVAL_MS) {
        return -1;
    }
    s_last_poll = now;

    int8_t key = ttp229_get_key();

    /* 边沿检测: 仅在 "无按键→有按键" 或 "换键" 时触发 */
    if (key >= 0 && key != s_last_key) {
        s_last_key = key;
        return key;
    }

    /* 松手时重置, 为下次按下做准备 */
    if (key < 0) {
        s_last_key = -1;
    }

    return -1;
}
