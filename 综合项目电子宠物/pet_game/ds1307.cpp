/**
 * @file    ds1307.cpp
 * @brief   DS1307 I2C RTC 驱动实现
 */

extern "C" {
#include "ds1307.h"
}

#include <Arduino.h>
#include <Wire.h>

/* ============================================================
 * BCD 转换工具 (内部)
 * ============================================================ */
static uint8_t dec_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

static uint8_t bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" void ds1307_init(void)
{
    Wire.beginTransmission(DS1307_I2C_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)DS1307_I2C_ADDR, (size_t)1);
    uint8_t sec_reg = Wire.read();

    if (sec_reg & 0x80) {
        sec_reg &= 0x7F;
        Wire.beginTransmission(DS1307_I2C_ADDR);
        Wire.write(0x00);
        Wire.write(sec_reg);
        Wire.endTransmission();
    }
}

extern "C" void ds1307_read_time(ds1307_time_t *t)
{
    Wire.beginTransmission(DS1307_I2C_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)DS1307_I2C_ADDR, (size_t)7);
    t->sec   = bcd_to_dec(Wire.read() & 0x7F);
    t->min   = bcd_to_dec(Wire.read());
    t->hour  = bcd_to_dec(Wire.read() & 0x3F);
    Wire.read();  /* 跳过星期 */
    t->date  = bcd_to_dec(Wire.read());
    t->month = bcd_to_dec(Wire.read());
    t->year  = bcd_to_dec(Wire.read());
}

extern "C" void ds1307_set_time(const ds1307_time_t *t)
{
    Wire.beginTransmission(DS1307_I2C_ADDR);
    Wire.write(0x00);
    Wire.write(dec_to_bcd(t->sec) & 0x7F);
    Wire.write(dec_to_bcd(t->min));
    Wire.write(dec_to_bcd(t->hour) & 0x3F);
    Wire.write(0x01);
    Wire.write(dec_to_bcd(t->date));
    Wire.write(dec_to_bcd(t->month));
    Wire.write(dec_to_bcd(t->year));
    Wire.endTransmission();
}

extern "C" bool ds1307_is_running(void)
{
    Wire.beginTransmission(DS1307_I2C_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(false);

    Wire.requestFrom((uint8_t)DS1307_I2C_ADDR, (size_t)1);
    uint8_t sec_reg = Wire.read();

    return !(sec_reg & 0x80);
}
