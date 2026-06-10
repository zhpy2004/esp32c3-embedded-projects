/**
 * @file    epd_ssd1680.cpp
 * @brief   2.13" BWR 三色电子墨水屏驱动 (SSD1680, 软件 SPI)
 *
 * 面板: ZJY122250-0213BBDMFGN-R  (122 x 250, 黑/白/红)
 * 初始化序列参考面板规格书 Page 28 示例代码
 *
 * SSD1680 命令参考:
 *   0x01  Driver Output Control        — 设置扫描行数和方向
 *   0x03  Gate Driving Voltage Control  — 栅极驱动电压
 *   0x04  Source Driving Voltage Control— 源极驱动电压
 *   0x11  Data Entry Mode Setting       — RAM 写入方向
 *   0x12  SW Reset                      — 软件复位
 *   0x21  Display Update Control 1      — RAM 反色/旁路选项
 *   0x22  Display Update Control 2      — 显示更新激活序列
 *   0x24  Write BW RAM                  — 写入黑白层 RAM
 *   0x26  Write RED RAM                 — 写入红色层 RAM
 *   0x2C  Write VCOM Register           — VCOM 电压
 *   0x32  Write LUT Register            — 波形查找表 (224 字节)
 *   0x3C  Border Waveform Control       — 边框波形
 *   0x44  Set RAM X Address Range       — X 方向窗口
 *   0x45  Set RAM Y Address Range       — Y 方向窗口
 *   0x4E  Set RAM X Address Counter     — 当前 X 地址
 *   0x4F  Set RAM Y Address Counter     — 当前 Y 地址
 *   0x10  Deep Sleep Mode               — 进入深度睡眠
 *   0x20  Activate Display Update       — 激活刷新
 */

#include <Arduino.h>
#include "epd_ssd1680.h"

/* ============================================================
 * 引脚变量 (软件 SPI)
 * ============================================================ */
static uint8_t pin_sck;
static uint8_t pin_mosi;
static uint8_t pin_cs;
static uint8_t pin_dc;
static uint8_t pin_rst;
static uint8_t pin_busy;

/* ============================================================
 * 面板规格书 Page 28 完整 LUT (224 字节)
 * 格式: VS[Lx-Ly] 查找表 + TP 时序参数
 * ============================================================ */
static const uint8_t LUT_FULL[] PROGMEM = {
    /* VS LUT (70 bytes: LUT0-LUT4 各14字节) */
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x48, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* TP (timing) 参数 (70 bytes) */
    0x0A, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x00, 0x08, 0x01,
    0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    /* FR (frame rate) 等附加参数 (84 bytes) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* ============================================================
 * 底层 SPI 与引脚操作
 * ============================================================ */

static inline void spi_write_byte(uint8_t data)
{
    for (int8_t i = 7; i >= 0; i--) {
        digitalWrite(pin_sck, LOW);
        digitalWrite(pin_mosi, (data >> i) & 0x01);
        digitalWrite(pin_sck, HIGH);
    }
}

static void epd_send_cmd(uint8_t cmd)
{
    digitalWrite(pin_dc, LOW);
    digitalWrite(pin_cs, LOW);
    spi_write_byte(cmd);
    digitalWrite(pin_cs, HIGH);
}

static void epd_send_data(uint8_t data)
{
    digitalWrite(pin_dc, HIGH);
    digitalWrite(pin_cs, LOW);
    spi_write_byte(data);
    digitalWrite(pin_cs, HIGH);
}

static void epd_wait_busy(void)
{
    while (digitalRead(pin_busy) == HIGH) {
        delay(10);
    }
}

static void epd_hw_reset(void)
{
    digitalWrite(pin_rst, HIGH);
    delay(20);
    digitalWrite(pin_rst, LOW);
    delay(2);
    digitalWrite(pin_rst, HIGH);
    delay(20);
    epd_wait_busy();
}

/* ============================================================
 * RAM 窗口与地址指针设置
 * ============================================================ */

static void epd_set_window(uint8_t x_start, uint8_t x_end,
                           uint16_t y_start, uint16_t y_end)
{
    /* Set RAM X address start/end (0x44) */
    epd_send_cmd(0x44);
    epd_send_data(x_start);
    epd_send_data(x_end);

    /* Set RAM Y address start/end (0x45) */
    epd_send_cmd(0x45);
    epd_send_data(y_start & 0xFF);
    epd_send_data((y_start >> 8) & 0xFF);
    epd_send_data(y_end & 0xFF);
    epd_send_data((y_end >> 8) & 0xFF);
}

static void epd_set_cursor(uint8_t x, uint16_t y)
{
    /* Set RAM X address counter (0x4E) */
    epd_send_cmd(0x4E);
    epd_send_data(x);

    /* Set RAM Y address counter (0x4F) */
    epd_send_cmd(0x4F);
    epd_send_data(y & 0xFF);
    epd_send_data((y >> 8) & 0xFF);
}

/* ============================================================
 * 写入 LUT
 * ============================================================ */

static void epd_load_lut(void)
{
    epd_send_cmd(0x32);
    for (uint16_t i = 0; i < sizeof(LUT_FULL); i++) {
        epd_send_data(pgm_read_byte(&LUT_FULL[i]));
    }
}

/* ============================================================
 * 初始化序列 (面板规格书 Page 28)
 * ============================================================ */

static void epd_init_panel(void)
{
    epd_hw_reset();

    /* SW Reset */
    epd_send_cmd(0x12);
    epd_wait_busy();

    /* Driver Output Control: MUX=249 (0xF9), GD=0, SM=0, TB=0 */
    epd_send_cmd(0x01);
    epd_send_data(0xF9);
    epd_send_data(0x00);
    epd_send_data(0x00);

    /* Data Entry Mode: Y decrement, X increment (0x01) */
    /* 扫描方向: 从上到下, 从左到右 */
    epd_send_cmd(0x11);
    epd_send_data(0x01);

    /* Set RAM X window: 1 ~ 16 (byte address) */
    /* Set RAM Y window: 249 ~ 0 */
    epd_set_window(0x01, EPD_W_BYTES, EPD_HEIGHT - 1, 0);

    /* Border Waveform Control */
    epd_send_cmd(0x3C);
    epd_send_data(0xC0);

    /* VCOM Register */
    epd_send_cmd(0x2C);
    epd_send_data(0x70);

    /* Gate Driving Voltage */
    epd_send_cmd(0x03);
    epd_send_data(0x17);

    /* Source Driving Voltage */
    epd_send_cmd(0x04);
    epd_send_data(0x41);
    epd_send_data(0x00);
    epd_send_data(0x32);

    /* Load LUT */
    epd_load_lut();
}

/* ============================================================
 * 激活显示更新
 * ============================================================ */

static void epd_update_full(void)
{
    /* Display Update Control 2: 使用 LUT 进行全刷新 */
    epd_send_cmd(0x22);
    epd_send_data(0xC7);

    /* Activate Display Update Sequence */
    epd_send_cmd(0x20);
    epd_wait_busy();
}

/* ============================================================
 * 公开 API 实现
 * ============================================================ */

void epd_init(uint8_t sck, uint8_t mosi, uint8_t cs,
              uint8_t dc, uint8_t rst, uint8_t busy)
{
    pin_sck  = sck;
    pin_mosi = mosi;
    pin_cs   = cs;
    pin_dc   = dc;
    pin_rst  = rst;
    pin_busy = busy;

    pinMode(pin_sck, OUTPUT);
    pinMode(pin_mosi, OUTPUT);
    pinMode(pin_cs, OUTPUT);
    pinMode(pin_dc, OUTPUT);
    pinMode(pin_rst, OUTPUT);
    pinMode(pin_busy, INPUT);

    digitalWrite(pin_cs, HIGH);
    digitalWrite(pin_sck, LOW);

    epd_init_panel();
}

void epd_display(const uint8_t *bw_img, const uint8_t *red_img)
{
    /* 重新初始化面板 (从深度睡眠唤醒需要) */
    epd_init_panel();

    /* 写入 BW RAM (0x24) */
    epd_set_cursor(0x01, EPD_HEIGHT - 1);
    epd_send_cmd(0x24);
    if (bw_img) {
        for (uint16_t i = 0; i < EPD_BUF_SIZE; i++) {
            epd_send_data(pgm_read_byte(&bw_img[i]));
        }
    } else {
        for (uint16_t i = 0; i < EPD_BUF_SIZE; i++) {
            epd_send_data(0xFF);
        }
    }

    /* 写入 RED RAM (0x26) */
    epd_set_cursor(0x01, EPD_HEIGHT - 1);
    epd_send_cmd(0x26);
    if (red_img) {
        for (uint16_t i = 0; i < EPD_BUF_SIZE; i++) {
            epd_send_data(pgm_read_byte(&red_img[i]));
        }
    } else {
        for (uint16_t i = 0; i < EPD_BUF_SIZE; i++) {
            epd_send_data(0x00);
        }
    }

    epd_update_full();
    epd_sleep();
}

void epd_display_bw(const uint8_t *bw_img)
{
    epd_display(bw_img, NULL);
}

void epd_clear(void)
{
    epd_display(NULL, NULL);
}

void epd_sleep(void)
{
    /* Deep Sleep Mode: 进入模式 1, 唤醒需硬件复位 */
    epd_send_cmd(0x10);
    epd_send_data(0x01);
    delay(100);
}

bool epd_is_busy(void)
{
    return digitalRead(pin_busy) == HIGH;
}
