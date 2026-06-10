/**
 * @file    dht11.cpp
 * @brief   DHT11 单总线温湿度传感器驱动
 *
 * 时序参考 DHT11 V1.3 手册 表4:
 *   起始信号: 主机拉低 18~30ms, 释放 10~20µs
 *   响应信号: 从机拉低 81~85µs, 拉高 85~88µs
 *   数据 "0": 低 52~56µs + 高 23~27µs
 *   数据 "1": 低 52~56µs + 高 68~74µs
 */

#include <Arduino.h>
#include "dht11.h"

static uint8_t s_pin      = 0;
static float   s_last_temp = -999.0f;
static float   s_last_hum  = -1.0f;

/* 等待引脚变为指定电平, 返回等待的微秒数; 超时返回 0 */
static uint32_t wait_for_level(bool level, uint32_t timeout_us)
{
    uint32_t start = micros();
    while (digitalRead(s_pin) != level) {
        if (micros() - start > timeout_us) return 0;
    }
    return micros() - start;
}

/* 测量引脚保持在指定电平的持续时间 (µs); 超时返回 0 */
static uint32_t measure_pulse(bool level, uint32_t timeout_us)
{
    uint32_t start = micros();
    while (digitalRead(s_pin) == level) {
        if (micros() - start > timeout_us) return 0;
    }
    return micros() - start;
}

void dht11_init(uint8_t pin)
{
    s_pin = pin;
    pinMode(s_pin, INPUT_PULLUP);
}

bool dht11_read(dht11_raw_t *raw)
{
    uint8_t data[5] = {0};

    /* === 主机发送起始信号 === */
    pinMode(s_pin, OUTPUT);
    digitalWrite(s_pin, LOW);
    delay(20);                      /* 拉低 20ms (18~30ms) */
    digitalWrite(s_pin, HIGH);
    delayMicroseconds(13);          /* 释放 ~13µs (10~20µs) */
    pinMode(s_pin, INPUT_PULLUP);

    /* === 等待从机响应 === */
    /* 等待从机拉低 (响应低电平 ~83µs) */
    if (!wait_for_level(LOW, 100)) return false;
    if (!measure_pulse(LOW, 100))  return false;

    /* 等待响应高电平 ~87µs 结束 */
    if (!measure_pulse(HIGH, 100)) return false;

    /* === 接收 40 bit 数据 === */
    for (uint8_t i = 0; i < 40; i++) {
        /* 每个 bit 前导低电平 ~54µs */
        if (!measure_pulse(LOW, 80)) return false;

        /* 测量高电平时长: <30µs → "0", >50µs → "1" */
        uint32_t high_us = measure_pulse(HIGH, 100);
        if (high_us == 0) return false;

        data[i / 8] <<= 1;
        if (high_us > 40) {
            data[i / 8] |= 1;
        }
    }

    /* === 校验 === */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) return false;

    /* === 解析并缓存 === */
    float temp = (float)data[2];
    uint8_t temp_dec = data[3];
    if (temp_dec & 0x80) {
        /* 负温度: bit7 标记, 低 7 位为小数 */
        temp = -(temp + (float)(temp_dec & 0x7F) * 0.1f);
    } else {
        temp = temp + (float)temp_dec * 0.1f;
    }

    s_last_temp = temp;
    s_last_hum  = (float)data[0] + (float)data[1] * 0.1f;

    if (raw) {
        raw->hum_int  = data[0];
        raw->hum_dec  = data[1];
        raw->temp_int = data[2];
        raw->temp_dec = data[3];
    }

    return true;
}

float dht11_get_temp(void)
{
    return s_last_temp;
}

float dht11_get_humidity(void)
{
    return s_last_hum;
}
