/**
 * @file    syn6658.cpp
 * @brief   SYN6658 UART 语音合成驱动实现
 * @note    通过 UART 发送 0x21 查询忙闲状态
 *          芯片播音完毕会自动回传 0x4F
 */

extern "C" {
#include "syn6658.h"
}

#include <Arduino.h>

/* ============================================================
 * 内部状态
 * ============================================================ */
static HardwareSerial SynSerial(1);

/* ============================================================
 * 内部: 发送单条命令帧 (无文本)
 *       帧格式: 0xFD + LenH + LenL + Cmd
 * ============================================================ */
static void send_cmd_frame(uint8_t cmd)
{
    SynSerial.write(0xFD);
    SynSerial.write((uint8_t)0x00);
    SynSerial.write((uint8_t)0x01);
    SynSerial.write(cmd);
}

/* ============================================================
 * 内部: 等待回传字节
 * ============================================================ */
static uint8_t wait_ack(uint32_t timeout_ms)
{
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
        if (SynSerial.available()) {
            return (uint8_t)SynSerial.read();
        }
        delay(1);
    }
    return 0;
}

/* ============================================================
 * API 实现
 * ============================================================ */

extern "C" bool syn6658_init(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud)
{
    SynSerial.begin(baud, SERIAL_8N1, rx_pin, tx_pin);

    Serial.printf("[SYN6658] Init  TX=%d RX=%d Baud=%lu\n", tx_pin, rx_pin, baud);
    Serial.println("[SYN6658] Waiting for init ACK (0x4A)...");

    /* 等待芯片上电回传 0x4A, 最多 3 秒 */
    uint8_t ack = wait_ack(3000);
    if (ack == SYN_ACK_INIT_OK) {
        Serial.println("[SYN6658] Init OK (0x4A)");
        return true;
    }

    /* 如果没收到 0x4A, 可能芯片已经启动完毕, 尝试查询确认通信 */
    Serial.printf("[SYN6658] No 0x4A (got 0x%02X), trying query...\n", ack);

    /* 清空残留数据 */
    while (SynSerial.available()) SynSerial.read();

    send_cmd_frame(SYN_CMD_QUERY);
    ack = wait_ack(500);
    if (ack == SYN_ACK_IDLE || ack == SYN_ACK_BUSY) {
        Serial.printf("[SYN6658] Query OK (0x%02X), chip is alive\n", ack);
        return true;
    }

    Serial.println("[SYN6658] Init FAILED - check wiring & baud rate");
    return false;
}

extern "C" bool syn6658_speak(const char *text)
{
    uint16_t text_len = strlen(text);
    uint16_t data_len = 2 + text_len;  /* 命令字(1) + 编码参数(1) + 文本 */

    /* 发送帧头 */
    SynSerial.write(0xFD);

    /* 发送数据区长度 (Big-Endian) */
    SynSerial.write((uint8_t)((data_len >> 8) & 0xFF));
    SynSerial.write((uint8_t)(data_len & 0xFF));

    /* 发送命令字: 语音合成播放 */
    SynSerial.write(SYN_CMD_SPEAK);

    /* 发送编码参数: GB2312 */
    SynSerial.write(SYN_ENC_GB2312);

    /* 发送文本数据 */
    for (uint16_t i = 0; i < text_len; i++) {
        SynSerial.write((uint8_t)text[i]);
    }

    /* 等待回传 ACK */
    uint8_t ack = wait_ack(500);
    if (ack == SYN_ACK_CMD_OK) {
        Serial.println("[SYN6658] Speak ACK OK");
        return true;
    }
    Serial.printf("[SYN6658] Speak ACK failed: 0x%02X\n", ack);
    return false;
}

extern "C" void syn6658_stop(void)
{
    send_cmd_frame(SYN_CMD_STOP);
}

extern "C" void syn6658_pause(void)
{
    send_cmd_frame(SYN_CMD_PAUSE);
}

extern "C" void syn6658_resume(void)
{
    send_cmd_frame(SYN_CMD_RESUME);
}

extern "C" uint8_t syn6658_query_status(void)
{
    /* 清空接收缓冲区, 避免残留数据干扰 */
    while (SynSerial.available()) SynSerial.read();

    send_cmd_frame(SYN_CMD_QUERY);
    return wait_ack(200);
}

extern "C" bool syn6658_is_busy(void)
{
    uint8_t status = syn6658_query_status();
    return (status == SYN_ACK_BUSY);
}

extern "C" bool syn6658_wait_idle(uint32_t timeout_ms)
{
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
        /* 方式 1: 检查芯片自动回传的 0x4F (播音完毕自动发送) */
        if (SynSerial.available()) {
            uint8_t b = SynSerial.read();
            if (b == SYN_ACK_IDLE) {
                Serial.println("[SYN6658] Playback done (0x4F)");
                return true;
            }
        }

        /* 方式 2: 每 300ms 主动查询一次 */
        uint8_t status = syn6658_query_status();
        if (status == SYN_ACK_IDLE) {
            Serial.println("[SYN6658] Playback done (query)");
            return true;
        }

        delay(300);
    }
    Serial.println("[SYN6658] Wait idle timeout");
    return false;
}
