/**
 * @file    bus_announcer.ino
 * @brief   公交车报站器主程序 (ESP32-C3 Arduino)
 *          LED 点阵滚动显示 + 语音播报 (非阻塞)
 */

extern "C" {
#include "gt30l32s4w.h"
#include "syn6658.h"
#include "led_matrix.h"
#include "station.h"
#include "ttp229.h"
}

#include <Arduino.h>
#include <Wire.h>

/* ============================================================
 *  所有引脚 & 参数定义集中在此处
 * ============================================================ */

/* --- I2C 总线 (TTP229 + HT16K33 共享) --- */
#define PIN_I2C_SDA             8
#define PIN_I2C_SCL             9
#define I2C_FREQ                400000UL

/* --- HT16K33 16×16 LED 点阵 (I2C, 双芯片) --- */
#define LED_MATRIX_ADDR_UPPER   0x70
#define LED_MATRIX_ADDR_LOWER   0x71

/* --- GT30L32S4W 字库芯片 (SPI) --- */
#define PIN_FONT_SCK            4
#define PIN_FONT_MISO           5
#define PIN_FONT_MOSI           6
#define PIN_FONT_CS             7

/* --- SYN6658 语音合成芯片 (UART) --- */
#define PIN_SYN_TX              2
#define PIN_SYN_RX              3
#define SYN_BAUD_RATE           115200

/* ============================================================
 * 报站状态机
 * ============================================================ */
enum AnnounceState {
    ANN_IDLE,           /* 空闲, 等待按键 */
    ANN_ARRIVE_PLAY,    /* 到站: 语音播放 + LED 滚动中 */
    ANN_ARRIVE_WAIT,    /* 到站: 等待语音播完 */
    ANN_DEPART_PLAY,    /* 离站: 语音播放 + LED 滚动中 */
    ANN_DEPART_WAIT     /* 离站: 等待语音播完 */
};

static int8_t        s_current_station = 0;
static bool          s_arriving = true;
static AnnounceState s_ann_state = ANN_IDLE;

/* ============================================================
 * 启动报站 (非阻塞)
 * ============================================================ */
static void start_announce(void)
{
    const station_info_t *st = &station_table[s_current_station];

    Serial.printf("\n=== Station %d/%d ===\n", s_current_station + 1, STATION_COUNT);

    if (s_arriving) {
        /* --- 到站 --- */
        Serial.println("[Announce] Arriving...");

        /* 语音播报 */
        syn6658_speak(st->tts_arrive);

        /* LED 滚动显示当前站名 */
        led_matrix_start_scroll(st->codes, st->code_len);

        s_ann_state = ANN_ARRIVE_PLAY;

    } else {
        /* --- 离站 --- */
        Serial.println("[Announce] Departing...");

        /* 语音播报 */
        syn6658_speak(st->tts_depart);

        /* LED 滚动显示下一站名 */
        if (s_current_station < STATION_COUNT - 1) {
            const station_info_t *next = &station_table[s_current_station + 1];
            led_matrix_start_scroll(next->codes, next->code_len);
        } else {
            led_matrix_start_scroll(st->codes, st->code_len);
        }

        s_ann_state = ANN_DEPART_PLAY;
    }
}

/* ============================================================
 * 报站状态机更新 (在 loop 中调用)
 * ============================================================ */
static void update_announce(void)
{
    switch (s_ann_state) {
        case ANN_IDLE:
            break;

        case ANN_ARRIVE_PLAY:
            /* 等待 LED 滚动完成 */
            if (!led_matrix_is_scrolling()) {
                s_ann_state = ANN_ARRIVE_WAIT;
            }
            break;

        case ANN_ARRIVE_WAIT:
            /* 等待语音播完 */
            if (!syn6658_is_busy()) {
                Serial.println("[Announce] Arrive done");
                s_arriving = false;
                s_ann_state = ANN_IDLE;
            }
            break;

        case ANN_DEPART_PLAY:
            if (!led_matrix_is_scrolling()) {
                s_ann_state = ANN_DEPART_WAIT;
            }
            break;

        case ANN_DEPART_WAIT:
            if (!syn6658_is_busy()) {
                Serial.println("[Announce] Depart done");
                if (s_current_station < STATION_COUNT - 1) {
                    s_current_station++;
                    s_arriving = true;
                }
                s_ann_state = ANN_IDLE;
            }
            break;
    }
}

/* ============================================================
 * setup
 * ============================================================ */
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=============================");
    Serial.println("  Bus Announcer - ESP32-C3");
    Serial.println("=============================\n");

    /* 1. 初始化 I2C 总线 */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
    delay(500);

    /* 2. 初始化 TTP229 触摸按键 */
    ttp229_init();
    Serial.println("[TTP229] Init OK");

    /* 3. 初始化 LED 点阵 */
    led_matrix_init(LED_MATRIX_ADDR_UPPER, LED_MATRIX_ADDR_LOWER);

    /* 4. 初始化字库芯片 */
    gt30l_init(PIN_FONT_SCK, PIN_FONT_MISO, PIN_FONT_MOSI, PIN_FONT_CS);

    /* 字库 + LED 测试: 滚动显示 "火车站" */
    Serial.println("[TEST] Scroll test...");
    led_matrix_start_scroll(CODE_HUOCHE, 3);
    while (led_matrix_is_scrolling()) {
        led_matrix_update_scroll();
        delay(1);
    }
    Serial.println("[TEST] Scroll test done");

    /* 5. 初始化语音芯片 */
    syn6658_init(PIN_SYN_TX, PIN_SYN_RX, SYN_BAUD_RATE);

    delay(500);
    Serial.println("\n[System] Ready!\n");
    Serial.println("  Key 0: Previous station");
    Serial.println("  Key 1: Next (announce)");
    Serial.println("  Key 2: Replay current");
}

/* ============================================================
 * loop
 * ============================================================ */
void loop()
{
    /* 1. 更新 LED 滚动 (非阻塞) */
    led_matrix_update_scroll();

    /* 2. 更新报站状态机 */
    update_announce();

    /* 3. 检测按键 (仅在空闲时响应) */
    int8_t key = ttp229_poll();

    if (key >= 0 && s_ann_state == ANN_IDLE) {
        Serial.printf("[Key] Pressed: %d\n", key);

        switch (key) {
            case 0:  /* 上一站 */
                if (s_current_station > 0) {
                    s_current_station--;
                    s_arriving = true;
                    start_announce();
                } else {
                    Serial.println("[Info] Already at first station");
                }
                break;

            case 1:  /* 下一站 / 继续报站 */
                start_announce();
                break;

            case 2:  /* 重播当前站 */
                s_arriving = true;
                start_announce();
                break;

            default:
                break;
        }
    }

    delay(1);
}
