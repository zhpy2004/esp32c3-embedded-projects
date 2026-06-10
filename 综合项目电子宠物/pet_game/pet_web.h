/**
 * @file    pet_web.h
 * @brief   WiFi Web Server 作弊控制台 — 手机浏览器访问, 无第三方库
 * @note    ESP32-C3 连接 WiFi 后在 80 端口提供网页
 *          手机访问 http://<IP> 即可操作
 */

#ifndef PET_WEB_H
#define PET_WEB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** 连接 WiFi + 启动 Web Server, 串口输出 IP 地址 */
void web_init(void);

/** 主循环调用: 处理 HTTP 请求 + WiFi 断线重连 */
void web_run(void);

/** NTP 校时 (复用已连接的 WiFi, 不断开连接) */
bool web_ntp_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* PET_WEB_H */
