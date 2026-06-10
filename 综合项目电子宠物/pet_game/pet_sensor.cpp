/**
 * @file    pet_sensor.cpp
 * @brief   传感器综合管理实现 — 时间片轮询 + 环境数据聚合
 */

#include <Arduino.h>
#include "pet_sensor.h"
#include "pet_core.h"
#include "game_config.h"
#include "ttp229.h"
#include "mpu6050.h"
#include "bh1750.h"
#include "dht11.h"

/* ============================================================
 * 各传感器上次轮询时间戳
 * ============================================================ */
static unsigned long s_last_key   = 0;
static unsigned long s_last_mpu   = 0;
static unsigned long s_last_dht   = 0;
static unsigned long s_last_decay = 0;
static unsigned long s_last_save  = 0;

/* BH1750 非阻塞状态机 */
static enum { BH_IDLE, BH_WAIT } s_bh_state = BH_IDLE;
static unsigned long s_bh_last    = 0;
static unsigned long s_bh_trigger = 0;

/* 最近按键值 */
static int8_t s_last_key_val = -1;

/* ============================================================
 * sensor_init
 * ============================================================ */
void sensor_init(void)
{
    ttp229_init();
    mpu6050_init(I2C_ADDR_MPU6050);
    bh1750_init(I2C_ADDR_BH1750);
    dht11_init(PIN_DHT11);

    unsigned long now = millis();
    s_last_key   = now;
    s_last_mpu   = now;
    s_last_dht   = now;
    s_last_decay = now;
    s_last_save  = now;
    s_bh_last    = now;
}

/* ============================================================
 * sensor_poll
 * ============================================================ */
uint16_t sensor_poll(void)
{
    uint16_t events = 0;
    unsigned long now = millis();
    env_data_t *env = pet_get_env();

    /* --- TTP229 键盘 (50ms) --- */
    if (now - s_last_key >= POLL_KEY_MS) {
        s_last_key = now;
        int8_t k = ttp229_poll();
        if (k >= 0) {
            s_last_key_val = k;
            events |= EVT_KEY_PRESS;
        }
    }

    /* --- MPU6050 摇晃 (200ms) --- */
    if (now - s_last_mpu >= POLL_MPU_MS) {
        s_last_mpu = now;
        if (mpu6050_check_shake(SHAKE_ACCEL_THRESHOLD)) {
            events |= EVT_SHAKE;
        }
    }

    /* --- BH1750 光照 (10s, 非阻塞) --- */
    if (s_bh_state == BH_IDLE) {
        if (now - s_bh_last >= POLL_BH1750_MS) {
            bh1750_trigger();
            s_bh_trigger = now;
            s_bh_state = BH_WAIT;
        }
    } else {
        if (now - s_bh_trigger >= 200) {
            int32_t lux = bh1750_read_lux();
            if (lux >= 0) {
                env->lux = (uint16_t)lux;
                env->is_night = (lux < NIGHT_LUX_THRESHOLD);
            }
            s_bh_last  = now;
            s_bh_state = BH_IDLE;
        }
    }

    /* --- DHT11 温湿度 (30s) --- */
    if (now - s_last_dht >= POLL_DHT11_MS) {
        s_last_dht = now;
        if (dht11_read(NULL)) {
            float t = dht11_get_temp();
            float h = dht11_get_humidity();
            if (t > -999.0f) {
                env->temp = (int8_t)t;
                env->temp_abnormal = (t > TEMP_HIGH_THRESHOLD ||
                                      t < TEMP_LOW_THRESHOLD);
            }
            if (h >= 0.0f) {
                env->humidity = (uint8_t)h;
            }
        }
    }

    /* --- 衰减 tick (60s) --- */
    if (now - s_last_decay >= POLL_DECAY_MS) {
        s_last_decay = now;
        events |= EVT_DECAY;
    }

    /* --- 自动存档 tick (90s) --- */
    if (now - s_last_save >= AUTO_SAVE_INTERVAL_MS) {
        s_last_save = now;
        events |= EVT_AUTO_SAVE;
    }

    return events;
}

/* ============================================================
 * sensor_get_key
 * ============================================================ */
int8_t sensor_get_key(void)
{
    return s_last_key_val;
}
