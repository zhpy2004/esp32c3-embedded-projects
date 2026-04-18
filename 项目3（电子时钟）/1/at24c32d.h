/**
 * @file    at24c32d.h
 * @brief   AT24C32D I2C EEPROM 驱动 (无第三方库, 基于 Wire)
 * @note    32Kbit (4096 字节), 128 页 × 32 字节/页
 *          12-bit 字地址 (0x000 ~ 0xFFF)
 *          写周期 ≤5ms, 使用 ACK Polling 检测完成
 */

#ifndef AT24C32D_H
#define AT24C32D_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================
 * 配置参数
 * ============================================================ */
#define AT24C32D_I2C_ADDR       0x50    /* 7-bit (A2=A1=A0=0) */
#define AT24C32D_PAGE_SIZE      32      /* 32 字节/页 */
#define AT24C32D_TOTAL_BYTES    4096    /* 4096 字节 */
#define AT24C32D_ACK_TIMEOUT_MS 10      /* ACK 轮询超时 (ms) */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  写入单个字节
 * @param  addr  字地址 (0x000 ~ 0xFFF)
 * @param  data  数据字节
 * @note   写入后内部执行 ACK Polling 等待写周期完成
 */
void at24c32d_write_byte(uint16_t addr, uint8_t data);

/**
 * @brief  随机读取单个字节
 * @param  addr  字地址 (0x000 ~ 0xFFF)
 * @return 读取的数据字节
 */
uint8_t at24c32d_read_byte(uint16_t addr);

/**
 * @brief  页写入 (最多 32 字节, 同一页内)
 * @param  addr  起始地址 (A11~A5 必须相同, 即同一页)
 * @param  buf   数据缓冲区
 * @param  len   长度 (≤32), 超出自动截断
 * @note   避免跨页写入, 否则地址回绕覆盖页首数据
 */
void at24c32d_write_page(uint16_t addr, const uint8_t *buf, uint8_t len);

/**
 * @brief  顺序读取多个字节
 * @param  addr  起始字地址
 * @param  buf   输出缓冲区
 * @param  len   读取长度
 * @note   到达最大地址 (0xFFF) 后回绕至 0x000
 */
void at24c32d_read_sequential(uint16_t addr, uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* AT24C32D_H */
