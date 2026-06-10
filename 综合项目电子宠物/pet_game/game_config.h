/**
 * @file    game_config.h
 * @brief   电子宠物游戏参数配置 (集中管理所有数值常量)
 */

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

/* ============================================================
 * 硬件引脚分配 (ESP32-C3 GPIO 0-10)
 * ============================================================ */
#define PIN_BUZZER          10
#define PIN_DHT11           2
#define PIN_EPD_BUSY        1
#define PIN_EPD_RST         19
#define PIN_EPD_SCK         4
#define PIN_EPD_MOSI        6
#define PIN_EPD_CS          7
#define PIN_EPD_DC          18
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

/* ============================================================
 * I2C 设备地址
 * ============================================================ */
#define I2C_ADDR_OLED       0x3C    /* SSD1315 OLED */
#define I2C_ADDR_BH1750     0x23    /* BH1750 光照 (ADDR=LOW) */
#define I2C_ADDR_TTP229     0x57    /* TTP229 触摸键盘 */
#define I2C_ADDR_DS1307     0x68    /* DS1307 RTC */
#define I2C_ADDR_MPU6050    0x69    /* MPU6050 (AD0=HIGH, 避开 DS1307) */

#define I2C_FREQ            100000UL    /* 100kHz (兼容 DS1307) */

/* ============================================================
 * 属性范围
 * ============================================================ */
#define HP_MAX              100
#define MOOD_MAX            100
#define COIN_MAX            99999UL

/* ============================================================
 * 初始属性 (新宠物)
 * ============================================================ */
#define INIT_HP             80
#define INIT_MOOD           80
#define INIT_COINS          100UL

/* ============================================================
 * 衰减速率
 * ============================================================ */
#define HP_DECAY_PER_MIN    1       /* 白天每分钟体力 -1 */
#define MOOD_DECAY_TICKS    2       /* 白天每 2 分钟心情 -1 */
#define NIGHT_DECAY_MULT_N  1       /* 夜间衰减 = 白天 × (N/D) */
#define NIGHT_DECAY_MULT_D  2       /* 即 ×0.5 */

/* ============================================================
 * 环境阈值
 * ============================================================ */
#define NIGHT_LUX_THRESHOLD 50      /* < 50 lux 判定夜间 (测试用) */
#define TEMP_HIGH_THRESHOLD 35      /* 高温 (°C) */
#define TEMP_LOW_THRESHOLD  10      /* 低温 (°C) */

/* ============================================================
 * 喂食系统 — 食物表
 * 索引: 0=面包, 1=牛排, 2=蛋糕, 3=药水
 * ============================================================ */
#define FOOD_COUNT          4

#define FOOD_BREAD_PRICE    5
#define FOOD_BREAD_HP       20
#define FOOD_BREAD_MOOD     0

#define FOOD_STEAK_PRICE    15
#define FOOD_STEAK_HP       40
#define FOOD_STEAK_MOOD     5

#define FOOD_CAKE_PRICE     10
#define FOOD_CAKE_HP        10
#define FOOD_CAKE_MOOD      15

#define FOOD_POTION_PRICE   20
#define FOOD_POTION_HP      60
#define FOOD_POTION_MOOD    (-10)

/* ============================================================
 * 互动系统
 * ============================================================ */
#define INTERACT_PET_MOOD       15  /* 抚摸 Mood +15 */
#define INTERACT_PET_HP_COST    3   /* 抚摸 HP -3 */
#define INTERACT_SHAKE_MOOD     20  /* 摇晃 Mood +20 */
#define INTERACT_SHAKE_HP_COST  5   /* 摇晃 HP -5 */
#define INTERACT_JOKE_MOOD_OK   35  /* 逗乐成功 Mood +35 */
#define INTERACT_JOKE_MOOD_FAIL 5   /* 逗乐失败 Mood +5 */
#define INTERACT_JOKE_HP_COST   5   /* 逗乐 HP -5 */

/* ============================================================
 * 互动冷却 (ms)
 * ============================================================ */
#define COOLDOWN_PET_MS         30000UL     /* 抚摸 30s */
#define COOLDOWN_SHAKE_MS       60000UL     /* 摇晃 60s */
#define COOLDOWN_MINIGAME_MS    120000UL    /* 逗乐 2min */

/* ============================================================
 * 打工系统
 * ============================================================ */
#define WORK_DURATION_MS    30000UL /* 打工时长 30s */
#define WORK_BASE_COIN      20      /* 基础收入 */
#define WORK_HP_COST        15      /* 体力消耗 */
#define WORK_MOOD_COST      10      /* 心情消耗 */
#define WORK_MOOD_HIGH      70      /* 心情好阈值 → 收入 ×1.5 */
#define WORK_MOOD_LOW       30      /* 心情差阈值 → 收入 ×0.5 */

/* ============================================================
 * 战斗系统
 * ============================================================ */
#define FIGHT_HP_MIN        20      /* 最低体力要求 */
#define FIGHT_WIN_BASE      40      /* 胜率基数 (%) */
#define FIGHT_WIN_HP_DIV    2       /* 胜率 = base + HP / div (%) */
#define FIGHT_BIGWIN_RATE   10      /* 大胜额外概率 (%) */
#define FIGHT_BIGWIN_HP_MIN 50      /* 大胜最低体力 */

#define FIGHT_WIN_COIN_MIN  40
#define FIGHT_WIN_COIN_MAX  60
#define FIGHT_WIN_HP_COST   10

#define FIGHT_LOSE_COIN     10
#define FIGHT_LOSE_HP_COST  30

#define FIGHT_BIGWIN_COIN   100
#define FIGHT_BIGWIN_HP_COST 5
#define FIGHT_BIGWIN_MOOD   20

/* ============================================================
 * 存档系统
 * ============================================================ */
#define AUTO_SAVE_INTERVAL_MS   90000UL /* 自动存档 90s */
#define MAX_OFFLINE_HOURS       24      /* 离线衰减上限 */

#define SAVE_MAGIC          0xA5        /* 有效标记 */
#define SAVE_VERSION        1           /* 存档格式版本 */

/* ============================================================
 * MPU6050 摇晃检测
 * ============================================================ */
#define SHAKE_ACCEL_THRESHOLD   20000   /* 加速度变化阈值 (raw LSB) */

/* ============================================================
 * 传感器采样周期 (ms)
 * ============================================================ */
#define POLL_KEY_MS         50UL        /* 键盘轮询 */
#define POLL_MPU_MS         200UL       /* MPU6050 */
#define POLL_OLED_MS        500UL       /* OLED 刷新 */
#define POLL_BH1750_MS      5000UL     /* 光照 */
#define POLL_DHT11_MS       5000UL     /* 温湿度 */
#define POLL_DECAY_MS       60000UL     /* 属性衰减 (每分钟) */
/* ============================================================
 * 逗乐小游戏
 * ============================================================ */
#define MINIGAME_SEQ_LEN    4           /* 数字序列长度 */
#define MINIGAME_TIMEOUT_MS 10000UL     /* 输入超时 10s */

/* ============================================================
 * 外观索引
 * ============================================================ */
#define APPEAR_HAPPY        0
#define APPEAR_NORMAL       1
#define APPEAR_HUNGRY       2
#define APPEAR_SAD          3
#define APPEAR_SLEEP        4
#define APPEAR_DEAD         5
#define APPEAR_COUNT        6

/* ============================================================
 * 键盘映射 (TTP229 键号 → 功能)
 * ============================================================ */
#define KEY_FEED            0
#define KEY_PLAY            1
#define KEY_WORK            2
#define KEY_FIGHT           3
#define KEY_UP              4
#define KEY_DOWN            5
#define KEY_OK              6
#define KEY_BACK            7
#define KEY_NUM_8           8
#define KEY_NUM_9           9
#define KEY_NUM_A           10
#define KEY_NUM_B           11
#define KEY_PET_C           12
#define KEY_PET_D           13
#define KEY_NTP             14
#define KEY_SAVE            15
#define KEY_NONE            (-1)

#endif /* GAME_CONFIG_H */
