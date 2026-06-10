/**
 * @file    pet_ui.cpp
 * @brief   OLED UI 渲染实现 — 按游戏状态分屏绘制
 */

#include <Arduino.h>
#include <string.h>
#include "pet_ui.h"
#include "pet_core.h"
#include "game_config.h"
#include "oled_ssd1315.h"
#include "ds1307.h"

/* ============================================================
 * 内部状态
 * ============================================================ */
static ui_extra_t s_extra;

static char s_toast_buf[22];
static unsigned long s_toast_expire = 0;

/* 食物名称/价格/恢复量 (菜单显示用) */
static const char * const s_food_name[] = {
    "Bread", "Steak", "Cake", "Potion"
};
static const uint8_t s_food_price[] = {
    FOOD_BREAD_PRICE, FOOD_STEAK_PRICE, FOOD_CAKE_PRICE, FOOD_POTION_PRICE
};
static const uint8_t s_food_hp[] = {
    FOOD_BREAD_HP, FOOD_STEAK_HP, FOOD_CAKE_HP, FOOD_POTION_HP
};

/* ============================================================
 * 工具: 分隔线 / Toast
 * ============================================================ */

static void draw_sep(uint8_t page)
{
    oled_hline(0, 127, page * 8 + 4);
}

/* 绘制 Toast 到 Page 7, 返回 true 表示已绘制 (覆盖默认提示) */
static bool draw_toast(void)
{
    if (s_toast_buf[0] == '\0') return false;
    if (millis() >= s_toast_expire) {
        s_toast_buf[0] = '\0';
        return false;
    }
    uint8_t len = strlen(s_toast_buf);
    uint8_t x = (len * 6 < 128) ? (128 - len * 6) / 2 : 0;
    oled_str(x, 7, s_toast_buf);
    return true;
}

/* 居中绘制 1x 字符串 */
static void draw_center(uint8_t page, const char *str)
{
    uint8_t len = strlen(str);
    uint8_t x = (len * 6 < 128) ? (128 - len * 6) / 2 : 0;
    oled_str(x, page, str);
}

/* ============================================================
 * STATE_INIT — 开机画面
 * ============================================================ */
static void draw_init(void)
{
    oled_str_2x(16, 2, "PET GAME");
    draw_center(5, "Loading...");
}

/* ============================================================
 * STATE_MAIN — 主界面
 * ============================================================ */
static void draw_main(void)
{
    pet_status_t *st = pet_get_status();
    env_data_t   *env = pet_get_env();

    /* Page 0: 标题 + 时间 */
    oled_str(0, 0, "PET");

    ds1307_time_t t;
    ds1307_read_time(&t);
    oled_printf(90, 0, "%02d:%02d", t.hour, t.min);

    /* Page 1: 分隔线 */
    draw_sep(1);

    /* Page 2: HP 进度条 */
    oled_str(0, 2, "HP");
    oled_progress_bar(16, 17, 88, 6, (float)st->hp / HP_MAX);
    oled_printf(108, 2, "%3d", st->hp);

    /* Page 3: Mood 进度条 */
    oled_str(0, 3, "MD");
    oled_progress_bar(16, 25, 88, 6, (float)st->mood / MOOD_MAX);
    oled_printf(108, 3, "%3d", st->mood);

    /* Page 4: 金币 */
    oled_printf(0, 4, "Coin: %lu", (unsigned long)st->coins);

    /* Page 5: 环境数据 */
    oled_printf(0, 5, "T:%dC H:%d%% L:%u",
                env->temp, env->humidity, env->lux);

    /* Page 6: 分隔线 */
    draw_sep(6);

    /* Page 7: 操作提示 / Toast */
    if (!draw_toast()) {
        oled_str(0, 7, "0Fd 1Pl 2Wk 3Ft F:Sv");
    }
}

/* ============================================================
 * STATE_FEED_MENU — 喂食菜单
 * ============================================================ */
static void draw_feed_menu(void)
{
    pet_status_t *st = pet_get_status();

    oled_str(28, 0, "== FEED ==");
    draw_sep(1);

    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        char cur = (s_extra.menu_cursor == i) ? '>' : ' ';
        oled_printf(0, 2 + i, "%c%-6s%3dG +%dHP",
                    cur, s_food_name[i], s_food_price[i], s_food_hp[i]);
    }

    draw_sep(6);
    if (!draw_toast()) {
        oled_printf(0, 7, "Coin:%lu 6OK 7Bk",
                    (unsigned long)st->coins);
    }
}

/* ============================================================
 * STATE_FEEDING — 喂食结果 (短暂显示)
 * ============================================================ */
static void draw_feeding(void)
{
    oled_str(28, 0, "== FEED ==");
    draw_sep(1);
    draw_center(3, s_extra.info);
    draw_sep(6);
}

/* ============================================================
 * STATE_PLAY_MENU — 互动菜单
 * ============================================================ */
static void draw_play_menu(void)
{
    oled_str(28, 0, "== PLAY ==");
    draw_sep(1);

    char cur0 = (s_extra.menu_cursor == 0) ? '>' : ' ';
    oled_printf(0, 2, "%cJoke game", cur0);
    oled_str(0, 4, " C/D: Pet touch");
    oled_str(0, 5, " Shake: Tilt it!");

    draw_sep(6);
    oled_str(0, 7, "6:Start 7:Back");
}

/* ============================================================
 * STATE_PLAYING — 互动结果 (短暂显示)
 * ============================================================ */
static void draw_playing(void)
{
    oled_str(28, 0, "== PLAY ==");
    draw_sep(1);
    draw_center(3, s_extra.info);
    draw_sep(6);
}

/* ============================================================
 * STATE_MINIGAME — 逗乐小游戏
 * ============================================================ */
static void draw_minigame(void)
{
    oled_str(16, 0, "== MINIGAME ==");
    draw_sep(1);

    /* 显示目标序列 */
    if (s_extra.minigame_seq) {
        oled_printf(10, 2, "Remember: %d %d %d %d",
                    s_extra.minigame_seq[0], s_extra.minigame_seq[1],
                    s_extra.minigame_seq[2], s_extra.minigame_seq[3]);
    }

    /* 输入进度: _ _ _ _ 逐步填充为 * */
    char buf[20];
    uint8_t off = 0;
    memcpy(buf, "Input:  ", 8);
    off = 8;
    for (uint8_t i = 0; i < MINIGAME_SEQ_LEN; i++) {
        buf[off++] = (i < s_extra.minigame_input_pos) ? '*' : '_';
        buf[off++] = ' ';
    }
    buf[off] = '\0';
    oled_str(10, 4, buf);

    draw_sep(6);
    oled_str(0, 7, "8=1 9=2 A=3 B=4");
}

/* ============================================================
 * STATE_MINIGAME_RESULT — 逗乐结果
 * ============================================================ */
static void draw_minigame_result(void)
{
    oled_str(22, 0, "== RESULT ==");
    draw_sep(1);

    if (s_extra.minigame_success) {
        oled_str_2x(16, 2, "SUCCESS!");
        oled_printf(28, 5, "Mood +%d", INTERACT_JOKE_MOOD_OK);
    } else {
        oled_str_2x(28, 2, "FAILED");
        oled_printf(28, 5, "Mood +%d", INTERACT_JOKE_MOOD_FAIL);
    }

    draw_sep(6);
    oled_str(4, 7, "Any key to continue");
}

/* ============================================================
 * STATE_WORKING — 打工倒计时
 * ============================================================ */
static void draw_working(void)
{
    oled_str(22, 0, "== WORKING ==");
    draw_sep(1);

    unsigned long elapsed = millis() - s_extra.work_start_ms;
    uint16_t remain_s = 0;
    if (elapsed < WORK_DURATION_MS) {
        remain_s = (uint16_t)((WORK_DURATION_MS - elapsed) / 1000);
    }

    oled_printf(34, 3, "Time: %ds", remain_s);

    float progress = (float)elapsed / WORK_DURATION_MS;
    if (progress > 1.0f) progress = 1.0f;
    oled_progress_bar(14, 34, 100, 6, progress);

    draw_sep(6);
    oled_str(16, 7, "Please wait...");
}

/* ============================================================
 * STATE_FIGHTING — 战斗动画
 * ============================================================ */
static void draw_fighting(void)
{
    oled_str(22, 0, "== BATTLE ==");
    draw_sep(1);

    if ((millis() / 500) & 1) {
        oled_str_2x(16, 3, "VS BOSS!");
    } else {
        oled_str_2x(28, 3, "FIGHT!");
    }
}

/* ============================================================
 * STATE_FIGHT_RESULT — 战斗结算
 * ============================================================ */
static void draw_fight_result(void)
{
    oled_str(22, 0, "== RESULT ==");
    draw_sep(1);

    switch (s_extra.fight_result) {
        case FIGHT_WIN:    oled_str_2x(40, 2, "WIN!");     break;
        case FIGHT_BIGWIN: oled_str_2x(16, 2, "BIG WIN!"); break;
        case FIGHT_LOSE:   oled_str_2x(22, 2, "LOSE...");  break;
        default: break;
    }

    oled_printf(28, 5, "Coin: +%d", s_extra.fight_coins);
    draw_sep(6);
    oled_str(4, 7, "Any key to continue");
}

/* ============================================================
 * STATE_DEAD — 死亡
 * ============================================================ */
static void draw_dead(void)
{
    oled_str_2x(10, 2, "GAME OVER");
    draw_center(5, "Your pet has died");
    draw_center(7, "Any key to revive");
}

/* ============================================================
 * STATE_NTP_SYNC — NTP 校时
 * ============================================================ */
static void draw_ntp_sync(void)
{
    oled_str(16, 0, "== NTP SYNC ==");
    draw_sep(1);
    draw_center(3, "Syncing time...");
    draw_center(5, "Please wait");
}

/* ============================================================
 * Public API
 * ============================================================ */

void ui_init(void)
{
    memset(&s_extra, 0, sizeof(s_extra));
    s_toast_buf[0] = '\0';

    oled_clear();
    draw_init();
    oled_flush();
}

void ui_draw(void)
{
    oled_clear();

    switch (pet_get_state()) {
        case STATE_INIT:            draw_init();            break;
        case STATE_MAIN:            draw_main();            break;
        case STATE_FEED_MENU:       draw_feed_menu();       break;
        case STATE_FEEDING:         draw_feeding();         break;
        case STATE_PLAY_MENU:       draw_play_menu();       break;
        case STATE_PLAYING:         draw_playing();         break;
        case STATE_MINIGAME:        draw_minigame();        break;
        case STATE_MINIGAME_RESULT: draw_minigame_result(); break;
        case STATE_WORKING:         draw_working();         break;
        case STATE_FIGHTING:        draw_fighting();        break;
        case STATE_FIGHT_RESULT:    draw_fight_result();    break;
        case STATE_DEAD:            draw_dead();            break;
        case STATE_NTP_SYNC:        draw_ntp_sync();        break;
    }

    oled_flush();
}

ui_extra_t *ui_get_extra(void)
{
    return &s_extra;
}

void ui_toast(const char *msg, uint16_t duration_ms)
{
    strncpy(s_toast_buf, msg, sizeof(s_toast_buf) - 1);
    s_toast_buf[sizeof(s_toast_buf) - 1] = '\0';
    s_toast_expire = millis() + duration_ms;
}
