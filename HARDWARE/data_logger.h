#ifndef __DATA_LOGGER_H
#define __DATA_LOGGER_H

#include "stm32f10x.h"

//////////////////////////////////////////////////////////////////////////////////
// 温湿度数据记录器 — 将采集到的数据存储到 W25Q64 SPI Flash
//
// 存储方案:
//   - W25Q64 共 2048 个扇区 (每扇区 4KB)，使用末尾 48 个扇区 (2000~2047)
//   - 每条记录 8 字节: temperature(float) + humidity(float)
//   - 每扇区可存 512 条记录，48 扇区 ≈ 24576 条 ≈ 8.5 天 (30s/条)
//   - 循环写入：写满后回到首扇区擦除继续
//////////////////////////////////////////////////////////////////////////////////

/* 数据存储区：扇区 2000 ~ 2047 */
#define DL_SECTOR_START        2000
#define DL_SECTOR_COUNT        48
#define DL_SECTOR_SIZE         4096        // 每扇区 4KB
#define DL_RECORD_SIZE         8           // 每条记录 8 字节 (2 x float)

/* 每扇区可存记录数 */
#define DL_RECORDS_PER_SECTOR  (DL_SECTOR_SIZE / DL_RECORD_SIZE)  // 512

/* 总记录容量 */
#define DL_MAX_RECORDS         (DL_RECORDS_PER_SECTOR * DL_SECTOR_COUNT)

/* 扫描空位时每次最多扫描的记录数（防止启动耗时过长） */
#define DL_SCAN_CHUNK          512

/* ========================== 对外接口 ========================== */

void DL_Init(void);                                     // 初始化：扫描 Flash 定位写入位置
void DL_LogRecord(float temperature, float humidity);   // 记录一条温湿度数据
uint32_t DL_GetRecordCount(void);                       // 获取已存储的记录总数
void DL_ReadRecords(float *tempBuf, float *humiBuf,     // 批量读取记录
                    uint32_t startIdx, uint32_t count);
void DL_ClearAll(void);                                 // 擦除全部数据（临时调试用）

#endif /* __DATA_LOGGER_H */
