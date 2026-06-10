/**
 * @file    buzzer.cpp
 * @brief   无源蜂鸣器驱动实现
 */

extern "C" {
#include "buzzer.h"
}

#include <Arduino.h>

/* 内部状态 */
static uint8_t  s_pin        = 0;
static bool     s_attached   = false;
static bool     s_is_on      = false;
static uint32_t s_toggle_ms  = 0;

extern "C" void buzzer_init(uint8_t pin)
{
    s_pin      = pin;
    s_attached = false;
    s_is_on    = false;

    pinMode(s_pin, OUTPUT);
    digitalWrite(s_pin, LOW);
}

extern "C" void buzzer_on(uint32_t freq_hz)
{
    if (!s_attached) {
        ledcAttach(s_pin, freq_hz, BUZZER_PWM_RESOLUTION);
        s_attached = true;
    }
    ledcWriteTone(s_pin, freq_hz);
    s_is_on = true;
}

extern "C" void buzzer_off(void)
{
    if (s_attached) {
        ledcDetach(s_pin);
        s_attached = false;
    }
    s_is_on = false;
}

extern "C" bool buzzer_is_on(void)
{
    return s_is_on;
}

extern "C" void buzzer_beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms)
{
    uint32_t now = millis();

    if (on_ms == 0 && off_ms == 0) {
        if (!s_is_on) {
            buzzer_on(freq_hz);
        }
        return;
    }

    if (s_is_on) {
        if ((now - s_toggle_ms) >= on_ms) {
            buzzer_off();
            s_toggle_ms = now;
        }
    } else {
        if ((now - s_toggle_ms) >= off_ms) {
            buzzer_on(freq_hz);
            s_toggle_ms = now;
        }
    }
}

extern "C" void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    buzzer_on(freq_hz);
    delay(duration_ms);
    buzzer_off();
}
