/**
 * @file    wifi_ntp.h
 * @brief   Wi-Fi NTP 校时模块
 * @note    连接 Wi-Fi → 获取 NTP 时间 → 写入 DS1307 → 断开 Wi-Fi
 */

#ifndef WIFI_NTP_H
#define WIFI_NTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "ds1307.h"

/* ============================================================
 * 配置参数 (使用前修改)
 * ============================================================ */
#define WIFI_SSID           "iQOONeo9Pro"
#define WIFI_PASSWORD       "20040921"
#define NTP_SERVER1         "pool.ntp.org"
#define NTP_SERVER2         "time.nist.gov"
#define NTP_GMT_OFFSET      (8 * 3600)      /* UTC+8 中国标准时间 */
#define NTP_DST_OFFSET      0               /* 无夏令时 */
#define WIFI_TIMEOUT_MS     15000           /* Wi-Fi 连接超时 (ms) */

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  执行 NTP 校时: 连接 Wi-Fi → 获取时间 → 写入 DS1307 → 断开
 * @return true=校时成功, false=失败
 */
bool wifi_ntp_sync(void);

/**
 * @brief  是否正在同步中
 * @return true=同步中
 */
bool wifi_ntp_is_syncing(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_NTP_H */
