/**
 * @file    at24c32d.cpp
 * @brief   AT24C32D I2C EEPROM 驱动实现
 */

extern "C" {
#include "at24c32d.h"
}

#include <Arduino.h>
#include <Wire.h>

/* ============================================================
 * 内部: ACK Polling
 * ============================================================ */
static void ack_poll(void)
{
    unsigned long start = millis();
    while (millis() - start < AT24C32D_ACK_TIMEOUT_MS) {
        Wire.beginTransmission(AT24C32D_I2C_ADDR);
        if (Wire.endTransmission() == 0) return;
        delayMicroseconds(500);
    }
}

/* ============================================================
 * 内部: 发送 12-bit 字地址
 * ============================================================ */
static void send_word_addr(uint16_t addr)
{
    Wire.write((uint8_t)((addr >> 8) & 0x0F));
    Wire.write((uint8_t)(addr & 0xFF));
}

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" void at24c32d_write_byte(uint16_t addr, uint8_t data)
{
    Wire.beginTransmission(AT24C32D_I2C_ADDR);
    send_word_addr(addr);
    Wire.write(data);
    Wire.endTransmission();
    ack_poll();
}

extern "C" uint8_t at24c32d_read_byte(uint16_t addr)
{
    Wire.beginTransmission(AT24C32D_I2C_ADDR);
    send_word_addr(addr);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)AT24C32D_I2C_ADDR, (size_t)1);
    return Wire.read();
}

extern "C" void at24c32d_write_page(uint16_t addr, const uint8_t *buf, uint8_t len)
{
    if (len > AT24C32D_PAGE_SIZE) len = AT24C32D_PAGE_SIZE;

    Wire.beginTransmission(AT24C32D_I2C_ADDR);
    send_word_addr(addr);
    for (uint8_t i = 0; i < len; i++) {
        Wire.write(buf[i]);
    }
    Wire.endTransmission();
    ack_poll();
}

extern "C" void at24c32d_read_sequential(uint16_t addr, uint8_t *buf, uint16_t len)
{
    Wire.beginTransmission(AT24C32D_I2C_ADDR);
    send_word_addr(addr);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)AT24C32D_I2C_ADDR, (size_t)len);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
}
