/**
 * @file    work3.ino
 * @brief   电子时钟主程序 (ESP32-C3 Arduino)
 */

#include <Wire.h>
#include "ds1307.h"
#include "at24c32d.h"
#include "ttp229.h"
#include "buzzer.h"
#include "oled_ssd1315.h"
#include "wifi_ntp.h"

/* ======================== 引脚定义 ======================== */
#define PIN_SDA         8
#define PIN_SCL         9
#define PIN_BUZZER      0

/* ======================== EEPROM 闹钟存储 ======================== */
#define EEPROM_ALARM_ADDR   0x000
#define EEPROM_MAGIC_BYTE   0xA5

/* ======================== 闹钟结构体 ======================== */
typedef struct {
    uint8_t hour;
    uint8_t min;
    bool    enabled;
} alarm_t;

/* ======================== 系统状态机 ======================== */
typedef enum {
    STATE_DISPLAY,
    STATE_SET_HOUR,
    STATE_SET_MIN,
    STATE_SET_ALARM_H,
    STATE_SET_ALARM_M,
} sys_state_t;

/* ======================== 全局变量 ======================== */
static ds1307_time_t g_time;                /* RTC 读取的时间 */
static ds1307_time_t g_edit_time;           /* 编辑中的时间副本 ★新增 */
static alarm_t       g_alarm = {7, 0, false};
static sys_state_t   g_state = STATE_DISPLAY;
static bool          g_alarm_ringing = false;
static unsigned long g_ring_start    = 0;

/* ======================== 闹钟 EEPROM 存取 ======================== */

static void alarm_save(const alarm_t *a)
{
    uint8_t buf[4] = {
        a->hour,
        a->min,
        (uint8_t)(a->enabled ? 0x01 : 0x00),
        EEPROM_MAGIC_BYTE
    };
    at24c32d_write_page(EEPROM_ALARM_ADDR, buf, 4);
    Serial.printf("[EEPROM] Saved: %02d:%02d %s\n",
                  a->hour, a->min, a->enabled ? "ON" : "OFF");
}

static bool alarm_load(alarm_t *a)
{
    uint8_t buf[4];
    at24c32d_read_sequential(EEPROM_ALARM_ADDR, buf, 4);

    if (buf[3] != EEPROM_MAGIC_BYTE) {
        Serial.println("[EEPROM] No valid alarm data");
        return false;
    }
    a->hour    = buf[0];
    a->min     = buf[1];
    a->enabled = (buf[2] == 0x01);

    if (a->hour > 23 || a->min > 59) {
        a->hour = 7; a->min = 0; a->enabled = false;
        return false;
    }
    Serial.printf("[EEPROM] Loaded: %02d:%02d %s\n",
                  a->hour, a->min, a->enabled ? "ON" : "OFF");
    return true;
}

/* ======================== 闹钟检测 ======================== */

static void alarm_check(const ds1307_time_t *t)
{
    if (!g_alarm.enabled || g_alarm_ringing) return;

    if (t->hour == g_alarm.hour && t->min == g_alarm.min && t->sec == 0) {
        g_alarm_ringing = true;
        g_ring_start = millis();
        Serial.println("[ALARM] RINGING!");
    }
}

static void alarm_ring_process(void)
{
    if (!g_alarm_ringing) return;

    if (millis() - g_ring_start > 60000) {
        buzzer_off();
        g_alarm_ringing = false;
        return;
    }

    buzzer_beep(2000, 150, 200);
}

/* ======================== OLED 显示 ======================== */

static void oled_draw_clock(void)
{
    /* 根据状态决定显示哪组时间数据 */
    uint8_t disp_hour, disp_min, disp_sec;
    uint8_t disp_year, disp_month, disp_date;

    bool is_time_setting = (g_state == STATE_SET_HOUR || g_state == STATE_SET_MIN);

    if (is_time_setting) {
        /* 设置模式: 显示编辑副本 */
        disp_hour  = g_edit_time.hour;
        disp_min   = g_edit_time.min;
        disp_sec   = 0;  /* 设置时秒显示为 00 */
        disp_year  = g_edit_time.year;
        disp_month = g_edit_time.month;
        disp_date  = g_edit_time.date;
    } else {
        /* 正常/闹钟设置模式: 显示 RTC 实时时间 */
        disp_hour  = g_time.hour;
        disp_min   = g_time.min;
        disp_sec   = g_time.sec;
        disp_year  = g_time.year;
        disp_month = g_time.month;
        disp_date  = g_time.date;
    }

    bool blink = (millis() / 500) % 2 == 0;  /* 用 millis 控制闪烁, 不依赖秒数 */

    oled_clear();

    /* 第 0 行: 日期 */
    oled_printf(0, 0, "20%02d-%02d-%02d", disp_year, disp_month, disp_date);

    if (wifi_ntp_is_syncing()) {
        oled_str(90, 0, "SYNC");
    }

    /* 第 2~3 行: 大号时间 */
    if (g_state == STATE_SET_HOUR && blink) {
        oled_str_2x(4, 2, "  ");
    } else {
        oled_printf_2x(4, 2, "%02d", disp_hour);
    }

    oled_str_2x(28, 2, ":");

    if (g_state == STATE_SET_MIN && blink) {
        oled_str_2x(40, 2, "  ");
    } else {
        oled_printf_2x(40, 2, "%02d", disp_min);
    }

    oled_str_2x(64, 2, ":");
    oled_printf_2x(76, 2, "%02d", disp_sec);

    /* 第 5 行: 闹钟信息 */
    if (g_state == STATE_SET_ALARM_H || g_state == STATE_SET_ALARM_M) {
        oled_str(0, 5, "Set Alarm:");
        if (g_state == STATE_SET_ALARM_H && blink) {
            oled_str(66, 5, "  ");
        } else {
            oled_printf(66, 5, "%02d", g_alarm.hour);
        }
        oled_str(78, 5, ":");
        if (g_state == STATE_SET_ALARM_M && blink) {
            oled_str(84, 5, "  ");
        } else {
            oled_printf(84, 5, "%02d", g_alarm.min);
        }
    } else {
        if (g_alarm.enabled) {
            oled_printf(0, 5, "ALM %02d:%02d ON", g_alarm.hour, g_alarm.min);
        } else {
            oled_str(0, 5, "ALM OFF");
        }
        if (g_alarm_ringing) {
            oled_str(102, 5, "!!");
        }
    }

    /* 第 7 行: 模式提示 */
    switch (g_state) {
    case STATE_SET_HOUR:    oled_str(0, 7, ">> SET HOUR  +/-/OK"); break;
    case STATE_SET_MIN:     oled_str(0, 7, ">> SET MIN   +/-/OK"); break;
    case STATE_SET_ALARM_H: oled_str(0, 7, ">> ALM HOUR  +/-/OK"); break;
    case STATE_SET_ALARM_M: oled_str(0, 7, ">> ALM MIN   +/-/OK"); break;
    default:
        oled_str(0, 7, "0:Set 3:Alm 4:NTP");
        break;
    }

    oled_flush();
}

/* ======================== 按键处理 ======================== */

static void handle_key(int8_t key)
{
    if (key < 0) return;
    Serial.printf("[KEY] Silk: %X  State: %d\n", key, g_state);

    switch (key) {
    case 0x0:  /* 模式切换 */
        switch (g_state) {
        case STATE_DISPLAY:
            /* 进入设置模式: 拷贝当前时间到编辑副本 */
            g_edit_time = g_time;
            g_state = STATE_SET_HOUR;
            Serial.printf("[EDIT] Start: %02d:%02d\n", g_edit_time.hour, g_edit_time.min);
            break;
        case STATE_SET_HOUR:
            g_state = STATE_SET_MIN;
            break;
        case STATE_SET_MIN:
            /* 确认时间: 将编辑副本写入 DS1307 */
            g_edit_time.sec = 0;
            ds1307_set_time(&g_edit_time);
            Serial.printf("[EDIT] Time set: %02d:%02d:00\n", g_edit_time.hour, g_edit_time.min);
            g_state = STATE_SET_ALARM_H;
            break;
        case STATE_SET_ALARM_H:
            g_state = STATE_SET_ALARM_M;
            break;
        case STATE_SET_ALARM_M:
            alarm_save(&g_alarm);
            g_state = STATE_DISPLAY;
            break;
        }
        break;

    case 0x1:  /* +1 */
        switch (g_state) {
        case STATE_SET_HOUR:    g_edit_time.hour = (g_edit_time.hour + 1) % 24;  break;
        case STATE_SET_MIN:     g_edit_time.min  = (g_edit_time.min + 1) % 60;   break;
        case STATE_SET_ALARM_H: g_alarm.hour     = (g_alarm.hour + 1) % 24;     break;
        case STATE_SET_ALARM_M: g_alarm.min      = (g_alarm.min + 1) % 60;      break;
        default: break;
        }
        break;

    case 0x2:  /* -1 */
        switch (g_state) {
        case STATE_SET_HOUR:    g_edit_time.hour = (g_edit_time.hour + 23) % 24; break;
        case STATE_SET_MIN:     g_edit_time.min  = (g_edit_time.min + 59) % 60;  break;
        case STATE_SET_ALARM_H: g_alarm.hour     = (g_alarm.hour + 23) % 24;    break;
        case STATE_SET_ALARM_M: g_alarm.min      = (g_alarm.min + 59) % 60;     break;
        default: break;
        }
        break;

    case 0x3:  /* 闹钟 Toggle */
        g_alarm.enabled = !g_alarm.enabled;
        alarm_save(&g_alarm);
        break;

    case 0x4:  /* NTP 校时 */
        oled_clear();
        oled_str_2x(16, 1, "Wi-Fi");
        oled_str_2x(10, 3, "NTP Sync");
        oled_str(16, 6, "Connecting...");
        oled_flush();
        wifi_ntp_sync();
        break;

    case 0x5:  /* 停止闹铃 */
        buzzer_off();
        g_alarm_ringing = false;
        break;
    }
}

/* ======================== setup ======================== */

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========== Digital Alarm Clock ==========");

    buzzer_init(PIN_BUZZER);

    oled_init(PIN_SDA, PIN_SCL, 100000UL, OLED_DEFAULT_ADDR);

    oled_clear();
    oled_str_2x(10, 1, "Digital");
    oled_str_2x(16, 3, "Clock");
    oled_str(10, 6, "ESP32-C3 + SSD1315");
    oled_flush();
    delay(1500);

    ds1307_init();

    alarm_load(&g_alarm);

    oled_clear();
    oled_str_2x(16, 2, "NTP Sync");
    oled_str(16, 5, "Connecting...");
    oled_flush();
    wifi_ntp_sync();

    delay(500);
    ttp229_init();

    Serial.println("[SYSTEM] Ready!");
}

/* ======================== loop ======================== */

void loop()
{
    static unsigned long last_read = 0;

    /* 每 500ms 刷新 */
    if (millis() - last_read >= 500) {
        last_read = millis();

        /* ★ 关键修复: 仅在非时间设置模式下才读取 DS1307 */
        if (g_state != STATE_SET_HOUR && g_state != STATE_SET_MIN) {
            ds1307_read_time(&g_time);
        }

        /* 刷新 OLED */
        oled_draw_clock();

        /* 闹钟检测 (仅正常模式下检测) */
        if (g_state == STATE_DISPLAY) {
            alarm_check(&g_time);
        }

        /* 串口调试 */
        Serial.printf("\r  20%02d-%02d-%02d  %02d:%02d:%02d\n",
                      g_time.year, g_time.month, g_time.date,
                      g_time.hour, g_time.min, g_time.sec);
    }

    /* 触摸按键轮询 */
    int8_t key = ttp229_poll();
    if (key >= 0) {
        handle_key(key);
    }

    /* 蜂鸣器处理 */
    alarm_ring_process();
}
