/**
 * @file    epd_ssd1680.h
 * @brief   2.13" BWR 三色电子墨水屏驱动 (SSD1680, 软件 SPI, 无第三方库)
 * @note    面板: ZJY122250-0213BBDMFGN-R
 *          分辨率: 122 x 250 (黑/白/红)
 *          接口: 4-wire SPI (软件模拟)
 *          全刷新时间约 15 秒
 *
 *          RAM 映射:
 *            BW RAM (0x24): bit=1 白, bit=0 黑
 *            RED RAM (0x26): bit=1 红, bit=0 非红(取 BW 值)
 *
 *          图像数据格式: 每行 16 字节 (128 像素, 实际只用 122),
 *          共 250 行, 总计 4000 字节/色
 */

#ifndef EPD_SSD1680_H
#define EPD_SSD1680_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 显示参数
 * ============================================================ */
#define EPD_WIDTH           122
#define EPD_HEIGHT          250
#define EPD_W_BYTES         16      /* ceil(122/8) = 16, 实际寻址 128 像素 */
#define EPD_BUF_SIZE        (EPD_W_BYTES * EPD_HEIGHT)  /* 4000 字节/色 */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化墨水屏引脚和 SSD1680
 * @param  sck, mosi, cs, dc, rst, busy  引脚号
 */
void epd_init(uint8_t sck, uint8_t mosi, uint8_t cs,
              uint8_t dc, uint8_t rst, uint8_t busy);

/**
 * @brief  全刷新显示 (BW + RED 双层, 从 PROGMEM 读取)
 * @param  bw_img   BW 层图像数据 (PROGMEM), 4000 字节; NULL = 全白
 * @param  red_img  RED 层图像数据 (PROGMEM), 4000 字节; NULL = 全无红
 * @note   阻塞等待刷新完成, 约 15 秒
 */
void epd_display(const uint8_t *bw_img, const uint8_t *red_img);

/**
 * @brief  仅刷新 BW 层 (RED 层清零), 从 PROGMEM 读取
 * @param  bw_img  BW 层图像数据 (PROGMEM), 4000 字节
 */
void epd_display_bw(const uint8_t *bw_img);

/**
 * @brief  清屏 (全白)
 */
void epd_clear(void);

/**
 * @brief  进入深度睡眠模式 (省电, 唤醒需硬件复位)
 */
void epd_sleep(void);

/**
 * @brief  查询墨水屏是否忙碌
 * @return true=忙碌 (正在刷新), false=空闲
 */
bool epd_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* EPD_SSD1680_H */
