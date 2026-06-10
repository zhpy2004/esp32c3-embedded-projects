/**
 * @file    pet_save.cpp
 * @brief   存档管理实现 — ESP32 NVS (Preferences) A/B 双槽交替写入
 */

#include <Arduino.h>
#include <Preferences.h>
#include "pet_save.h"
#include "pet_core.h"
#include "game_config.h"
#include "ds1307.h"

/* ============================================================
 * 内部状态
 * ============================================================ */
static uint8_t  s_active_slot = 0;
static uint8_t  s_next_seq    = 1;
static uint32_t s_offline_min = 0;

static Preferences s_prefs;

static const char *NVS_NAMESPACE = "pet_save";
static const char *KEY_SLOT_A    = "slot_a";
static const char *KEY_SLOT_B    = "slot_b";

/* ============================================================
 * 工具: XOR 校验 (前 sizeof-1 字节)
 * ============================================================ */
static uint8_t calc_checksum(const pet_save_t *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < sizeof(pet_save_t) - 1; i++) {
        xor_val ^= p[i];
    }
    return xor_val;
}

/* ============================================================
 * 工具: 校验存档有效性
 * ============================================================ */
static bool slot_valid(const pet_save_t *s)
{
    if (s->magic != SAVE_MAGIC)     return false;
    if (s->version != SAVE_VERSION) return false;
    return calc_checksum(s) == s->checksum;
}

/* ============================================================
 * 工具: 读/写存档槽 (NVS)
 * ============================================================ */
static void slot_read(const char *key, pet_save_t *s)
{
    size_t len = s_prefs.getBytes(key, s, sizeof(pet_save_t));
    if (len != sizeof(pet_save_t)) {
        memset(s, 0, sizeof(pet_save_t));
    }
}

static void slot_write(const char *key, const pet_save_t *s)
{
    s_prefs.putBytes(key, s, sizeof(pet_save_t));
}

/* ============================================================
 * 工具: 时间戳 → 分钟数 (自 2000-01-01 起, 近似值)
 * ============================================================ */
static uint32_t time_to_minutes(uint8_t year, uint8_t month, uint8_t date,
                                uint8_t hour, uint8_t min)
{
    static const uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    uint32_t days = (uint32_t)year * 365 + (year / 4);

    for (uint8_t m = 1; m < month && m <= 12; m++) {
        days += mdays[m - 1];
    }
    days += date;

    return days * 1440UL + (uint32_t)hour * 60 + min;
}

/* ============================================================
 * save_init — 上电读取双槽, 校验, 恢复, 离线衰减
 * ============================================================ */
bool save_init(void)
{
    s_prefs.begin(NVS_NAMESPACE, false);

    pet_save_t slot_a, slot_b;

    slot_read(KEY_SLOT_A, &slot_a);
    slot_read(KEY_SLOT_B, &slot_b);

    bool a_ok = slot_valid(&slot_a);
    bool b_ok = slot_valid(&slot_b);

    pet_save_t *chosen = NULL;

    if (a_ok && b_ok) {
        int8_t diff = (int8_t)(slot_a.slot_seq - slot_b.slot_seq);
        if (diff >= 0) {
            chosen = &slot_a;
            s_active_slot = 0;
        } else {
            chosen = &slot_b;
            s_active_slot = 1;
        }
    } else if (a_ok) {
        chosen = &slot_a;
        s_active_slot = 0;
    } else if (b_ok) {
        chosen = &slot_b;
        s_active_slot = 1;
    }

    if (!chosen) {
        pet_init_new();
        s_next_seq    = 1;
        s_offline_min = 0;
        return false;
    }

    pet_restore(chosen->alive, (int16_t)chosen->hp, (int16_t)chosen->mood,
                chosen->coins, chosen->appearance);

    s_next_seq = chosen->slot_seq + 1;

    ds1307_time_t now;
    ds1307_read_time(&now);

    uint32_t save_min = time_to_minutes(chosen->year, chosen->month,
                                        chosen->date, chosen->hour, chosen->min);
    uint32_t now_min  = time_to_minutes(now.year, now.month,
                                        now.date, now.hour, now.min);

    s_offline_min = (now_min > save_min) ? (now_min - save_min) : 0;

    if (s_offline_min > 0) {
        pet_offline_decay(s_offline_min);
    }

    return true;
}

/* ============================================================
 * save_write — 写入非活跃槽, 翻转
 * ============================================================ */
void save_write(void)
{
    pet_status_t *st = pet_get_status();
    ds1307_time_t t;
    ds1307_read_time(&t);

    pet_save_t s;
    s.magic      = SAVE_MAGIC;
    s.version    = SAVE_VERSION;
    s.slot_seq   = s_next_seq++;
    s.alive      = st->alive ? 1 : 0;
    s.hp         = (uint16_t)st->hp;
    s.mood       = (uint16_t)st->mood;
    s.coins      = st->coins;
    s.appearance = st->appearance;
    s.year       = t.year;
    s.month      = t.month;
    s.date       = t.date;
    s.hour       = t.hour;
    s.min        = t.min;
    s.sec        = t.sec;
    s.checksum   = calc_checksum(&s);

    const char *key = (s_active_slot == 0) ? KEY_SLOT_B : KEY_SLOT_A;
    slot_write(key, &s);

    s_active_slot ^= 1;
}

/* ============================================================
 * save_get_offline_minutes
 * ============================================================ */
uint32_t save_get_offline_minutes(void)
{
    return s_offline_min;
}
