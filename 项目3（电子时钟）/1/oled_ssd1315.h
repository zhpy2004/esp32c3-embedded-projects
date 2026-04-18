/**
 * @file    oled_ssd1315.h
 * @brief   SSD1315/SSD1306 OLED 裸驱动 (I2C, 128×64, 无第三方库)
 * @note    适用于 ZJY130-2864KSWRG03 等 SSD1315 模块
 *          兼容 SSD1306 驱动 IC
 */

#ifndef OLED_SSD1315_H
#define OLED_SSD1315_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数 (使用前按需修改)
 * ============================================================ */
#define OLED_DEFAULT_SDA       6
#define OLED_DEFAULT_SCL       10
#define OLED_DEFAULT_I2C_FREQ  400000UL   // 400KHz Fast Mode
#define OLED_DEFAULT_ADDR      0x3C       // SA0=0: 0x3C, SA0=1: 0x3D

#define OLED_WIDTH             128
#define OLED_HEIGHT            64
#define OLED_PAGES             (OLED_HEIGHT / 8)
#define OLED_BUF_SIZE          (OLED_WIDTH * OLED_PAGES)  // 1024 Bytes

/* ============================================================
 * 初始化 & 控制
 * ============================================================ */

/**
 * @brief  初始化 I2C 总线和 SSD1315 (按数据手册上电时序)
 * @param  sda       SDA 引脚
 * @param  scl       SCL 引脚
 * @param  i2c_freq  I2C 时钟频率 (Hz)
 * @param  addr      I2C 从机地址 (0x3C 或 0x3D)
 */
void oled_init(uint8_t sda, uint8_t scl, uint32_t i2c_freq, uint8_t addr);

/**
 * @brief  使用默认参数初始化
 *         等价于 oled_init(OLED_DEFAULT_SDA, OLED_DEFAULT_SCL,
 *                          OLED_DEFAULT_I2C_FREQ, OLED_DEFAULT_ADDR)
 */
void oled_init_default(void);

/**
 * @brief  将帧缓冲区刷新到 OLED 显存
 * @note   所有绘图操作都在帧缓冲区中进行，
 *         调用此函数后才会显示到屏幕上
 */
void oled_flush(void);

/**
 * @brief  清空帧缓冲区 (全黑)
 * @note   需调用 oled_flush() 后才生效
 */
void oled_clear(void);

/**
 * @brief  填满帧缓冲区 (全白)
 */
void oled_fill(void);

/**
 * @brief  设置显示亮度
 * @param  contrast  亮度值 (0x00~0xFF)
 */
void oled_set_contrast(uint8_t contrast);

/**
 * @brief  开启/关闭显示
 * @param  on  true=开启, false=关闭(省电)
 */
void oled_display_on(bool on);

/**
 * @brief  反显模式
 * @param  invert  true=反显, false=正常
 */
void oled_invert(bool invert);

/* ============================================================
 * 像素级绘图
 * ============================================================ */

/**
 * @brief  设置/清除单个像素
 * @param  x   列 (0~127)
 * @param  y   行 (0~63)
 * @param  on  true=点亮, false=熄灭
 */
void oled_pixel(uint8_t x, uint8_t y, bool on);

/**
 * @brief  绘制水平线
 */
void oled_hline(uint8_t x0, uint8_t x1, uint8_t y);

/**
 * @brief  绘制垂直线
 */
void oled_vline(uint8_t x, uint8_t y0, uint8_t y1);

/**
 * @brief  绘制矩形 (仅边框)
 */
void oled_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/**
 * @brief  绘制填充矩形
 */
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/* ============================================================
 * 文字绘制 (内置 6×8 ASCII 字体)
 * ============================================================ */

/**
 * @brief  绘制单个字符 (6×8)
 * @param  x     列起始 (像素)
 * @param  page  页号 (0~7), 每页 8 像素高
 * @param  ch    ASCII 字符 (空格~z)
 */
void oled_char(uint8_t x, uint8_t page, char ch);

/**
 * @brief  绘制字符串 (6×8)
 * @param  x     列起始
 * @param  page  页号
 * @param  str   字符串
 */
void oled_str(uint8_t x, uint8_t page, const char *str);

/**
 * @brief  绘制 2 倍高度字符 (12×16, 占 2 页)
 *         通过纵向拉伸 6×8 字体实现，零额外 Flash
 */
void oled_char_2x(uint8_t x, uint8_t page, char ch);

/**
 * @brief  绘制 2 倍高度字符串
 */
void oled_str_2x(uint8_t x, uint8_t page, const char *str);

/**
 * @brief  格式化打印 (类似 printf)
 * @param  x     列起始
 * @param  page  页号
 * @param  fmt   格式字符串
 * @param  ...   参数
 *
 * @example
 *   oled_printf(0, 3, "Dist: %.1f cm", 25.3);
 */
void oled_printf(uint8_t x, uint8_t page, const char *fmt, ...);

/**
 * @brief  2 倍高度格式化打印
 */
void oled_printf_2x(uint8_t x, uint8_t page, const char *fmt, ...);

/* ============================================================
 * 进度条 / 条形图
 * ============================================================ */

/**
 * @brief  绘制带边框的水平进度条
 * @param  x       左上角 x
 * @param  y       左上角 y
 * @param  w       总宽度 (像素)
 * @param  h       总高度 (像素)
 * @param  percent 填充百分比 (0.0 ~ 1.0)
 */
void oled_progress_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float percent);

/**
 * @brief  获取帧缓冲区指针 (高级用法)
 * @return 指向 1024 字节帧缓冲区的指针
 */
uint8_t *oled_get_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED_SSD1315_H */
