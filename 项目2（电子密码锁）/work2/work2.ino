/*******************************************************************************
 * @file    electronic_password_lock.ino
 * @brief   ESP32 电子密码锁 — TTP229-LSF 触摸键盘 + SSD1315 OLED
 *          无源蜂鸣器 LEDC 驱动 (ledcWriteTone)
 * @note    不使用任何第三方库，全部底层手写驱动
 *          TTP229-LSF: I2C Slave 地址 0x57 (7-bit), 仅读操作
 *          SSD1315:    I2C Slave 地址 0x3C
 *
 *          键盘布局
 *          TP0='0'  TP1='1'  TP2='2'  TP3='3'
 *          TP4='4'  TP5='5'  TP6='6'  TP7='7'
 *          TP8='8'  TP9='9'  TP10='A' TP11='B'
 *          TP12='C' TP13='D' TP14='E' TP15='F'
 *
 * @author  Embedded Master
 * @date    2026-03-29
 ******************************************************************************/

#include <Wire.h>

/* ========================== 硬件引脚定义 ========================== */
#define PIN_SDA         8
#define PIN_SCL         9
//#define PIN_TTP229_SDO  5       /* DV 脉冲中断引脚 */
#define PIN_LED_GREEN   5      /* 开锁 LED */
#define PIN_LED_RED     7      /* 上锁/错误 LED */
#define PIN_LED_BLUE    4      /* 闭锁 LED */
#define PIN_BUZZER      0      /* 无源蜂鸣器 */

/* ========================== 蜂鸣器 LEDC 参数 ========================== */
#define BUZZER_FREQ_DEFAULT  500
#define BUZZER_RESOLUTION    8

/* 音调频率 (Hz) */
#define TONE_C4     523
#define TONE_E4     659
#define TONE_G4     784
#define TONE_C5     1047
#define TONE_E5     1319
#define TONE_G5     1568

#define TONE_KEY_CLICK   4000
#define TONE_CONFIRM     2000
#define TONE_ERROR       800
#define TONE_ALARM_HIGH  3500
#define TONE_ALARM_LOW   500

/* ========================== I2C 地址 ========================== */
#define TTP229_I2C_ADDR   0x57
#define SSD1315_I2C_ADDR  0x3C

/* ========================== 系统参数 ========================== */
#define PASSWORD_MIN_LEN    4
#define PASSWORD_MAX_LEN    10
#define ADMIN_PWD_MIN_LEN   6
#define MAX_WRONG_ATTEMPTS  3
#define LOCKOUT_TIME_MS     30000

/* ========================== 管理员密码 ========================== */
static const char ADMIN_PASSWORD[] = "Admin1";//输入 ADMIN:Admin1 换行 即可解锁

/* ========================== 系统状态 ========================== */
typedef enum {
    STATE_IDLE,
    STATE_SET_PASSWORD,
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_LOCKOUT
} system_state_t;

/* ========================== 全局变量 ========================== */
static system_state_t g_state = STATE_IDLE;
static char g_user_password[PASSWORD_MAX_LEN + 1] = {0};
static uint8_t g_pwd_len = 0;
static char g_input_buf[PASSWORD_MAX_LEN + 1] = {0};
static uint8_t g_input_len = 0;
static uint8_t g_wrong_count = 0;
static unsigned long g_lockout_start = 0;
static volatile bool g_key_changed = false;
static uint16_t g_last_keys = 0;

/* ========================== 5x7 ASCII 字体 ========================== */
static const uint8_t FONT_5X7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 'F'
    {0x3E,0x41,0x41,0x51,0x32}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x7F,0x20,0x18,0x20,0x7F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x03,0x04,0x78,0x04,0x03}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x00,0x7F,0x41,0x41}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x41,0x41,0x7F,0x00,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x08,0x54,0x54,0x54,0x3C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x00,0x7F,0x10,0x28,0x44}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x04,0x08,0x10,0x08}, // '~'
};

/* ==================================================================
 *                    SSD1315 OLED 驱动 (I2C)
 * ================================================================== */

static void oled_write_cmd(uint8_t cmd)
{
    Wire.beginTransmission(SSD1315_I2C_ADDR);
    Wire.write(0x00);
    Wire.write(cmd);
    Wire.endTransmission();
}

static void oled_write_data(uint8_t data)
{
    Wire.beginTransmission(SSD1315_I2C_ADDR);
    Wire.write(0x40);
    Wire.write(data);
    Wire.endTransmission();
}

static void oled_clear(void);

static void oled_init(void)
{
    delay(100);
    oled_write_cmd(0xAE);
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);
    oled_write_cmd(0xD3); oled_write_cmd(0x00);
    oled_write_cmd(0x40);
    oled_write_cmd(0xA1);
    oled_write_cmd(0xC8);
    oled_write_cmd(0xDA); oled_write_cmd(0x12);
    oled_write_cmd(0xD5); oled_write_cmd(0xF0);
    oled_write_cmd(0x81); oled_write_cmd(0xB0);
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);
    oled_write_cmd(0xDB); oled_write_cmd(0x30);
    oled_write_cmd(0x8D); oled_write_cmd(0x14);
    oled_write_cmd(0xA4);
    oled_write_cmd(0xA6);
    oled_write_cmd(0x20); oled_write_cmd(0x00);
    oled_clear();
    oled_write_cmd(0xAF);
}

static void oled_clear(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);
        oled_write_cmd(0x00);
        oled_write_cmd(0x10);
        for (uint8_t col = 0; col < 128; col++) {
            oled_write_data(0x00);
        }
    }
}

static void oled_set_cursor(uint8_t page, uint8_t col)
{
    oled_write_cmd(0xB0 + page);
    oled_write_cmd(0x00 + (col & 0x0F));
    oled_write_cmd(0x10 + ((col >> 4) & 0x0F));
}

static void oled_putchar(uint8_t page, uint8_t col, char ch)
{
    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    oled_set_cursor(page, col);
    uint8_t idx = ch - 0x20;
    for (uint8_t i = 0; i < 5; i++) {
        oled_write_data(FONT_5X7[idx][i]);
    }
    oled_write_data(0x00);
}

static void oled_puts(uint8_t page, uint8_t col, const char *str)
{
    while (*str && col < 122) {
        oled_putchar(page, col, *str++);
        col += 6;
    }
}

static void oled_clear_page(uint8_t page)
{
    oled_set_cursor(page, 0);
    for (uint8_t i = 0; i < 128; i++) {
        oled_write_data(0x00);
    }
}

/* ==================================================================
 *              无源蜂鸣器 LEDC 驱动 (ledcWriteTone)
 * ==================================================================
 * 驱动流程: ledcAttach → ledcWriteTone → delay → ledcDetach
 */

static void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    ledcAttach(PIN_BUZZER, BUZZER_FREQ_DEFAULT, BUZZER_RESOLUTION);
    ledcWriteTone(PIN_BUZZER, freq_hz);
    delay(duration_ms);
    ledcDetach(PIN_BUZZER);
}

static void buzzer_beep(uint16_t duration_ms)
{
    buzzer_tone(TONE_KEY_CLICK, duration_ms);
}

static void buzzer_confirm(uint16_t duration_ms)
{
    buzzer_tone(TONE_CONFIRM, duration_ms);
}

static void buzzer_success(void)
{
    buzzer_tone(TONE_C5, 120);
    delay(30);
    buzzer_tone(TONE_E5, 120);
    delay(30);
    buzzer_tone(TONE_G5, 200);
}

static void buzzer_error(void)
{
    for (uint8_t i = 0; i < 3; i++) {
        buzzer_tone(TONE_ERROR, 100);
        delay(80);
    }
}

static void buzzer_alarm(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        buzzer_tone(TONE_ALARM_HIGH, 100);
        delay(20);
        buzzer_tone(TONE_ALARM_LOW, 100);
        delay(20);
    }
}

static void buzzer_lock(void)
{
    buzzer_tone(TONE_G5, 100);
    delay(30);
    buzzer_tone(TONE_C5, 200);
}

static void buzzer_startup(void)
{
    buzzer_tone(TONE_C4, 80);
    delay(20);
    buzzer_tone(TONE_E4, 80);
    delay(20);
    buzzer_tone(TONE_G4, 120);
}

/* ==================================================================
 *              TTP229-LSF 触摸键盘驱动 (I2C)
 * ================================================================== */

static uint16_t ttp229_read_keys(void)
{
    uint8_t buf[2] = {0, 0};

    Wire.requestFrom((uint8_t)TTP229_I2C_ADDR, (uint8_t)2);
    if (Wire.available() >= 2) {
        buf[0] = Wire.read();
        buf[1] = Wire.read();
    }

    /* 重新映射: MSB-first → bit0=TP0 */
    uint16_t keys = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (buf[0] & (0x80 >> i)) keys |= (1 << i);
        if (buf[1] & (0x80 >> i)) keys |= (1 << (i + 8));
    }
    return keys;
}

static int8_t ttp229_get_key_number(uint16_t keys)
{
    for (uint8_t i = 0; i < 16; i++) {
        if (keys & (1 << i)) return (int8_t)i;
    }
    return -1;
}

/**
 * @brief 将键号映射为字符 (与 PCB 丝印一致)
 *
 *        PCB 布局:
 *        TP0 ='0'   TP1 ='1'   TP2 ='2'   TP3 ='3'
 *        TP4 ='4'   TP5 ='5'   TP6 ='6'   TP7 ='7'
 *        TP8 ='8'   TP9 ='9'   TP10='A'   TP11='B'
 *        TP12='C'   TP13='D'   TP14='E'   TP15='F'
 *
 *        功能分配:
 *        0~9  → 数字输入
 *        A, B → 未使用 (保留)
 *        C    → 确认 (Confirm)
 *        D    → 删除 (Delete)
 *        E    → 关门/上锁 (Lock)
 *        F    → 未使用 (保留)
 */
static char key_to_char(int8_t key)
{
    static const char KEY_MAP[16] = {
     /* TP0   TP1   TP2   TP3  */
        '0', '1', '2', '3',
     /* TP4   TP5   TP6   TP7  */
        '4', '5', '6', '7',
     /* TP8   TP9   TP10  TP11 */
        '8', '9', 'A',  'B',
     /* TP12  TP13  TP14  TP15 */
        'C',  'D',  'E',  'F'
    };
    if (key >= 0 && key < 16) return KEY_MAP[key];
    return '\0';
}

/**
 * @brief 判断字符是否为数字键 (0~9)
 */
static bool is_digit_key(char ch)
{
    return (ch >= '0' && ch <= '9');
}

/**
 * @brief 判断字符是否为功能键
 */
static bool is_confirm_key(char ch)  { return (ch == 'C'); }
static bool is_delete_key(char ch)   { return (ch == 'D'); }
static bool is_lock_key(char ch)     { return (ch == 'E'); }

/* ========================== SDO 中断 ========================== */

static void IRAM_ATTR ttp229_isr(void)
{
    g_key_changed = true;
}

/* ========================== LED 控制 ========================== */

static void led_set(bool green, bool red, bool blue)
{
    digitalWrite(PIN_LED_GREEN, green ? HIGH : LOW);
    digitalWrite(PIN_LED_RED,   red   ? HIGH : LOW);
    digitalWrite(PIN_LED_BLUE,  blue  ? HIGH : LOW);
}

/* ========================== OLED 界面 ========================== */

static void ui_show_idle(void)
{
    oled_clear();
    oled_puts(0, 0, "=== Password Lock ===");
    oled_puts(2, 0, "Door OPEN");
    oled_puts(3, 0, "Press E to CLOSE");
    oled_puts(4, 0, "& set password");
    oled_puts(6, 0, "C=OK D=DEL E=LOCK");
    led_set(true, false, false);
}

static void ui_show_set_password(void)
{
    oled_clear();
    oled_puts(0, 0, "== SET PASSWORD ==");
    oled_puts(2, 0, "Min 4 digits (0-9):");
    oled_puts(4, 0, "Input: ");
    oled_puts(6, 0, "C=Confirm D=Delete");
    led_set(false, false, true);
}

static void ui_show_locked(void)
{
    oled_clear();
    oled_puts(0, 0, "===== LOCKED =====");
    oled_puts(2, 0, "Enter password:");
    oled_puts(4, 0, "Input: ");
    char attempts[22];
    snprintf(attempts, sizeof(attempts), "Attempts: %d/%d",
             g_wrong_count, MAX_WRONG_ATTEMPTS);
    oled_puts(6, 0, attempts);
    led_set(false, true, false);
}

static void ui_show_unlocked(void)
{
    oled_clear();
    oled_puts(0, 0, "==== UNLOCKED ====");
    oled_puts(2, 0, "Door is OPEN!");
    oled_puts(4, 0, "Password cleared.");
    oled_puts(6, 0, "Press E to re-lock");
    led_set(true, false, false);
}

static void ui_show_lockout(void)
{
    oled_clear();
    oled_puts(0, 0, "!!! LOCKOUT !!!");
    oled_puts(2, 0, "Too many attempts!");
    oled_puts(4, 0, "Wait 30 seconds...");
    led_set(false, false, true);
}

static void ui_update_input(uint8_t page)
{
    oled_clear_page(page);
    oled_puts(page, 0, "Input: ");
    for (uint8_t i = 0; i < g_input_len; i++) {
        oled_putchar(page, 42 + i * 6, '*');
    }
}

/* ========================== 管理员处理 ========================== */

static void process_serial_admin(void)
{
    if (Serial.available() == 0) return;

    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("ADMIN:")) {
        String pwd = line.substring(6);
        Serial.print("[Admin] Password attempt: ");
        Serial.println(pwd);

        if (pwd.equals(ADMIN_PASSWORD)) {
            Serial.println("[Admin] *** ACCESS GRANTED ***");
            Serial.println("[Admin] User password cleared, door unlocked.");

            memset(g_user_password, 0, sizeof(g_user_password));
            g_pwd_len = 0;
            g_wrong_count = 0;
            g_input_len = 0;
            memset(g_input_buf, 0, sizeof(g_input_buf));

            g_state = STATE_UNLOCKED;
            ui_show_unlocked();
            buzzer_success();
            led_set(true, false, false);
        } else {
            Serial.println("[Admin] *** ACCESS DENIED ***");
            buzzer_error();
        }
    } else {
        Serial.println("[System] Unknown cmd. Use: ADMIN:<password>");
    }
}

/* ========================== 主状态机 ========================== */

static void handle_key_press(char key)
{
    switch (g_state) {

    /* ---------- 空闲: 门开，按 E 关门 ---------- */
    case STATE_IDLE:
        if (is_lock_key(key)) {
            g_state = STATE_SET_PASSWORD;
            g_input_len = 0;
            memset(g_input_buf, 0, sizeof(g_input_buf));
            ui_show_set_password();
            buzzer_lock();
            Serial.println("[State] Door closed -> Set password");
        }
        break;

    /* ---------- 设定密码: 数字输入, C确认, D删除 ---------- */
    case STATE_SET_PASSWORD:
        if (is_digit_key(key)) {
            if (g_input_len < PASSWORD_MAX_LEN) {
                g_input_buf[g_input_len++] = key;
                g_input_buf[g_input_len] = '\0';
                ui_update_input(4);
                buzzer_beep(30);
            }
        } else if (is_delete_key(key)) {
            if (g_input_len > 0) {
                g_input_buf[--g_input_len] = '\0';
                ui_update_input(4);
                buzzer_beep(30);
            }
        } else if (is_confirm_key(key)) {
            if (g_input_len >= PASSWORD_MIN_LEN) {
                memcpy(g_user_password, g_input_buf, g_input_len + 1);
                g_pwd_len = g_input_len;
                g_wrong_count = 0;
                g_input_len = 0;
                memset(g_input_buf, 0, sizeof(g_input_buf));

                g_state = STATE_LOCKED;
                ui_show_locked();
                buzzer_confirm(200);
                Serial.print("[State] Password set (");
                Serial.print(g_pwd_len);
                Serial.println(" digits), LOCKED!");
            } else {
                oled_clear_page(5);
                oled_puts(5, 0, "Min 4 digits!");
                buzzer_error();
            }
        }
        break;

    /* ---------- 上锁: 输入密码, C验证, D删除 ---------- */
    case STATE_LOCKED:
        if (is_digit_key(key)) {
            if (g_input_len < PASSWORD_MAX_LEN) {
                g_input_buf[g_input_len++] = key;
                g_input_buf[g_input_len] = '\0';
                ui_update_input(4);
                buzzer_beep(30);
            }
        } else if (is_delete_key(key)) {
            if (g_input_len > 0) {
                g_input_buf[--g_input_len] = '\0';
                ui_update_input(4);
                buzzer_beep(30);
            }
        } else if (is_confirm_key(key)) {
            if (g_input_len == g_pwd_len &&
                memcmp(g_input_buf, g_user_password, g_pwd_len) == 0) {
                /* 密码正确 → 开锁, 密码即时失效 */
                Serial.println("[State] CORRECT -> UNLOCKED!");
                memset(g_user_password, 0, sizeof(g_user_password));
                g_pwd_len = 0;
                g_wrong_count = 0;
                g_input_len = 0;
                memset(g_input_buf, 0, sizeof(g_input_buf));

                g_state = STATE_UNLOCKED;
                ui_show_unlocked();
                buzzer_success();
            } else {
                /* 密码错误 */
                g_wrong_count++;
                Serial.print("[State] WRONG! Attempt ");
                Serial.print(g_wrong_count);
                Serial.print("/");
                Serial.println(MAX_WRONG_ATTEMPTS);

                g_input_len = 0;
                memset(g_input_buf, 0, sizeof(g_input_buf));

                if (g_wrong_count >= MAX_WRONG_ATTEMPTS) {
                    g_state = STATE_LOCKOUT;
                    g_lockout_start = millis();
                    ui_show_lockout();
                    buzzer_alarm();
                    Serial.println("[State] !!! LOCKOUT !!!");
                } else {
                    ui_show_locked();
                    oled_clear_page(5);
                    oled_puts(5, 0, "Wrong! Try again.");
                    buzzer_error();
                }
            }
        }
        break;

    /* ---------- 开锁: 密码已失效, 按 E 关门 ---------- */
    case STATE_UNLOCKED:
        if (is_lock_key(key)) {
            g_state = STATE_SET_PASSWORD;
            g_input_len = 0;
            memset(g_input_buf, 0, sizeof(g_input_buf));
            ui_show_set_password();
            buzzer_lock();
            Serial.println("[State] Door closed -> Set new password");
        }
        break;

    /* ---------- 闭锁: 忽略所有按键 ---------- */
    case STATE_LOCKOUT:
        break;
    }
}

/* ========================== Arduino 入口 ========================== */

void setup(void)
{
    Serial.begin(115200);
    Serial.println("\n========================================");
    Serial.println("  ESP32 Electronic Password Lock v2.1");
    Serial.println("  Keypad: 0-9=digit C=OK D=DEL E=LOCK");
    Serial.println("  Admin cmd: ADMIN:<password>");
    Serial.println("========================================\n");

    /* LED GPIO */
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_LED_BLUE,  OUTPUT);
    //pinMode(PIN_TTP229_SDO, INPUT_PULLUP);

    led_set(false, false, false);

    /* I2C */
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    /* TTP229 上电等待 */
    delay(600);

    /* OLED */
    oled_init();

    /* TTP229 SDO 中断 */
    //attachInterrupt(digitalPinToInterrupt(PIN_TTP229_SDO),
    //                ttp229_isr, FALLING);
    g_last_keys = ttp229_read_keys();

    /* 初始状态 */
    g_state = STATE_IDLE;
    ui_show_idle();

    /* 开机提示音 */
    buzzer_startup();

    Serial.println("[System] Init complete.");
    Serial.println("[System] Keypad layout:");
    Serial.println("  0 1 2 3");
    Serial.println("  4 5 6 7");
    Serial.println("  8 9 A B");
    Serial.println("  C D E F");
    Serial.println("  C=Confirm D=Delete E=Lock/Close\n");
}

void loop(void)
{
    /* 1. 串口管理员 */
    process_serial_admin();

    /* 2. 闭锁超时 */
    if (g_state == STATE_LOCKOUT) {
        unsigned long elapsed = millis() - g_lockout_start;
        if (elapsed >= LOCKOUT_TIME_MS) {
            g_wrong_count = 0;
            g_input_len = 0;
            memset(g_input_buf, 0, sizeof(g_input_buf));
            g_state = STATE_LOCKED;
            ui_show_locked();
            buzzer_confirm(150);
            Serial.println("[State] Lockout expired -> LOCKED");
        } else {
            static unsigned long last_blink = 0;
            if (millis() - last_blink > 500) {
                last_blink = millis();
                digitalWrite(PIN_LED_BLUE, !digitalRead(PIN_LED_BLUE));
            }
            uint8_t remaining = (LOCKOUT_TIME_MS - elapsed) / 1000;
            char countdown[22];
            snprintf(countdown, sizeof(countdown), "Remain: %2ds  ", remaining);
            oled_puts(5, 0, countdown);
        }
        return;
    }

    /* 3. DV 中断按键 */
    if (g_key_changed) {
        g_key_changed = false;
        delay(20);

        uint16_t current_keys = ttp229_read_keys();
        uint16_t new_press = current_keys & (~g_last_keys);
        g_last_keys = current_keys;

        if (new_press != 0) {
            int8_t key_num = ttp229_get_key_number(new_press);
            if (key_num >= 0) {
                char key_char = key_to_char(key_num);
                Serial.print("[Key] TP");
                Serial.print(key_num);
                Serial.print(" -> '");
                Serial.print(key_char);
                Serial.println("'");
                handle_key_press(key_char);
            }
        }
    }

    /* 4. 轮询补充 */
    static unsigned long last_poll = 0;
    if (millis() - last_poll > 100) {
        last_poll = millis();
        uint16_t current_keys = ttp229_read_keys();
        uint16_t new_press = current_keys & (~g_last_keys);
        g_last_keys = current_keys;

        if (new_press != 0) {
            int8_t key_num = ttp229_get_key_number(new_press);
            if (key_num >= 0) {
                char key_char = key_to_char(key_num);
                Serial.print("[Key-Poll] TP");
                Serial.print(key_num);
                Serial.print(" -> '");
                Serial.print(key_char);
                Serial.println("'");
                handle_key_press(key_char);
            }
        }
    }
}
