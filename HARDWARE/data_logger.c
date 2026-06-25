/**
 ******************************************************************************
 * @file    data_logger.c
 * @brief   温湿度数据记录器 — 存储到 W25Q64 SPI Flash
 *
 * 工作原理:
 *   1. 上电时扫描数据区，找到第一个空白位置作为写入点
 *   2. 每次 DL_LogRecord() 写入 8 字节 (temp + humi)
 *   3. 当写到扇区末尾时，自动切换到下一扇区
 *   4. 当写满所有扇区时，擦除首扇区并回绕
 ******************************************************************************
 */

#include "data_logger.h"
#include "bsp_spi_flash.h"
#include <stdio.h>

/* ========================== 模块内部状态 ========================== */

static uint32_t  g_write_addr;          // 当前写入地址（Flash 绝对地址）
static uint32_t  g_record_count;        // 已存储的总记录数
static uint8_t   g_current_sector;      // 当前写入扇区索引 (0 ~ DL_SECTOR_COUNT-1)
static uint16_t  g_sector_offset;       // 当前扇区内偏移 (bytes)

/* ========================== 辅助函数 ========================== */

/**
 * @brief  将扇区索引转换为 Flash 绝对地址
 */
static uint32_t DL_SectorToAddr(uint8_t sectorIdx)
{
    return (uint32_t)(DL_SECTOR_START + sectorIdx) * DL_SECTOR_SIZE;
}

/**
 * @brief  检查 Flash 上一个 32 位字是否全为 0xFF（已擦除）
 */
static uint8_t DL_IsErased32(uint32_t addr)
{
    uint32_t val;
    SPI_FLASH_BufferRead((uint8_t *)&val, addr, 4);
    return (val == 0xFFFFFFFF) ? 1 : 0;
}

/**
 * @brief  扫描数据区，定位到第一个空白（已擦除）位置
 *
 * 从 DL_SECTOR_START 开始逐扇区、逐记录扫描，
 * 找到第一个全 0xFF 的 4 字节位置，将 g_write_addr 指向该处。
 * 若全部写满，回到首扇区擦除并重新开始。
 */
static void DL_ScanForWritePosition(void)
{
    uint8_t  sectorIdx;
    uint32_t sectorAddr;
    uint16_t recIdx;
    uint32_t checkAddr;

    g_record_count  = 0;
    g_current_sector = 0;
    g_sector_offset  = 0;

    for (sectorIdx = 0; sectorIdx < DL_SECTOR_COUNT; sectorIdx++)
    {
        sectorAddr = DL_SectorToAddr(sectorIdx);

        for (recIdx = 0; recIdx < DL_RECORDS_PER_SECTOR; recIdx++)
        {
            checkAddr = sectorAddr + (uint32_t)recIdx * DL_RECORD_SIZE;

            /* 只检查记录第一个字的擦除状态（效率优先） */
            if (DL_IsErased32(checkAddr))
            {
                /* 找到一个空位，设置为写入点 */
                g_write_addr     = checkAddr;
                g_current_sector = sectorIdx;
                g_sector_offset  = recIdx * DL_RECORD_SIZE;
                return;
            }
            g_record_count++;
        }
    }

    /* 全部写满：擦除首扇区，回绕 */
    SPI_FLASH_SectorErase(DL_SectorToAddr(0));

    g_write_addr     = DL_SectorToAddr(0);
    g_current_sector = 0;
    g_sector_offset  = 0;
    g_record_count   = DL_MAX_RECORDS;
}

/* ========================== 对外接口 ========================== */

/**
 * @brief  初始化数据记录器
 *         初始化 SPI Flash，扫描定位写入位置
 */
void DL_Init(void)
{
    /* 初始化 SPI Flash */
    SPI_FLASH_Init();

    /* 验证 Flash ID */
    if (SPI_FLASH_ReadID() != sFLASH_ID)
    {
        /* Flash 未就绪，后续写入操作将被跳过 */
        g_write_addr     = 0;
        g_record_count   = 0;
        g_current_sector = 0;
        g_sector_offset  = 0;
        return;
    }

    /* 扫描定位写入位置 */
    DL_ScanForWritePosition();
}

/**
 * @brief  记录一条温湿度数据
 * @param  temperature  温度值 (°C)
 * @param  humidity     湿度值 (%)
 *
 * @note   如果当前扇区已满，自动切换到下一扇区；
 *         如果所有扇区已满，擦除首扇区并回绕。
 */
void DL_LogRecord(float temperature, float humidity)
{
    uint8_t buf[DL_RECORD_SIZE];

    /* 未检测到 Flash，跳过 */
    if (g_write_addr == 0)
        return;

    /* 打包记录：温度 + 湿度 */
    *(float *)(buf + 0) = temperature;
    *(float *)(buf + 4) = humidity;

    /* 跨页写入由 SPI_FLASH_BufferWrite 自动处理 */
    SPI_FLASH_BufferWrite(buf, g_write_addr, DL_RECORD_SIZE);

    /* 串口输出：T=温度,H=湿度, 序号, 记录总数 */
    {
        int ti = (int)temperature;
        int td = (int)((temperature - ti) * 10.0f + 0.5f);
        int hi = (int)humidity;
        int hd = (int)((humidity - hi) * 10.0f + 0.5f);
        printf("Stored: [%lu] T=%d.%d C  H=%d.%d %%\r\n",
               (unsigned long)g_record_count,
               ti, td, hi, hd);
    }

    /* 更新位置 */
    g_write_addr     += DL_RECORD_SIZE;
    g_sector_offset  += DL_RECORD_SIZE;
    g_record_count++;

    /* 检查是否需要切换扇区 */
    if (g_sector_offset >= DL_SECTOR_SIZE)
    {
        g_current_sector++;

        if (g_current_sector >= DL_SECTOR_COUNT)
        {
            /* 回绕到首扇区，先擦除 */
            g_current_sector = 0;
            SPI_FLASH_SectorErase(DL_SectorToAddr(0));
        }

        g_write_addr    = DL_SectorToAddr(g_current_sector);
        g_sector_offset = 0;
    }
}

/**
 * @brief  获取已存储的记录总数
 */
uint32_t DL_GetRecordCount(void)
{
    return g_record_count;
}

/**
 * @brief  擦除所有数据扇区，重置记录器（临时调试功能）
 */
void DL_ClearAll(void)
{
    uint8_t i;

    printf("Clearing all records...\r\n");

    for (i = 0; i < DL_SECTOR_COUNT; i++)
    {
        SPI_FLASH_SectorErase(DL_SectorToAddr(i));
    }

    /* 重置内部状态 */
    g_write_addr     = DL_SectorToAddr(0);
    g_current_sector = 0;
    g_sector_offset  = 0;
    g_record_count   = 0;

    printf("Done. \r\n");
}

/**
 * @brief  批量读取历史记录
 * @param  tempBuf  温度缓冲区（调用方分配，至少 count 个 float）
 * @param  humiBuf  湿度缓冲区（调用方分配，至少 count 个 float）
 * @param  startIdx 起始记录索引 (0-based)
 * @param  count    读取条数
 *
 * @note   超出存储范围的记录将填充 0
 */
void DL_ReadRecords(float *tempBuf, float *humiBuf,
                    uint32_t startIdx, uint32_t count)
{
    uint32_t i;
    uint32_t addr;
    uint8_t  raw[DL_RECORD_SIZE];

    for (i = 0; i < count; i++)
    {
        if ((startIdx + i) >= g_record_count)
        {
            tempBuf[i] = 0.0f;
            humiBuf[i] = 0.0f;
            continue;
        }

        /* 循环缓冲区中第 N 条记录的地址 */
        addr = DL_SectorToAddr(0)
             + ((startIdx + i) % DL_MAX_RECORDS) * DL_RECORD_SIZE;

        SPI_FLASH_BufferRead(raw, addr, DL_RECORD_SIZE);
        tempBuf[i] = *(float *)(raw + 0);
        humiBuf[i] = *(float *)(raw + 4);
    }
}
