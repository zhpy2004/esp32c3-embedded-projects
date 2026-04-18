/**
 * @file    cs100_oled_buzzer.ino
 * @brief   CS100 测距 + LEDC 蜂鸣器报警 + SSD1315 OLED 显示 (全裸驱动)
 * @board   ESP32-C3
 * @wiring
 *   CS100:  TRIG->GPIO2, ECHO->GPIO3, VCC->5V, GND->GND
 *   Buzzer: GPIO7 (LEDC PWM, 串联 100Ω)
 *   OLED:   SDA->GPIO6, SCL->GPIO10 (I2C, 外接 4.7kΩ 上拉)
 */

#include <Arduino.h>
#include <Wire.h>

/* ============================================================
 * 1. 引脚 & 参数配置
 * ============================================================ */
// CS100
#define CS100_PIN_TRIG       2
#define CS100_PIN_ECHO       3
#define CS100_MEASURE_MS     100
#define CS100_TIMEOUT_US     58000UL
#define CS100_RANGE_MIN_CM   2.0f
#define CS100_RANGE_MAX_CM   560.0f
#define CS100_CM_PER_US      0.0170f
#define CS100_MEDIAN_N       5
#define CS100_EMA_ALPHA      0.3f

// 蜂鸣器
#define BUZZER_PIN           0
#define BUZZER_RESOLUTION    8
#define ALERT_DIST_FAR       100.0f
#define ALERT_DIST_MID       50.0f
#define ALERT_DIST_NEAR      20.0f
#define BUZZER_FREQ_LOW      1000
#define BUZZER_FREQ_MID      2500
#define BUZZER_FREQ_HIGH     4000
#define BEEP_ON_FAR          100
#define BEEP_OFF_FAR         400
#define BEEP_ON_MID          150
#define BEEP_OFF_MID         200
#define BEEP_ON_NEAR         0
#define BEEP_OFF_NEAR        0

// OLED (SSD1315 / I2C)
#define OLED_SDA             8
#define OLED_SCL             9
#define OLED_I2C_FREQ        400000UL
#define OLED_ADDR            0x3C
#define OLED_WIDTH           128
#define OLED_HEIGHT          64
#define OLED_PAGES           (OLED_HEIGHT / 8)

// 显示刷新周期
#define DISPLAY_UPDATE_MS    150

/* ============================================================
 * 2. CS100 中断驱动
 * ============================================================ */
typedef enum {
    ST_IDLE = 0,
    ST_WAIT_RISE,
    ST_WAIT_FALL,
    ST_DONE,
    ST_TIMEOUT
} cs100_state_t;

static volatile cs100_state_t g_state      = ST_IDLE;
static volatile uint32_t      g_echo_start = 0;
static volatile uint32_t      g_echo_end   = 0;
static volatile bool          g_data_ready = false;

static void IRAM_ATTR cs100_echo_isr(void)
{
    uint32_t now = micros();

    if (digitalRead(CS100_PIN_ECHO) == HIGH) {
        if (g_state == ST_WAIT_RISE) {
            g_echo_start = now;
            g_state      = ST_WAIT_FALL;
        }
    } else {
        if (g_state == ST_WAIT_FALL) {
            g_echo_end   = now;
            g_state      = ST_DONE;
            g_data_ready = true;
        }
    }
}

/* ============================================================
 * 3. 滤波层 —— 中值(5) + EMA(0.3)
 * ============================================================ */
static float   s_med_buf[CS100_MEDIAN_N];
static uint8_t s_med_idx   = 0;
static uint8_t s_med_count = 0;
static float   s_ema_val   = 0.0f;
static bool    s_ema_init  = false;

static float median_of(const float *src, uint8_t len)
{
    float tmp[CS100_MEDIAN_N];
    memcpy(tmp, src, len * sizeof(float));

    for (uint8_t i = 1; i < len; i++) {
        float key = tmp[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[len / 2];
}

static bool filter_process(float raw, float *out)
{
    s_med_buf[s_med_idx] = raw;
    s_med_idx = (s_med_idx + 1) % CS100_MEDIAN_N;

    if (s_med_count < CS100_MEDIAN_N) {
        s_med_count++;
        if (s_med_count < CS100_MEDIAN_N) {
            return false;
        }
    }

    float med = median_of(s_med_buf, CS100_MEDIAN_N);

    if (!s_ema_init) {
        s_ema_val  = med;
        s_ema_init = true;
    } else {
        s_ema_val = CS100_EMA_ALPHA * med
                  + (1.0f - CS100_EMA_ALPHA) * s_ema_val;
    }

    *out = s_ema_val;
    return true;
}

/* ============================================================
 * 4. 核心函数: cs100_read_cm()
 * ============================================================
 *  返回值:
 *    > 0   有效距离 (cm)
 *    = 0   无新数据
 *   -1.0   超时无回波
 *   -2.0   超出量程
 *   -3.0   滤波器预热中
 */
static uint32_t s_last_trig_ms = 0;
static uint32_t s_trig_time_ms = 0;

float cs100_read_cm(void)
{
    uint32_t now = millis();

    /* ---- IDLE / TIMEOUT → 定时触发 ---- */
    if (g_state == ST_IDLE || g_state == ST_TIMEOUT) {
        if ((now - s_last_trig_ms) < CS100_MEASURE_MS) {
            return 0.0f;
        }

        g_data_ready = false;
        g_state      = ST_WAIT_RISE;

        digitalWrite(CS100_PIN_TRIG, LOW);
        delayMicroseconds(2);
        digitalWrite(CS100_PIN_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(CS100_PIN_TRIG, LOW);

        s_trig_time_ms = now;
        s_last_trig_ms = now;
        return 0.0f;
    }

    /* ---- 等待中 → 超时检测 ---- */
    if (g_state == ST_WAIT_RISE || g_state == ST_WAIT_FALL) {
        if ((now - s_trig_time_ms) > (CS100_TIMEOUT_US / 1000 + 10)) {
            noInterrupts();
            g_state = ST_TIMEOUT;
            interrupts();
            return -1.0f;
        }
        return 0.0f;
    }

    /* ---- DONE → 处理数据 ---- */
    if (g_state == ST_DONE && g_data_ready) {
        noInterrupts();
        uint32_t t0 = g_echo_start;
        uint32_t t1 = g_echo_end;
        g_data_ready = false;
        g_state      = ST_IDLE;
        interrupts();

        uint32_t pulse_us = t1 - t0;

        if (pulse_us == 0 || pulse_us > CS100_TIMEOUT_US) {
            return -1.0f;
        }

        float raw_cm = (float)pulse_us * CS100_CM_PER_US;

        if (raw_cm < CS100_RANGE_MIN_CM || raw_cm > CS100_RANGE_MAX_CM) {
            return -2.0f;
        }

        float filtered = 0.0f;
        if (!filter_process(raw_cm, &filtered)) {
            return -3.0f;
        }

        return filtered;
    }

    return 0.0f;
}

/* ============================================================
 * 5. 蜂鸣器控制 (Attach/Detach 生命周期模式)
 * ============================================================
 *
 * ESP32-C3 Arduino Core 3.x LEDC 可靠驱动方式:
 *   发声: ledcAttach → ledcWriteTone(freq)
 *   停声: ledcDetach
 *   不能用 ledcWriteTone(0) 停声后再恢复
 */
static bool s_buzzer_attached = false;

static void buzzer_on(uint32_t freq_hz)
{
    // 每次发声: 先 Attach 再 Tone (参考能响的代码)
    if (!s_buzzer_attached) {
        ledcAttach(BUZZER_PIN, freq_hz, BUZZER_RESOLUTION);
        s_buzzer_attached = true;
    }
    ledcWriteTone(BUZZER_PIN, freq_hz);
}

static void buzzer_off(void)
{
    if (s_buzzer_attached) {
        ledcDetach(BUZZER_PIN);
        s_buzzer_attached = false;
    }
}

/**
 * @brief 蜂鸣器报警
 * @param dist_cm  距离值
 *                  >0 : 有效距离，根据阈值判断是否报警
 *                  =0 : 无新数据，保持当前状态不变
 *                  <0 : 错误，静音
 */
static void buzzer_alert(float dist_cm)
{
    static uint32_t s_beep_toggle_ms = 0;
    static bool     s_beep_is_on     = false;
    static float    s_last_dist      = 999.0f;  // 缓存上次有效距离

    /* ====== 关键修正: distance==0 时直接返回，保持现状 ====== */
    if (dist_cm == 0.0f) {
        // 无新数据，但间歇鸣叫的定时切换仍需执行
        dist_cm = s_last_dist;
    } else if (dist_cm > 0.0f) {
        // 有新的有效数据，更新缓存
        s_last_dist = dist_cm;
    } else {
        // 负值 = 错误，静音
        if (s_beep_is_on) {
            buzzer_off();
            s_beep_is_on = false;
        }
        return;
    }

    uint32_t now = millis();

    /* 安全距离 → 静音 */
    if (dist_cm > ALERT_DIST_FAR) {
        if (s_beep_is_on) {
            buzzer_off();
            s_beep_is_on = false;
        }
        return;
    }

    /* 确定报警级别 */
    uint32_t freq, on_ms, off_ms;
    if (dist_cm <= ALERT_DIST_NEAR) {
        freq = BUZZER_FREQ_HIGH; on_ms = BEEP_ON_NEAR; off_ms = BEEP_OFF_NEAR;
    } else if (dist_cm <= ALERT_DIST_MID) {
        freq = BUZZER_FREQ_MID;  on_ms = BEEP_ON_MID;  off_ms = BEEP_OFF_MID;
    } else {
        freq = BUZZER_FREQ_LOW;  on_ms = BEEP_ON_FAR;  off_ms = BEEP_OFF_FAR;
    }

    /* 紧急: 持续鸣叫 */
    if (on_ms == 0 && off_ms == 0) {
        if (!s_beep_is_on) {
            buzzer_on(freq);
            s_beep_is_on = true;
        }
        return;
    }

    /* 间歇模式 */
    if (s_beep_is_on) {
        if ((now - s_beep_toggle_ms) >= on_ms) {
            buzzer_off();
            s_beep_is_on     = false;
            s_beep_toggle_ms = now;
        }
    } else {
        if ((now - s_beep_toggle_ms) >= off_ms) {
            buzzer_on(freq);
            s_beep_is_on     = true;
            s_beep_toggle_ms = now;
        }
    }
}

/* ============================================================
 * 6. SSD1315 OLED 裸驱动 (I2C)
 * ============================================================ */

// 帧缓冲区: 128×8 = 1024 Bytes
static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

static void oled_cmd(uint8_t cmd)
{
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void oled_cmd2(uint8_t cmd, uint8_t val)
{
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(cmd);
    Wire.write(val);
    Wire.endTransmission();
}

/** @brief 将帧缓冲区刷新到 OLED */
static void oled_flush(void)
{
    oled_cmd(0x21); oled_cmd(0x00); oled_cmd(0x7F);  // 列范围 0~127
    oled_cmd(0x22); oled_cmd(0x00); oled_cmd(0x07);  // 页范围 0~7

    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x40);
        Wire.write(&oled_buffer[page * OLED_WIDTH], 64);
        Wire.endTransmission();

        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x40);
        Wire.write(&oled_buffer[page * OLED_WIDTH + 64], 64);
        Wire.endTransmission();
    }
}

/**
 * @brief SSD1315 初始化 —— 按数据手册上电时序
 */
static void oled_init(void)
{
    delay(1);

    oled_cmd(0xAE);          // Display Off
    oled_cmd2(0xA8, 0x3F);  // Multiplex Ratio: 1/64
    oled_cmd2(0xD3, 0x00);  // Display Offset: 0
    oled_cmd(0x40);          // Display Start Line: 0
    oled_cmd(0xA1);          // Segment Re-map: 列反转
    oled_cmd(0xC8);          // COM Scan: 行反转
    oled_cmd2(0xDA, 0x12);  // COM Pins Config
    oled_cmd2(0xD5, 0xF0);  // Clock Divide
    oled_cmd2(0x81, 0xB0);  // Contrast
    oled_cmd2(0xD9, 0xF1);  // Pre-charge Period
    oled_cmd2(0xDB, 0x30);  // VCOMH Deselect
    oled_cmd2(0x8D, 0x14);  // Charge Pump Enable
    oled_cmd2(0x20, 0x00);  // Horizontal Addressing Mode
    oled_cmd(0xA4);          // Display follows RAM
    oled_cmd(0xA6);          // Normal Display

    memset(oled_buffer, 0x00, sizeof(oled_buffer));
    oled_flush();

    delay(100);
    oled_cmd(0xAF);          // Display On
}

static void oled_clear(void)
{
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

static void oled_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint16_t idx = (y / 8) * OLED_WIDTH + x;
    if (on) oled_buffer[idx] |=  (1 << (y & 7));
    else    oled_buffer[idx] &= ~(1 << (y & 7));
}

/* ============================================================
 * 7. 6×8 ASCII 字体 (空格 ~ z, 95 字符, 570 Bytes Flash)
 * ============================================================ */
static const uint8_t PROGMEM font_6x8[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14,0x00}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12,0x00}, // '$'
    {0x23,0x13,0x08,0x64,0x62,0x00}, // '%'
    {0x36,0x49,0x55,0x22,0x50,0x00}, // '&'
    {0x00,0x05,0x03,0x00,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08,0x00}, // '*'
    {0x08,0x08,0x3E,0x08,0x08,0x00}, // '+'
    {0x00,0x50,0x30,0x00,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08,0x00}, // '-'
    {0x00,0x60,0x60,0x00,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02,0x00}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E,0x00}, // '0'
    {0x00,0x42,0x7F,0x40,0x00,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46,0x00}, // '2'
    {0x21,0x41,0x45,0x4B,0x31,0x00}, // '3'
    {0x18,0x14,0x12,0x7F,0x10,0x00}, // '4'
    {0x27,0x45,0x45,0x45,0x39,0x00}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30,0x00}, // '6'
    {0x01,0x71,0x09,0x05,0x03,0x00}, // '7'
    {0x36,0x49,0x49,0x49,0x36,0x00}, // '8'
    {0x06,0x49,0x49,0x29,0x1E,0x00}, // '9'
    {0x00,0x36,0x36,0x00,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14,0x00}, // '='
    {0x41,0x22,0x14,0x08,0x00,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06,0x00}, // '?'
    {0x32,0x49,0x79,0x41,0x3E,0x00}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E,0x00}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36,0x00}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22,0x00}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C,0x00}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41,0x00}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01,0x00}, // 'F'
    {0x3E,0x41,0x41,0x51,0x32,0x00}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F,0x00}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01,0x00}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41,0x00}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40,0x00}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F,0x00}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F,0x00}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E,0x00}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06,0x00}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E,0x00}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46,0x00}, // 'R'
    {0x46,0x49,0x49,0x49,0x31,0x00}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01,0x00}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F,0x00}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F,0x00}, // 'V'
    {0x7F,0x20,0x18,0x20,0x7F,0x00}, // 'W'
    {0x63,0x14,0x08,0x14,0x63,0x00}, // 'X'
    {0x03,0x04,0x78,0x04,0x03,0x00}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43,0x00}, // 'Z'
    {0x00,0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20,0x00}, // '\'
    {0x41,0x41,0x7F,0x00,0x00,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04,0x00}, // '^'
    {0x40,0x40,0x40,0x40,0x40,0x00}, // '_'
    {0x00,0x01,0x02,0x04,0x00,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78,0x00}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38,0x00}, // 'b'
    {0x38,0x44,0x44,0x44,0x20,0x00}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F,0x00}, // 'd'
    {0x38,0x54,0x54,0x54,0x18,0x00}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02,0x00}, // 'f'
    {0x08,0x14,0x54,0x54,0x3C,0x00}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78,0x00}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00,0x00}, // 'j'
    {0x00,0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78,0x00}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78,0x00}, // 'n'
    {0x38,0x44,0x44,0x44,0x38,0x00}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08,0x00}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C,0x00}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08,0x00}, // 'r'
    {0x48,0x54,0x54,0x54,0x20,0x00}, // 's'
    {0x04,0x3F,0x44,0x40,0x20,0x00}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C,0x00}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C,0x00}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C,0x00}, // 'w'
    {0x44,0x28,0x10,0x28,0x44,0x00}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C,0x00}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44,0x00}, // 'z'
};

/* ============================================================
 * 8. OLED 绘图函数
 * ============================================================ */

/** @brief 绘制单个 6×8 字符 */
static void oled_draw_char(uint8_t x, uint8_t page, char ch)
{
    if (ch < 32 || ch > 'z') ch = ' ';
    uint8_t idx = ch - 32;
    uint16_t base = page * OLED_WIDTH + x;

    for (uint8_t col = 0; col < 6 && (x + col) < OLED_WIDTH; col++) {
        oled_buffer[base + col] = pgm_read_byte(&font_6x8[idx][col]);
    }
}

/** @brief 绘制字符串 */
static void oled_draw_str(uint8_t x, uint8_t page, const char *str)
{
    while (*str && x < OLED_WIDTH) {
        oled_draw_char(x, page, *str++);
        x += 6;
    }
}

/** @brief 绘制 2 倍高度字符 (12×16, 占 2 页) */
static void oled_draw_char_2x(uint8_t x, uint8_t page, char ch)
{
    if (ch < 32 || ch > 'z') ch = ' ';
    uint8_t idx = ch - 32;

    for (uint8_t col = 0; col < 6; col++) {
        uint8_t src = pgm_read_byte(&font_6x8[idx][col]);
        uint16_t stretched = 0;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (src & (1 << bit)) {
                stretched |= (3 << (bit * 2));
            }
        }
        uint8_t x_pos = x + col * 2;
        if (x_pos + 1 < OLED_WIDTH) {
            oled_buffer[page * OLED_WIDTH + x_pos]         = (uint8_t)(stretched & 0xFF);
            oled_buffer[page * OLED_WIDTH + x_pos + 1]     = (uint8_t)(stretched & 0xFF);
            oled_buffer[(page + 1) * OLED_WIDTH + x_pos]   = (uint8_t)(stretched >> 8);
            oled_buffer[(page + 1) * OLED_WIDTH + x_pos + 1] = (uint8_t)(stretched >> 8);
        }
    }
}

/** @brief 绘制 2 倍高度字符串 */
static void oled_draw_str_2x(uint8_t x, uint8_t page, const char *str)
{
    while (*str && x < OLED_WIDTH) {
        oled_draw_char_2x(x, page, *str++);
        x += 12;
    }
}

/** @brief 绘制水平线 */
static void oled_draw_hline(uint8_t x0, uint8_t x1, uint8_t y)
{
    for (uint8_t x = x0; x <= x1 && x < OLED_WIDTH; x++) {
        oled_pixel(x, y, true);
    }
}

/** @brief 绘制距离条形图 (Page 7) */
static void oled_draw_bar(float dist_cm, float max_cm)
{
    memset(&oled_buffer[7 * OLED_WIDTH], 0x00, OLED_WIDTH);

    if (dist_cm <= 0 || dist_cm > max_cm) return;

    uint8_t bar_len = (uint8_t)((1.0f - dist_cm / max_cm) * (OLED_WIDTH - 4));
    if (bar_len > OLED_WIDTH - 4) bar_len = OLED_WIDTH - 4;

    // 边框
    for (uint8_t x = 0; x < OLED_WIDTH; x++) {
        oled_buffer[7 * OLED_WIDTH + x] |= 0x81;
    }
    oled_buffer[7 * OLED_WIDTH + 0]   |= 0xFF;
    oled_buffer[7 * OLED_WIDTH + 127] |= 0xFF;

    // 填充
    for (uint8_t x = 2; x < 2 + bar_len; x++) {
        oled_buffer[7 * OLED_WIDTH + x] |= 0x7E;
    }
}

/** @brief 更新 OLED 显示 (内部控制刷新频率) */
static void oled_update_display(float dist_cm, const char *status)
{
    static uint32_t s_last_update = 0;
    uint32_t now = millis();
    if ((now - s_last_update) < DISPLAY_UPDATE_MS) return;
    s_last_update = now;

    oled_clear();

    /* Page 0: 标题 */
    oled_draw_str(16, 0, "CS100 Ranger");

    /* Page 1: 分隔线 */
    oled_draw_hline(0, 127, 11);

    /* Page 2~3: 距离大字体 */
    if (dist_cm > 0) {
        char buf[16];
        if (dist_cm < 100.0f) {
            snprintf(buf, sizeof(buf), "%5.1f cm", dist_cm);
        } else {
            snprintf(buf, sizeof(buf), "%5.0f cm", dist_cm);
        }
        oled_draw_str_2x(16, 2, buf);
    } else {
        oled_draw_str_2x(16, 2, "--- cm");
    }

    /* Page 5: 分隔线 */
    oled_draw_hline(0, 127, 43);

    /* Page 6: 状态 */
    oled_draw_str(0, 6, status);

    /* Page 7: 条形图 */
    oled_draw_bar(dist_cm, ALERT_DIST_FAR);

    oled_flush();
}

/* ============================================================
 * 9. setup()
 * ============================================================ */
void setup()
{
    /* 串口 */
    Serial.begin(115200);
    while (!Serial) { ; }
    Serial.println();
    Serial.println("==========================================");
    Serial.println(" CS100 + OLED + Buzzer  v4.1");
    Serial.println(" Board: ESP32-C3");
    Serial.println("==========================================");

    /* CS100 GPIO */
    pinMode(CS100_PIN_TRIG, OUTPUT);
    pinMode(CS100_PIN_ECHO, INPUT);
    digitalWrite(CS100_PIN_TRIG, LOW);
    attachInterrupt(digitalPinToInterrupt(CS100_PIN_ECHO),
                    cs100_echo_isr, CHANGE);

    /* 蜂鸣器: 仅设为输出低电平，由 buzzer_on() 按需 Attach */
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    /* OLED I2C */
    Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);
    oled_init();

    /* 启动画面 */
    oled_clear();
    oled_draw_str(22, 2, "CS100 Ranger");
    oled_draw_str(28, 4, "Starting...");
    oled_flush();

    /* CS100 上电稳定 */
    delay(100);

    Serial.println("[INIT] All peripherals ready.\n");
}

/* ============================================================
 * 10. loop()
 * ============================================================ */
static float       g_last_valid_dist = 0.0f;
static const char *g_status_str      = "Initializing";

void loop()
{
    float distance = cs100_read_cm();

    if (distance > 0.0f) {
        g_last_valid_dist = distance;
        Serial.printf("[CS100] Distance: %.2f cm\n", distance);

        if (distance <= ALERT_DIST_NEAR) {
            g_status_str = "!!! DANGER !!!";
        } else if (distance <= ALERT_DIST_MID) {
            g_status_str = "** WARNING **";
        } else if (distance <= ALERT_DIST_FAR) {
            g_status_str = "* Caution *";
        } else {
            g_status_str = "Safe";
        }

    } else if (distance == -1.0f) {
        g_status_str = "TIMEOUT";
        Serial.println("[CS100] Error: TIMEOUT");
    } else if (distance == -2.0f) {
        g_status_str = "OUT OF RANGE";
        Serial.println("[CS100] Error: OUT OF RANGE");
    } else if (distance == -3.0f) {
        g_status_str = "Warming up...";
        Serial.println("[CS100] Warming up...");
    }

    /* 蜂鸣器报警 */
    buzzer_alert(distance);

    /* OLED 显示 */
    oled_update_display(g_last_valid_dist, g_status_str);
}
