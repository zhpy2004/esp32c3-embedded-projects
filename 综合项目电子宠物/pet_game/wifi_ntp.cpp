/**
 * @file    wifi_ntp.cpp
 * @brief   Wi-Fi NTP 校时模块实现
 */

extern "C" {
#include "wifi_ntp.h"
}

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

/* ============================================================
 * 内部状态
 * ============================================================ */
static bool s_syncing = false;

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" bool wifi_ntp_is_syncing(void)
{
    return s_syncing;
}

extern "C" bool wifi_ntp_sync(void)
{
    s_syncing = true;

    Serial.printf("[WiFi] Connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WiFi] Connection FAILED");
        WiFi.disconnect(true);
        s_syncing = false;
        return false;
    }
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER1, NTP_SERVER2);

    struct tm timeinfo;
    delay(2000);
    if (!getLocalTime(&timeinfo, 5000)) {
        Serial.println("[NTP] Failed to get time");
        WiFi.disconnect(true);
        s_syncing = false;
        return false;
    }

    Serial.printf("[NTP] Got: %d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    ds1307_time_t t;
    t.year  = (uint8_t)((timeinfo.tm_year + 1900) - 2000);
    t.month = (uint8_t)(timeinfo.tm_mon + 1);
    t.date  = (uint8_t)timeinfo.tm_mday;
    t.hour  = (uint8_t)timeinfo.tm_hour;
    t.min   = (uint8_t)timeinfo.tm_min;
    t.sec   = (uint8_t)timeinfo.tm_sec;
    ds1307_set_time(&t);

    WiFi.disconnect(true);
    s_syncing = false;
    Serial.println("[WiFi] Disconnected (power saving)");
    return true;
}
