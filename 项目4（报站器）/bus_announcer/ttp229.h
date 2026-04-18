/**
 * @file    ttp229.h
 * @brief   TTP229-LSF 16 键电容触摸 I2C 驱动 (轮询模式)
 * @note    I2C 地址: 0x57 (7-bit), 仅支持读操作
 *          16 键模式需 TP2 外接 820KΩ 到 VSS
 *
 *          丝印键盘布局:
 *            0  1  2  3
 *            4  5  6  7
 *            8  9  A  B
 *            C  D  E  F
 */

#ifndef TTP229_H
#define TTP229_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 配置参数
 * ============================================================ */
#define TTP229_I2C_ADDR         0x57    /* 7-bit: 1010_111 */
#define TTP229_POLL_INTERVAL_MS 80      /* 轮询间隔 (ms) */
#define TTP229_KEY_COUNT        16      /* 总键数 */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化 TTP229 (轮询模式, 无中断)
 * @note   上电后需等待 ≥500ms 再进行首次读取
 */
void ttp229_init(void);

/**
 * @brief  读取 16 键原始状态 (未映射)
 * @return 16-bit, bit0=TP0 ... bit15=TP15, 1=按下 0=释放
 */
uint16_t ttp229_read_keys_raw(void);

/**
 * @brief  获取当前按下的单键编号 (已映射为丝印编号)
 * @return 0x0~0xF = 丝印键号, -1 = 无按键
 */
int8_t ttp229_get_key(void);

/**
 * @brief  轮询检测按键事件 (需在 loop 中周期调用)
 * @return 0x0~0xF = 新按下的丝印键号, -1 = 无新事件
 * @note   内部做消抖与边沿检测, 仅在按下瞬间返回一次键号
 */
int8_t ttp229_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* TTP229_H */
