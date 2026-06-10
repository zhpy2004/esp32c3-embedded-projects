/**
 * @file    pet_core.h
 * @brief   宠物核心逻辑 — 属性管理 / 自然衰减 / 活动系统
 */

#ifndef PET_CORE_H
#define PET_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/* ============================================================
 * 游戏状态机
 * ============================================================ */
typedef enum {
    STATE_INIT,
    STATE_MAIN,
    STATE_FEED_MENU,
    STATE_FEEDING,
    STATE_PLAY_MENU,
    STATE_PLAYING,
    STATE_MINIGAME,
    STATE_MINIGAME_RESULT,
    STATE_WORKING,
    STATE_FIGHTING,
    STATE_FIGHT_RESULT,
    STATE_DEAD,
    STATE_NTP_SYNC,
} game_state_t;

/* ============================================================
 * 战斗结果
 * ============================================================ */
typedef enum {
    FIGHT_NONE,
    FIGHT_WIN,
    FIGHT_LOSE,
    FIGHT_BIGWIN,
} fight_result_t;

/* ============================================================
 * 宠物数据 (运行时)
 * ============================================================ */
typedef struct {
    bool        alive;
    int16_t     hp;
    int16_t     mood;
    uint32_t    coins;
    uint8_t     appearance;
} pet_status_t;

/* ============================================================
 * 环境数据 (传感器聚合)
 * ============================================================ */
typedef struct {
    int8_t      temp;           /* 温度 °C */
    uint8_t     humidity;       /* 湿度 % */
    uint16_t    lux;            /* 光照 lux */
    bool        is_night;       /* lux < 阈值 */
    bool        temp_abnormal;  /* 温度异常 */
} env_data_t;

/* ============================================================
 * API — 初始化
 * ============================================================ */

/** 初始化为新宠物 (默认属性) */
void pet_init_new(void);

/** 从存档数据恢复 */
void pet_restore(uint8_t alive, int16_t hp, int16_t mood,
                 uint32_t coins, uint8_t appearance);

/* ============================================================
 * API — 属性访问
 * ============================================================ */

pet_status_t *pet_get_status(void);
game_state_t  pet_get_state(void);
void          pet_set_state(game_state_t state);
env_data_t   *pet_get_env(void);

/* ============================================================
 * API — 自然衰减
 * ============================================================ */

/** 每分钟调用, 根据环境执行衰减 */
void pet_decay_tick(void);

/** 离线衰减计算 (加载存档后调用) */
void pet_offline_decay(uint32_t offline_minutes);

/* ============================================================
 * API — 外观更新
 * ============================================================ */

/** 根据当前属性和环境自动计算应显示的外观, 返回是否发生变化 */
bool pet_update_appearance(void);

/* ============================================================
 * API — 活动系统
 * ============================================================ */

/** 喂食: food_idx = 0~3 (面包/牛排/蛋糕/药水)
 *  返回 0=成功, 1=金币不足, 2=体力已满 */
uint8_t pet_feed(uint8_t food_idx);

/** 抚摸互动, 返回 true=成功, false=冷却中 */
bool pet_interact_pet(void);

/** 摇晃互动, 返回 true=成功, false=冷却中 */
bool pet_interact_shake(void);

/** 逗乐小游戏: 生成数字序列, 返回序列指针 (长度 MINIGAME_SEQ_LEN) */
const uint8_t *pet_minigame_start(void);

/** 逗乐结果判定: 0=成功, 1=失败, 2=超时/冷却 */
uint8_t pet_minigame_check(const uint8_t *input, uint8_t len);

/** 开始打工, 返回 true=成功, false=条件不满足 */
bool pet_work_start(void);

/** 打工结算 (30s 后调用), 返回获得金币数 */
uint16_t pet_work_finish(void);

/** 开始战斗, 返回 true=成功, false=体力不足 */
bool pet_fight_start(void);

/** 战斗结算, 返回结果和获得金币数 */
fight_result_t pet_fight_resolve(int16_t *coin_reward);

/* ============================================================
 * API — 作弊接口 (Blinker)
 * ============================================================ */

void pet_cheat_add_coins(uint32_t amount);
void pet_cheat_set_hp(int16_t hp);
void pet_cheat_set_mood(int16_t mood);
void pet_cheat_set_coins(uint32_t coins);
void pet_cheat_max_all(void);
void pet_cheat_revive(void);

#ifdef __cplusplus
}
#endif

#endif /* PET_CORE_H */
