/**
 * @file    pet_save.h
 * @brief   存档管理 — A/B 双槽 NVS 存储 / XOR 校验 / 离线恢复
 */

#ifndef PET_SAVE_H
#define PET_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 存档数据结构 (20 字节, packed, 单页可写入)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  magic;         /* 0xA5 = 有效标记 */
    uint8_t  version;       /* 存档格式版本 */
    uint8_t  slot_seq;      /* 递增序号, 判断哪个槽更新 */
    uint8_t  alive;         /* 1=存活, 0=死亡 */
    uint16_t hp;            /* 体力值 */
    uint16_t mood;          /* 心情值 */
    uint32_t coins;         /* 金币数 */
    uint8_t  appearance;    /* 外观索引 */
    uint8_t  year;          /* 0~99 (2000~2099) */
    uint8_t  month;         /* 1~12 */
    uint8_t  date;          /* 1~31 */
    uint8_t  hour;          /* 0~23 */
    uint8_t  min;           /* 0~59 */
    uint8_t  sec;           /* 0~59 */
    uint8_t  checksum;      /* 前 19 字节 XOR */
} pet_save_t;

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief  上电初始化: 读取 A/B 双槽, 校验, 恢复宠物状态, 计算离线衰减
 * @return true=找到有效存档并已恢复, false=无有效存档(已初始化新宠物)
 */
bool save_init(void);

/**
 * @brief  存档写入: 将当前宠物状态写入 NVS (A/B 交替)
 */
void save_write(void);

/**
 * @brief  获取上次加载时计算的离线分钟数 (供 UI 显示)
 */
uint32_t save_get_offline_minutes(void);

#ifdef __cplusplus
}
#endif

#endif /* PET_SAVE_H */
