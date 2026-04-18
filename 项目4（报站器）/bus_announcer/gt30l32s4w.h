/**
 * @file    gt30l32s4w.h
 * @brief   GT30L32S4W 汉字字库芯片 SPI 驱动
 */

#ifndef GT30L32S4W_H
#define GT30L32S4W_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 字库基地址 */
#define FONT_16X16_GB2312_BASE  0x2C9D0

/**
 * @brief  初始化 GT30L32S4W SPI 接口
 * @param  sck   SPI 时钟引脚
 * @param  miso  SPI MISO 引脚 (← 芯片 SO)
 * @param  mosi  SPI MOSI 引脚 (→ 芯片 SI)
 * @param  cs    SPI CS 引脚 (→ 芯片 CS#)
 */
void gt30l_init(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs);

/**
 * @brief  读取 16×16 汉字点阵数据
 * @param  msb  GB2312 高字节
 * @param  lsb  GB2312 低字节
 * @param  buf  输出缓冲区 (至少 32 字节)
 */
void gt30l_read_hanzi_16x16(uint8_t msb, uint8_t lsb, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* GT30L32S4W_H */
