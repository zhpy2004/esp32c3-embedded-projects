/**
 * @file    syn6658.h
 * @brief   SYN6658 中文语音合成芯片 UART 驱动 (无第三方库)
 * @note    无 R_/B 引脚版本, 使用 UART 软件查询替代硬件忙检测
 */

#ifndef SYN6658_H
#define SYN6658_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 命令字定义
 * ============================================================ */
#define SYN_CMD_SPEAK       0x01
#define SYN_CMD_STOP        0x02
#define SYN_CMD_PAUSE       0x03
#define SYN_CMD_RESUME      0x04
#define SYN_CMD_QUERY       0x21
#define SYN_CMD_STANDBY     0x22
#define SYN_CMD_WAKEUP      0xFF

/* 编码参数 */
#define SYN_ENC_GB2312      0x00
#define SYN_ENC_GBK         0x01
#define SYN_ENC_UNICODE_LE  0x03

/* 回传状态码 */
#define SYN_ACK_INIT_OK     0x4A
#define SYN_ACK_CMD_OK      0x41
#define SYN_ACK_CMD_ERR     0x45
#define SYN_ACK_BUSY        0x4E
#define SYN_ACK_IDLE        0x4F

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  初始化 SYN6658 UART 接口
 * @param  tx_pin  ESP32 TX → SYN6658 RXD
 * @param  rx_pin  ESP32 RX ← SYN6658 TXD
 * @param  baud    波特率 (需与模块硬件配置一致)
 * @return true=初始化成功, false=超时
 */
bool syn6658_init(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud);

/**
 * @brief  发送 GB2312 编码文本进行语音合成
 * @param  text  GB2312 编码的中文字符串
 * @return true=发送成功(收到 0x41), false=失败
 */
bool syn6658_speak(const char *text);

/** @brief  停止当前合成 */
void syn6658_stop(void);

/** @brief  暂停合成 */
void syn6658_pause(void);

/** @brief  恢复合成 */
void syn6658_resume(void);

/**
 * @brief  查询芯片状态 (UART 软件查询, 无需 R_/B 引脚)
 * @return SYN_ACK_BUSY(0x4E)=播音中, SYN_ACK_IDLE(0x4F)=空闲, 0=超时
 */
uint8_t syn6658_query_status(void);

/**
 * @brief  检查芯片是否忙 (UART 查询, 无需 R_/B 引脚)
 * @return true=忙(播音中), false=空闲
 */
bool syn6658_is_busy(void);

/**
 * @brief  阻塞等待播音完成
 * @param  timeout_ms  超时时间 (ms)
 * @return true=播音完成, false=超时
 */
bool syn6658_wait_idle(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SYN6658_H */
