/**
 * @file    pet_core.cpp
 * @brief   宠物核心逻辑实现
 */

#include <Arduino.h>
#include "pet_core.h"

/* ============================================================
 * 内部状态
 * ============================================================ */
static pet_status_t s_pet;
static env_data_t   s_env;
static game_state_t s_state = STATE_INIT;

/* 衰减计数器: 心情每 MOOD_DECAY_TICKS 分钟 -1 */
static uint8_t s_mood_tick_cnt = 0;

/* 冷却时间戳 */
static unsigned long s_cd_pet       = 0;
static unsigned long s_cd_shake     = 0;
static unsigned long s_cd_minigame  = 0;

/* 逗乐序列 */
static uint8_t s_minigame_seq[MINIGAME_SEQ_LEN];
static unsigned long s_minigame_start_ms = 0;

/* ============================================================
 * 食物表 (price, hp, mood)
 * ============================================================ */
static const int16_t food_table[FOOD_COUNT][3] = {
    { FOOD_BREAD_PRICE,  FOOD_BREAD_HP,  FOOD_BREAD_MOOD  },
    { FOOD_STEAK_PRICE,  FOOD_STEAK_HP,  FOOD_STEAK_MOOD  },
    { FOOD_CAKE_PRICE,   FOOD_CAKE_HP,   FOOD_CAKE_MOOD   },
    { FOOD_POTION_PRICE, FOOD_POTION_HP, FOOD_POTION_MOOD },
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static int16_t clamp16(int16_t val, int16_t lo, int16_t hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static uint32_t clamp32(uint32_t val, uint32_t hi)
{
    return val > hi ? hi : val;
}

static bool cd_ready(unsigned long *stamp, unsigned long cooldown_ms)
{
    unsigned long now = millis();
    if (now - *stamp >= cooldown_ms) {
        *stamp = now;
        return true;
    }
    return false;
}

static void check_death(void)
{
    if (s_pet.hp <= 0) {
        s_pet.hp = 0;
        s_pet.alive = false;
        s_pet.appearance = APPEAR_DEAD;
        s_state = STATE_DEAD;
    }
}

/* ============================================================
 * 初始化
 * ============================================================ */

void pet_init_new(void)
{
    s_pet.alive      = true;
    s_pet.hp         = INIT_HP;
    s_pet.mood       = INIT_MOOD;
    s_pet.coins      = INIT_COINS;
    s_pet.appearance = APPEAR_NORMAL;
    s_state          = STATE_MAIN;
    s_mood_tick_cnt  = 0;

    randomSeed(analogRead(0) ^ micros());
}

void pet_restore(uint8_t alive, int16_t hp, int16_t mood,
                 uint32_t coins, uint8_t appearance)
{
    s_pet.alive      = alive;
    s_pet.hp         = clamp16(hp, 0, HP_MAX);
    s_pet.mood       = clamp16(mood, 0, MOOD_MAX);
    s_pet.coins      = clamp32(coins, COIN_MAX);
    s_pet.appearance = appearance < APPEAR_COUNT ? appearance : APPEAR_NORMAL;
    s_state          = alive ? STATE_MAIN : STATE_DEAD;
    s_mood_tick_cnt  = 0;

    randomSeed(analogRead(0) ^ micros());
}

/* ============================================================
 * 属性访问
 * ============================================================ */

pet_status_t *pet_get_status(void)  { return &s_pet; }
game_state_t  pet_get_state(void)   { return s_state; }
void          pet_set_state(game_state_t state) { s_state = state; }
env_data_t   *pet_get_env(void)     { return &s_env; }

/* ============================================================
 * 自然衰减
 * ============================================================ */

void pet_decay_tick(void)
{
    if (!s_pet.alive) return;

    /* HP 衰减: -1/min (测试: 夜间不减半) */
    int16_t hp_dec = HP_DECAY_PER_MIN;
    s_pet.hp -= hp_dec;

    /* Mood 衰减: 每 2min -1 (测试: 夜间不减半) */
    s_mood_tick_cnt++;
    uint8_t mood_period = MOOD_DECAY_TICKS;
    if (s_mood_tick_cnt >= mood_period) {
        s_mood_tick_cnt = 0;

        int16_t mood_dec = 1;

        /* 温度异常额外 -1 */
        if (s_env.temp_abnormal) mood_dec += 1;

        s_pet.mood -= mood_dec;
    }

    s_pet.hp   = clamp16(s_pet.hp, 0, HP_MAX);
    s_pet.mood = clamp16(s_pet.mood, 0, MOOD_MAX);

    check_death();
}

void pet_offline_decay(uint32_t offline_minutes)
{
    if (!s_pet.alive) return;

    uint32_t max_min = (uint32_t)MAX_OFFLINE_HOURS * 60;
    if (offline_minutes > max_min) offline_minutes = max_min;

    s_pet.hp   -= (int16_t)(offline_minutes * HP_DECAY_PER_MIN);
    s_pet.mood -= (int16_t)(offline_minutes / MOOD_DECAY_TICKS);

    s_pet.hp   = clamp16(s_pet.hp, 0, HP_MAX);
    s_pet.mood = clamp16(s_pet.mood, 0, MOOD_MAX);

    check_death();
}

/* ============================================================
 * 外观更新
 * ============================================================ */

bool pet_update_appearance(void)
{
    if (!s_pet.alive) {
        if (s_pet.appearance != APPEAR_DEAD) {
            s_pet.appearance = APPEAR_DEAD;
            return true;
        }
        return false;
    }

    uint8_t prev = s_pet.appearance;

    if (s_env.is_night) {
        s_pet.appearance = APPEAR_SLEEP;
    } else if (s_pet.hp <= 30) {
        s_pet.appearance = APPEAR_HUNGRY;
    } else if (s_pet.hp <= 30 || s_pet.mood <= 30) {
        s_pet.appearance = APPEAR_SAD;
    } else if (s_pet.hp > 70 && s_pet.mood > 70) {
        s_pet.appearance = APPEAR_HAPPY;
    } else {
        s_pet.appearance = APPEAR_NORMAL;
    }

    return s_pet.appearance != prev;
}

/* ============================================================
 * 喂食系统
 * ============================================================ */

uint8_t pet_feed(uint8_t food_idx)
{
    if (food_idx >= FOOD_COUNT) return 1;
    if (!s_pet.alive) return 1;

    int16_t  price = food_table[food_idx][0];
    int16_t  hp_add = food_table[food_idx][1];
    int16_t  mood_add = food_table[food_idx][2];

    if ((uint32_t)price > s_pet.coins) return 1;
    if (s_pet.hp >= HP_MAX) return 2;

    s_pet.coins -= price;
    s_pet.hp   = clamp16(s_pet.hp + hp_add, 0, HP_MAX);
    s_pet.mood = clamp16(s_pet.mood + mood_add, 0, MOOD_MAX);

    return 0;
}

/* ============================================================
 * 互动系统
 * ============================================================ */

bool pet_interact_pet(void)
{
    if (!s_pet.alive) return false;
    if (!cd_ready(&s_cd_pet, COOLDOWN_PET_MS)) return false;

    s_pet.mood = clamp16(s_pet.mood + INTERACT_PET_MOOD, 0, MOOD_MAX);
    s_pet.hp   = clamp16(s_pet.hp - INTERACT_PET_HP_COST, 0, HP_MAX);
    check_death();
    return true;
}

bool pet_interact_shake(void)
{
    if (!s_pet.alive) return false;
    if (!cd_ready(&s_cd_shake, COOLDOWN_SHAKE_MS)) return false;

    s_pet.mood = clamp16(s_pet.mood + INTERACT_SHAKE_MOOD, 0, MOOD_MAX);
    s_pet.hp   = clamp16(s_pet.hp - INTERACT_SHAKE_HP_COST, 0, HP_MAX);
    check_death();
    return true;
}

/* ============================================================
 * 逗乐小游戏
 * ============================================================ */

const uint8_t *pet_minigame_start(void)
{
    for (uint8_t i = 0; i < MINIGAME_SEQ_LEN; i++) {
        s_minigame_seq[i] = random(1, 5);   /* 1~4 */
    }
    s_minigame_start_ms = millis();
    return s_minigame_seq;
}

uint8_t pet_minigame_check(const uint8_t *input, uint8_t len)
{
    if (!s_pet.alive) return 2;
    if (!cd_ready(&s_cd_minigame, COOLDOWN_MINIGAME_MS)) return 2;

    if (millis() - s_minigame_start_ms > MINIGAME_TIMEOUT_MS) return 1;

    bool ok = (len == MINIGAME_SEQ_LEN);
    if (ok) {
        for (uint8_t i = 0; i < MINIGAME_SEQ_LEN; i++) {
            if (input[i] != s_minigame_seq[i]) { ok = false; break; }
        }
    }

    if (ok) {
        s_pet.mood = clamp16(s_pet.mood + INTERACT_JOKE_MOOD_OK, 0, MOOD_MAX);
    } else {
        s_pet.mood = clamp16(s_pet.mood + INTERACT_JOKE_MOOD_FAIL, 0, MOOD_MAX);
    }
    s_pet.hp = clamp16(s_pet.hp - INTERACT_JOKE_HP_COST, 0, HP_MAX);
    check_death();

    return ok ? 0 : 1;
}

/* ============================================================
 * 打工系统
 * ============================================================ */

bool pet_work_start(void)
{
    if (!s_pet.alive) return false;
    if (s_pet.hp < WORK_HP_COST) return false;
    return true;
}

uint16_t pet_work_finish(void)
{
    uint16_t coin = WORK_BASE_COIN;

    if (s_pet.mood > WORK_MOOD_HIGH) {
        coin = coin * 3 / 2;           /* ×1.5 */
    } else if (s_pet.mood < WORK_MOOD_LOW) {
        coin = coin / 2;               /* ×0.5 */
    }

    s_pet.hp   = clamp16(s_pet.hp - WORK_HP_COST, 0, HP_MAX);
    s_pet.mood = clamp16(s_pet.mood - WORK_MOOD_COST, 0, MOOD_MAX);
    s_pet.coins = clamp32(s_pet.coins + coin, COIN_MAX);

    check_death();
    return coin;
}

/* ============================================================
 * 战斗系统
 * ============================================================ */

bool pet_fight_start(void)
{
    if (!s_pet.alive) return false;
    if (s_pet.hp < FIGHT_HP_MIN) return false;
    return true;
}

fight_result_t pet_fight_resolve(int16_t *coin_reward)
{
    int16_t win_rate = FIGHT_WIN_BASE + s_pet.hp / FIGHT_WIN_HP_DIV;
    int16_t roll = random(0, 100);

    fight_result_t result;

    if (roll < win_rate) {
        /* 胜利判定 */
        if (s_pet.hp > FIGHT_BIGWIN_HP_MIN && random(0, 100) < FIGHT_BIGWIN_RATE) {
            /* 大胜 */
            *coin_reward = FIGHT_BIGWIN_COIN;
            s_pet.hp = clamp16(s_pet.hp - FIGHT_BIGWIN_HP_COST, 0, HP_MAX);
            s_pet.mood = clamp16(s_pet.mood + FIGHT_BIGWIN_MOOD, 0, MOOD_MAX);
            result = FIGHT_BIGWIN;
        } else {
            /* 普通胜利 */
            *coin_reward = random(FIGHT_WIN_COIN_MIN, FIGHT_WIN_COIN_MAX + 1);
            s_pet.hp = clamp16(s_pet.hp - FIGHT_WIN_HP_COST, 0, HP_MAX);
            result = FIGHT_WIN;
        }
    } else {
        /* 失败 */
        *coin_reward = FIGHT_LOSE_COIN;
        s_pet.hp = clamp16(s_pet.hp - FIGHT_LOSE_HP_COST, 0, HP_MAX);
        result = FIGHT_LOSE;
    }

    s_pet.coins = clamp32(s_pet.coins + *coin_reward, COIN_MAX);
    check_death();
    return result;
}

/* ============================================================
 * 作弊接口
 * ============================================================ */

void pet_cheat_add_coins(uint32_t amount)
{
    s_pet.coins = clamp32(s_pet.coins + amount, COIN_MAX);
}

void pet_cheat_set_hp(int16_t hp)
{
    s_pet.hp = clamp16(hp, 0, HP_MAX);
}

void pet_cheat_set_mood(int16_t mood)
{
    s_pet.mood = clamp16(mood, 0, MOOD_MAX);
}

void pet_cheat_set_coins(uint32_t coins)
{
    s_pet.coins = clamp32(coins, COIN_MAX);
}

void pet_cheat_max_all(void)
{
    s_pet.hp = HP_MAX;
    s_pet.mood = MOOD_MAX;
    s_pet.coins = clamp32(s_pet.coins + 2000, COIN_MAX);
}

void pet_cheat_revive(void)
{
    s_pet.alive      = true;
    s_pet.hp         = 50;
    s_pet.mood       = 50;
    s_pet.appearance = APPEAR_NORMAL;
    s_state          = STATE_MAIN;
}
