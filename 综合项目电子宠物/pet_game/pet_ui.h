/**
 * @file    pet_ui.h
 * @brief   OLED UI 渲染 — 主界面 / 菜单 / 进度条 / 动画 / Toast
 */

#ifndef PET_UI_H
#define PET_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "pet_core.h"

/* ============================================================
 * UI 附加状态 (主循环设置, UI 渲染时读取)
 * ============================================================ */
typedef struct {
    uint8_t         menu_cursor;        /* 菜单光标位置 */
    const uint8_t  *minigame_seq;       /* 逗乐序列指针 */
    uint8_t         minigame_input_pos; /* 已输入的数字数 */
    bool            minigame_success;   /* 逗乐结果 */
    fight_result_t  fight_result;       /* 战斗结果 */
    int16_t         fight_coins;        /* 战斗金币奖励 */
    unsigned long   work_start_ms;      /* 打工开始时间戳 */
    char            info[22];           /* 通用信息行 (喂食/互动结果) */
} ui_extra_t;

/* ============================================================
 * API
 * ============================================================ */

/** 初始化 UI, 显示开机画面 */
void ui_init(void);

/** 根据当前游戏状态绘制整屏并刷新 OLED (500ms 调用一次) */
void ui_draw(void);

/** 获取 UI 附加状态指针, 主循环直接修改 */
ui_extra_t *ui_get_extra(void);

/** 在底部显示 Toast 消息, 持续 duration_ms 毫秒 */
void ui_toast(const char *msg, uint16_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* PET_UI_H */
