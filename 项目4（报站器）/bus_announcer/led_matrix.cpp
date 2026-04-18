/**
 * @file    led_matrix.cpp
 * @brief   16×16 LED 点阵驱动实现 (双 HT16K33, Adafruit 库)
 * @note    参考已验证可用的 Display_Matrix 代码
 */

extern "C" {
#include "led_matrix.h"
#include "gt30l32s4w.h"
}

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

/* ============================================================
 * 内部状态
 * ============================================================ */
static Adafruit_8x16matrix matrix_left;
static Adafruit_8x16matrix matrix_right;

/* 滚动状态 */
#define SCROLL_MAX_CHARS  10
static uint8_t  s_fonts[SCROLL_MAX_CHARS][32];
static int      s_num_chars = 0;
static int      s_offset_x = 16;
static int      s_total_width = 0;
static bool     s_scrolling = false;
static unsigned long s_last_scroll = 0;

/* ============================================================
 * 内部: 统一坐标绘制像素
 * ============================================================ */
static void draw_pixel_unified(int x, int y, uint16_t color)
{
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;
    if (x < 8) matrix_left.drawPixel(x, y, color);
    else        matrix_right.drawPixel(x - 8, y, color);
}

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" void led_matrix_init(uint8_t addr_upper, uint8_t addr_lower)
{
    matrix_left.begin(addr_upper);
    matrix_right.begin(addr_lower);
    matrix_left.setBrightness(5);
    matrix_right.setBrightness(5);

    led_matrix_clear();
    Serial.printf("[LED] Matrix init OK  Upper=0x%02X Lower=0x%02X\n",
                  addr_upper, addr_lower);
}

extern "C" void led_matrix_brightness(uint8_t brightness)
{
    if (brightness > 15) brightness = 15;
    matrix_left.setBrightness(brightness);
    matrix_right.setBrightness(brightness);
}

extern "C" void led_matrix_clear(void)
{
    matrix_left.clear();
    matrix_right.clear();
    matrix_left.writeDisplay();
    matrix_right.writeDisplay();
}

extern "C" void led_matrix_show_16x16(const uint8_t *data)
{
    matrix_left.clear();
    matrix_right.clear();

    for (int y = 0; y < 16; y++) {
        uint8_t left_byte  = data[y * 2];
        uint8_t right_byte = data[y * 2 + 1];
        for (int c = 0; c < 8; c++) {
            if (left_byte & (0x80 >> c))
                draw_pixel_unified(c, y, LED_ON);
            if (right_byte & (0x80 >> c))
                draw_pixel_unified(8 + c, y, LED_ON);
        }
    }

    matrix_left.writeDisplay();
    matrix_right.writeDisplay();
}

extern "C" void led_matrix_start_scroll(const uint16_t *gb_codes, int num)
{
    s_num_chars = num;
    if (s_num_chars > SCROLL_MAX_CHARS) s_num_chars = SCROLL_MAX_CHARS;
    s_total_width = s_num_chars * 16;
    s_offset_x = 16;

    for (int i = 0; i < s_num_chars; i++) {
        uint8_t msb = (gb_codes[i] >> 8) & 0xFF;
        uint8_t lsb = gb_codes[i] & 0xFF;
        gt30l_read_hanzi_16x16(msb, lsb, s_fonts[i]);
    }

    s_scrolling = true;
    s_last_scroll = millis();
}

extern "C" void led_matrix_update_scroll(void)
{
    if (!s_scrolling) return;

    if (millis() - s_last_scroll >= 40) {
        s_last_scroll = millis();
        matrix_left.clear();
        matrix_right.clear();

        for (int i = 0; i < s_num_chars; i++) {
            int char_start_x = s_offset_x + i * 16;
            if (char_start_x <= -16 || char_start_x >= 16) continue;

            for (int y = 0; y < 16; y++) {
                uint8_t left_byte  = s_fonts[i][y * 2];
                uint8_t right_byte = s_fonts[i][y * 2 + 1];
                for (int c = 0; c < 8; c++) {
                    if (left_byte & (0x80 >> c))
                        draw_pixel_unified(char_start_x + c, y, LED_ON);
                    if (right_byte & (0x80 >> c))
                        draw_pixel_unified(char_start_x + 8 + c, y, LED_ON);
                }
            }
        }

        matrix_left.writeDisplay();
        matrix_right.writeDisplay();

        s_offset_x--;
        if (s_offset_x < -s_total_width) {
            s_scrolling = false;
            led_matrix_clear();
        }
    }
}

extern "C" bool led_matrix_is_scrolling(void)
{
    return s_scrolling;
}
