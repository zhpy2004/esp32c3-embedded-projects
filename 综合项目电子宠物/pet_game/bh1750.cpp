/**
 * @file    bh1750.cpp
 * @brief   BH1750FVI I2C 环境光传感器驱动
 */

#include <Arduino.h>
#include <Wire.h>
#include "bh1750.h"

/* BH1750 指令集 */
#define CMD_POWER_DOWN      0x00
#define CMD_POWER_ON        0x01
#define CMD_RESET           0x07
#define CMD_ONE_TIME_HRES   0x20    /* One Time H-Resolution Mode, 1 lx, ~120ms */

static uint8_t s_addr = 0;

static void bh1750_write_cmd(uint8_t cmd)
{
    Wire.beginTransmission(s_addr);
    Wire.write(cmd);
    Wire.endTransmission();
}

void bh1750_init(uint8_t addr)
{
    s_addr = addr;
    bh1750_write_cmd(CMD_POWER_ON);
    delay(1);
    bh1750_write_cmd(CMD_RESET);
    delay(1);
    bh1750_write_cmd(CMD_POWER_DOWN);
}

void bh1750_trigger(void)
{
    bh1750_write_cmd(CMD_ONE_TIME_HRES);
}

int32_t bh1750_read_lux(void)
{
    uint8_t buf[2];

    Wire.requestFrom(s_addr, (uint8_t)2);
    if (Wire.available() < 2) return -1;

    buf[0] = Wire.read();
    buf[1] = Wire.read();

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];

    /* lux = raw / 1.2 */
    return (int32_t)((float)raw / 1.2f);
}

int32_t bh1750_read_lux_blocking(void)
{
    bh1750_trigger();
    delay(180);
    return bh1750_read_lux();
}
