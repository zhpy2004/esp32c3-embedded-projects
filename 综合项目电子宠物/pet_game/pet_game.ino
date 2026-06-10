/**
 * @file    pet_game.ino
 * @brief   ESP32-C3 电子宠物 — 主程序 (setup/loop, 状态机调度)
 */

#include <Wire.h>
#include "game_config.h"
#include "pet_core.h"
#include "pet_save.h"
#include "pet_sensor.h"
#include "pet_ui.h"
#include "pet_web.h"
#include "pet_images.h"
#include "epd_ssd1680.h"
#include "oled_ssd1315.h"
#include "ds1307.h"
#include "buzzer.h"
#include "wifi_ntp.h"

/* ============================================================
 * 食物名/HP 表 (喂食结果显示用)
 * ============================================================ */
static const char * const s_food_name[FOOD_COUNT] = {
    "Bread", "Steak", "Cake", "Potion"
};
static const uint8_t s_food_hp[FOOD_COUNT] = {
    FOOD_BREAD_HP, FOOD_STEAK_HP, FOOD_CAKE_HP, FOOD_POTION_HP
};

/* ============================================================
 * 状态机辅助
 * ============================================================ */
static unsigned long s_state_enter_ms = 0;
static unsigned long s_last_oled      = 0;
static uint8_t       s_prev_appear    = 0xFF;
static unsigned long s_key_tone_off   = 0;

/* 逗乐输入缓冲 */
static uint8_t s_mg_input[MINIGAME_SEQ_LEN];
static uint8_t s_mg_pos = 0;

static void enter_state(game_state_t st)
{
    pet_set_state(st);
    s_state_enter_ms = millis();
}

/* 检查并刷新墨水屏 (外观变化时) */
static void refresh_epd_if_needed(void)
{
    pet_update_appearance();
    uint8_t app = pet_get_status()->appearance;
    if (app != s_prev_appear) {
        epd_display(PET_IMG_BW[app], PET_IMG_RED[app]);
        s_prev_appear = app;
    }
}

/* ============================================================
 * 按键处理 — STATE_MAIN
 * ============================================================ */
static void on_key_main(int8_t key)
{
    ui_extra_t *ex = ui_get_extra();

    switch (key) {
    case KEY_FEED:
        ex->menu_cursor = 0;
        enter_state(STATE_FEED_MENU);
        break;

    case KEY_PLAY:
        ex->menu_cursor = 0;
        enter_state(STATE_PLAY_MENU);
        break;

    case KEY_WORK:
        if (pet_work_start()) {
            ex->work_start_ms = millis();
            enter_state(STATE_WORKING);
            buzzer_tone(800, 80);
        } else {
            ui_toast("Too weak!", 1500);
        }
        break;

    case KEY_FIGHT:
        if (pet_fight_start()) {
            enter_state(STATE_FIGHTING);
            buzzer_tone(1200, 80);
        } else {
            ui_toast("Too weak!", 1500);
        }
        break;

    case KEY_PET_C:
    case KEY_PET_D:
        if (pet_interact_pet()) {
            snprintf(ex->info, sizeof(ex->info),
                     "Pet! Mood +%d", INTERACT_PET_MOOD);
            enter_state(STATE_PLAYING);
            buzzer_tone(2000, 50);
        } else {
            ui_toast("Cooldown!", 1000);
        }
        break;

    case KEY_NTP:
        enter_state(STATE_NTP_SYNC);
        ui_draw();
        {
            bool ok = web_ntp_sync();
            ui_toast(ok ? "Time synced!" : "Sync failed!", 2000);
        }
        enter_state(STATE_MAIN);
        break;

    case KEY_SAVE:
        save_write();
        ui_toast("Saved!", 1500);
        buzzer_tone(1500, 50);
        break;

    default:
        break;
    }
}

/* ============================================================
 * 按键处理 — STATE_FEED_MENU
 * ============================================================ */
static void on_key_feed_menu(int8_t key)
{
    ui_extra_t *ex = ui_get_extra();

    switch (key) {
    case KEY_UP:
        if (ex->menu_cursor > 0) ex->menu_cursor--;
        break;

    case KEY_DOWN:
        if (ex->menu_cursor < FOOD_COUNT - 1) ex->menu_cursor++;
        break;

    case KEY_OK: {
        uint8_t idx = ex->menu_cursor;
        uint8_t res = pet_feed(idx);
        if (res == 0) {
            snprintf(ex->info, sizeof(ex->info),
                     "Ate %s! HP+%d", s_food_name[idx], s_food_hp[idx]);
            enter_state(STATE_FEEDING);
            save_write();
            buzzer_tone(1000, 80);
        } else if (res == 1) {
            ui_toast("Not enough coins!", 1500);
        } else {
            ui_toast("HP is full!", 1500);
        }
        break;
    }

    case KEY_BACK:
        enter_state(STATE_MAIN);
        break;

    default:
        break;
    }
}

/* ============================================================
 * 按键处理 — STATE_PLAY_MENU
 * ============================================================ */
static void on_key_play_menu(int8_t key)
{
    ui_extra_t *ex = ui_get_extra();

    switch (key) {
    case KEY_OK: {
        const uint8_t *seq = pet_minigame_start();
        ex->minigame_seq = seq;
        ex->minigame_input_pos = 0;
        s_mg_pos = 0;
        enter_state(STATE_MINIGAME);
        break;
    }

    case KEY_BACK:
        enter_state(STATE_MAIN);
        break;

    default:
        break;
    }
}

/* ============================================================
 * 按键处理 — STATE_MINIGAME
 * ============================================================ */
static void on_key_minigame(int8_t key)
{
    ui_extra_t *ex = ui_get_extra();

    /* 键 8=1, 9=2, A=3, B=4 */
    if (key >= KEY_NUM_8 && key <= KEY_NUM_B) {
        uint8_t digit = key - KEY_NUM_8 + 1;
        s_mg_input[s_mg_pos++] = digit;
        ex->minigame_input_pos = s_mg_pos;
        buzzer_tone(1500, 30);

        if (s_mg_pos >= MINIGAME_SEQ_LEN) {
            uint8_t res = pet_minigame_check(s_mg_input, s_mg_pos);
            ex->minigame_success = (res == 0);
            enter_state(STATE_MINIGAME_RESULT);
            save_write();
            if (res == 0) {
                buzzer_tone(2000, 200);
            } else {
                buzzer_tone(500, 200);
            }
        }
    }

    if (key == KEY_BACK) {
        enter_state(STATE_MAIN);
    }
}

/* ============================================================
 * 按键处理 — 结果页面 (任意键返回)
 * ============================================================ */
static void on_key_any_back(int8_t key)
{
    (void)key;
    enter_state(STATE_MAIN);
}

/* ============================================================
 * 传感器事件处理
 * ============================================================ */
static void handle_events(uint16_t events, int8_t key)
{
    /* 仅在 STATE_MAIN 时处理互动事件 */
    if (pet_get_state() == STATE_MAIN) {
        if (events & EVT_SHAKE) {
            if (pet_interact_shake()) {
                ui_extra_t *ex = ui_get_extra();
                snprintf(ex->info, sizeof(ex->info),
                         "Shake! Mood +%d", INTERACT_SHAKE_MOOD);
                enter_state(STATE_PLAYING);
                buzzer_tone(1800, 60);
            }
        }

    }

    /* 衰减和自动存档不受状态限制 */
    if (events & EVT_DECAY) {
        pet_decay_tick();
        refresh_epd_if_needed();
    }

    if (events & EVT_AUTO_SAVE) {
        save_write();
    }
}

/* ============================================================
 * 时间驱动的状态转换 (非按键触发)
 * ============================================================ */
static void update_timed_states(void)
{
    unsigned long elapsed = millis() - s_state_enter_ms;

    switch (pet_get_state()) {
    case STATE_FEEDING:
    case STATE_PLAYING:
        if (elapsed >= 1500) {
            enter_state(STATE_MAIN);
            refresh_epd_if_needed();
        }
        break;

    case STATE_WORKING:
        if (elapsed >= WORK_DURATION_MS) {
            uint16_t coin = pet_work_finish();
            char buf[22];
            snprintf(buf, sizeof(buf), "Earned %d coin!", coin);
            ui_toast(buf, 2000);
            enter_state(STATE_MAIN);
            save_write();
            refresh_epd_if_needed();
            buzzer_tone(1500, 100);
        }
        break;

    case STATE_FIGHTING:
        if (elapsed >= 2000) {
            ui_extra_t *ex = ui_get_extra();
            int16_t coin_reward = 0;
            fight_result_t res = pet_fight_resolve(&coin_reward);
            ex->fight_result = res;
            ex->fight_coins  = coin_reward;
            enter_state(STATE_FIGHT_RESULT);
            save_write();
            refresh_epd_if_needed();
            if (res == FIGHT_BIGWIN) {
                buzzer_tone(2500, 300);
            } else if (res == FIGHT_WIN) {
                buzzer_tone(2000, 150);
            } else {
                buzzer_tone(400, 300);
            }
        }
        break;

    case STATE_MINIGAME:
        if (elapsed >= MINIGAME_TIMEOUT_MS) {
            ui_extra_t *ex = ui_get_extra();
            ex->minigame_success = false;
            enter_state(STATE_MINIGAME_RESULT);
            buzzer_tone(500, 200);
        }
        break;

    default:
        break;
    }
}

/* ============================================================
 * setup
 * ============================================================ */
void setup()
{
    Serial.begin(115200);
    Serial.println("\n[PET] Starting...");

    /* I2C 总线 (6 设备共享) */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_FREQ);

    /* OLED 初始化 + Loading 画面 */
    oled_init(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ, I2C_ADDR_OLED);
    ui_init();

    /* RTC */
    ds1307_init();

    /* NVS 存档加载 */
    bool has_save = save_init();
    if (has_save) {
        uint32_t off_min = save_get_offline_minutes();
        Serial.printf("[Save] Restored, offline %lu min\n",
                      (unsigned long)off_min);
    } else {
        Serial.println("[Save] No valid save, new pet created");
    }

    /* 蜂鸣器 */
    buzzer_init(PIN_BUZZER);
    buzzer_tone(1000, 100);

    /* 传感器 */
    sensor_init();

    /* 墨水屏初始外观 */
    epd_init(PIN_EPD_SCK, PIN_EPD_MOSI, PIN_EPD_CS,
             PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY);
    pet_update_appearance();
    uint8_t app = pet_get_status()->appearance;
    epd_display(PET_IMG_BW[app], PET_IMG_RED[app]);
    s_prev_appear = app;

    /* WiFi + Web Server 作弊控制台 (连接 WiFi, 可能阻塞数秒) */
    web_init();

    /* 进入主界面 */
    enter_state(STATE_MAIN);
    s_last_oled = millis();

    Serial.println("[PET] Ready!");
}

/* ============================================================
 * loop
 * ============================================================ */
void loop()
{
    /* 传感器轮询 → 事件位掩码 */
    uint16_t events = sensor_poll();
    int8_t key = (events & EVT_KEY_PRESS) ? sensor_get_key() : KEY_NONE;

    /* 按键音阶反馈 (非阻塞) */
    static const uint16_t key_tone[] = {
        262, 294, 330, 349, 392, 440, 494, 523,
        587, 659, 698, 784, 880, 988, 1047, 1175
    };
    if (key != KEY_NONE && key >= 0 && key < 16) {
        buzzer_on(key_tone[key]);
        s_key_tone_off = millis() + 50;
    }
    if (s_key_tone_off && millis() >= s_key_tone_off) {
        buzzer_off();
        s_key_tone_off = 0;
    }

    /* 按键分发 */
    if (key != KEY_NONE) {
        switch (pet_get_state()) {
        case STATE_MAIN:            on_key_main(key);       break;
        case STATE_FEED_MENU:       on_key_feed_menu(key);  break;
        case STATE_PLAY_MENU:       on_key_play_menu(key);  break;
        case STATE_MINIGAME:        on_key_minigame(key);   break;
        case STATE_FIGHT_RESULT:    on_key_any_back(key);   break;
        case STATE_MINIGAME_RESULT: on_key_any_back(key);   break;
        case STATE_DEAD:
            pet_cheat_revive();
            save_write();
            enter_state(STATE_MAIN);
            refresh_epd_if_needed();
            buzzer_tone(1200, 100);
            break;
        default:
            break;
        }
    }

    /* 传感器事件 (摇晃/陪伴/衰减/自动存档) */
    handle_events(events, key);

    /* 时间驱动状态转换 (打工完成/战斗结算/喂食动画超时) */
    update_timed_states();

    /* OLED 刷新 (500ms) */
    unsigned long now = millis();
    if (now - s_last_oled >= POLL_OLED_MS) {
        s_last_oled = now;
        ui_draw();
    }

    /* Web Server 处理 HTTP 请求 */
    web_run();
    refresh_epd_if_needed();
}
