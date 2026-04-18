/**
 * @file    station.h
 * @brief   公交车站点信息定义 (GB2312 编码)
 */

#ifndef STATION_H
#define STATION_H

#include <stdint.h>

#define STATION_COUNT   5

/* ============================================================
 * 站名 GB2312 编码数组 (供 LED 滚动显示用)
 * ============================================================ */
static const uint16_t CODE_HUOCHE[]  = {0xBBF0, 0xB3B5, 0xD5BE};           /* 火车站 */
static const uint16_t CODE_RENMIN[]  = {0xC8CB, 0xC3F1, 0xB9E3, 0xB3A1};   /* 人民广场 */
static const uint16_t CODE_ZHONGSHAN[] = {0xD6D0, 0xC9BD, 0xC2B7};         /* 中山路 */
static const uint16_t CODE_SHIZHENG[] = {0xCAD0, 0xD5FE, 0xB8AE};          /* 市政府 */
static const uint16_t CODE_KEJI[]    = {0xBFC6, 0xBCBC, 0xD4B0};           /* 科技园 */

/* ============================================================
 * 站点信息结构体
 * ============================================================ */
typedef struct {
    const char     *name_gb2312;    /* 站名 GB2312 字符串 (语音用) */
    const char     *tts_arrive;     /* 到站播报文本 */
    const char     *tts_depart;     /* 离站播报文本 */
    const uint16_t *codes;          /* 站名 GB2312 编码数组 (LED 滚动用) */
    int             code_len;       /* 编码数组长度 (汉字个数) */
} station_info_t;

/* ============================================================
 * 站点表
 * ============================================================ */
static const station_info_t station_table[STATION_COUNT] = {
    {
        "\xBB\xF0\xB3\xB5\xD5\xBE",
        "[v10][s5]\xBB\xF0\xB3\xB5\xD5\xBE\xB5\xBD\xC1\xCB",
        "[v10][s5]\xCF\xC2\xD2\xBB\xD5\xBE\xC8\xCB\xC3\xF1\xB9\xE3\xB3\xA1",
        CODE_HUOCHE, 3
    },
    {
        "\xC8\xCB\xC3\xF1\xB9\xE3\xB3\xA1",
        "[v10][s5]\xC8\xCB\xC3\xF1\xB9\xE3\xB3\xA1\xB5\xBD\xC1\xCB",
        "[v10][s5]\xCF\xC2\xD2\xBB\xD5\xBE\xD6\xD0\xC9\xBD\xC2\xB7",
        CODE_RENMIN, 4
    },
    {
        "\xD6\xD0\xC9\xBD\xC2\xB7",
        "[v10][s5]\xD6\xD0\xC9\xBD\xC2\xB7\xB5\xBD\xC1\xCB",
        "[v10][s5]\xCF\xC2\xD2\xBB\xD5\xBE\xCA\xD0\xD5\xFE\xB8\xAE",
        CODE_ZHONGSHAN, 3
    },
    {
        "\xCA\xD0\xD5\xFE\xB8\xAE",
        "[v10][s5]\xCA\xD0\xD5\xFE\xB8\xAE\xB5\xBD\xC1\xCB",
        "[v10][s5]\xCF\xC2\xD2\xBB\xD5\xBE\xBF\xC6\xBC\xBC\xD4\xB0",
        CODE_SHIZHENG, 3
    },
    {
        "\xBF\xC6\xBC\xBC\xD4\xB0",
        "[v10][s5]\xBF\xC6\xBC\xBC\xD4\xB0\xB5\xBD\xC1\xCB",
        "[v10][s5]\xD6\xD5\xB5\xE3\xD5\xBE\xB5\xBD\xC1\xCB",
        CODE_KEJI, 3
    }
};

#endif /* STATION_H */
