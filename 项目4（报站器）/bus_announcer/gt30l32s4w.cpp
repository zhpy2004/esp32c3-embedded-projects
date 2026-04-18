/**
 * @file    gt30l32s4w.cpp
 * @brief   GT30L32S4W SPI 字库芯片驱动实现
 * @note    严格按照数据手册: READ 0x03, 24-bit 地址, 0 dummy, 连续读取
 *          参考已验证可用的驱动代码
 */

extern "C" {
#include "gt30l32s4w.h"
}

#include <Arduino.h>
#include <SPI.h>

static uint8_t s_cs_pin = 0;

extern "C" void gt30l_init(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs)
{
    s_cs_pin = cs;
    pinMode(s_cs_pin, OUTPUT);
    digitalWrite(s_cs_pin, HIGH);

    /* 关键: 把 CS 引脚也传给 SPI.begin(), 让硬件 SPI 控制器管理 CS */
    SPI.begin(sck, miso, mosi, cs);

    Serial.printf("[GT30L] Init OK  SCK=%d MISO=%d MOSI=%d CS=%d\n", sck, miso, mosi, cs);
}

extern "C" void gt30l_read_hanzi_16x16(uint8_t msb, uint8_t lsb, uint8_t *buf)
{
    uint32_t address = FONT_16X16_GB2312_BASE;

    if (msb >= 0xA1 && msb <= 0xA9 && lsb >= 0xA1) {
        /* GB2312 1区符号 */
        address = ((msb - 0xA1) * 94 + (lsb - 0xA1)) * 32 + FONT_16X16_GB2312_BASE;
    } else if (msb >= 0xB0 && msb <= 0xF7 && lsb >= 0xA1) {
        /* GB2312 2区汉字 */
        address = ((msb - 0xB0) * 94 + (lsb - 0xA1) + 846) * 32 + FONT_16X16_GB2312_BASE;
    }

    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(s_cs_pin, LOW);

    SPI.transfer(0x03);
    SPI.transfer((address >> 16) & 0xFF);
    SPI.transfer((address >> 8) & 0xFF);
    SPI.transfer(address & 0xFF);

    for (int i = 0; i < 32; i++) {
        buf[i] = SPI.transfer(0x00);
    }

    digitalWrite(s_cs_pin, HIGH);
    SPI.endTransaction();
}
