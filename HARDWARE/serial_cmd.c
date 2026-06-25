/**
 ******************************************************************************
 * @file    serial_cmd.c
 * @brief   串口命令解析 — 通过 USART1 控制环境监控主机
 *
 * 命令格式（以 \r\n 结束）：
 *   TEMP <min> <max>   设置温度报警阈值 (整数 °C)
 *   HUMI <min> <max>   设置湿度报警阈值 (整数 %)
 *   THRESH             查询当前阈值
 *   FAN <duty>         手动控制风机 (0~999)
 *   DUMP [start] [n]   导出已存储的温湿度记录
 *   PAUSE              暂停/恢复数据采集
 *   CLR                清除全部数据
 *   HELP               显示可用命令
 ******************************************************************************
 */

#include "serial_cmd.h"
#include "usart_console.h"
#include "alarm.h"
#include "data_logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 由 main.c 定义，PAUSE 命令复用 K1 逻辑 */
extern volatile uint8_t g_paused;

/* ========================== 阈值存储 ========================== */

/* 默认温度报警阈值: 26 ~ 35°C (舒适区间) */
static int16_t g_temp_min = 26;
static int16_t g_temp_max = 35;

/* 默认湿度报警阈值: 30 ~ 70% (舒适区间) */
static int16_t g_humi_min = 30;
static int16_t g_humi_max = 70;

/* ========================== 环形接收缓冲区 ========================== */

static uint8_t  g_rx_buf[SC_RX_BUF_SIZE];
static uint8_t  g_rx_head = 0;    // 写入位置
static uint8_t  g_rx_tail = 0;    // 读取位置（由 SC_Poll 消费）

/* ========================== 对外接口 ========================== */

/**
 * @brief  初始化串口命令模块（已在 main 中先调用 USART_Console_Init）
 * @note   配置 NVIC 并启用 USART1 接收中断 (RXNEIE)
 */
void SC_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 清空环形缓冲区 */
    g_rx_head = 0;
    g_rx_tail = 0;

    /* NVIC 配置：USART1 中断 */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 使能 USART1 接收中断 */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

/* ---- 阈值查询 ---- */

int16_t SC_GetTempMin(void) { return g_temp_min; }
int16_t SC_GetTempMax(void) { return g_temp_max; }
int16_t SC_GetHumiMin(void) { return g_humi_min; }
int16_t SC_GetHumiMax(void) { return g_humi_max; }

/* ---- USART1 接收中断回调 ---- */

/**
 * @brief  由 USART1_IRQHandler 调用，将接收到的字节存入环形缓冲区
 */
void SC_ISR_ReceiveChar(uint8_t ch)
{
    uint8_t next = (g_rx_head + 1) % SC_RX_BUF_SIZE;

    /* 缓冲区满则丢弃最旧字节 */
    if (next == g_rx_tail)
    {
        g_rx_tail = (g_rx_tail + 1) % SC_RX_BUF_SIZE;
    }

    g_rx_buf[g_rx_head] = ch;
    g_rx_head = next;
}

/* ---- 命令解析 ---- */

/**
 * @brief  从环形缓冲区读取一个字节（非阻塞）
 * @retval 1=成功, 0=缓冲区空
 */
static uint8_t SC_ReadByte(uint8_t *ch)
{
    if (g_rx_head == g_rx_tail)
        return 0;

    *ch = g_rx_buf[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1) % SC_RX_BUF_SIZE;
    return 1;
}

/**
 * @brief  尝试从缓冲区读取一行命令（以 \r\n 或 \n 结尾）
 * @param  line  输出缓冲区
 * @param  size  缓冲区大小
 * @retval 1=读取到完整行, 0=暂无完整行
 */
static uint8_t SC_ReadLine(char *line, uint8_t size)
{
    uint8_t idx = 0;
    uint8_t ch;
    uint8_t has_data = 0;

    while (idx < size - 1 && SC_ReadByte(&ch))
    {
        has_data = 1;

        /* 跳过 \r */
        if (ch == '\r')
            continue;

        /* \n 表示行结束 */
        if (ch == '\n')
        {
            line[idx] = '\0';
            return (idx > 0) ? 1 : 0;   // 忽略空行
        }

        /* 过滤不可打印字符，小写自动转大写 */
        if (ch >= 0x20 && ch <= 0x7E)
        {
            if (ch >= 'a' && ch <= 'z')
                ch -= 32;
            line[idx++] = (char)ch;
        }
    }

    /* 如果缓冲区不为空但未遇到换行，可能是不完整的命令，丢弃 */
    if (has_data && idx > 0)
    {
        line[idx] = '\0';
        /* 不做处理，等待更多字符 */
        /* 但为了避免死等，如果已经填满也尝试解析 */
    }

    return 0;   // 无完整行
}

/**
 * @brief  解析并执行一条命令
 */
static void SC_ParseCommand(const char *cmd)
{
    char keyword[8];
    int a, b;

    /* ---- THRESH: 查询当前阈值 ---- */
    if (strncmp(cmd, "THRESH", 6) == 0)
    {
        printf("\r\n=== Alarm Thresholds ===\r\n");
        printf("  Temperature: %d ~ %d C\r\n", g_temp_min, g_temp_max);
        printf("  Humidity:    %d ~ %d %%\r\n", g_humi_min, g_humi_max);
        printf("=========================\r\n\r\n");
        return;
    }

    /* ---- HELP: 显示可用命令 ---- */
    if (strncmp(cmd, "HELP", 4) == 0)
    {
        printf("\r\n=== Available Commands ===\r\n");
        printf("  TEMP <min> <max>   Set temperature alarm threshold (C)\r\n");
        printf("  HUMI <min> <max>   Set humidity alarm threshold (%%)\r\n");
        printf("  THRESH             Show current alarm thresholds\r\n");
        printf("  FAN <duty>         Manual fan control, 0=off 999=full\r\n");
        printf("  DUMP [start] [n]   Dump stored temp/humi records\r\n");
        printf("  PAUSE              Pause / resume data collection\r\n");
        printf("  CLR                Clear all stored records\r\n");
        printf("  HELP               Show this help\r\n");
        printf("===========================\r\n\r\n");
        return;
    }

    /* ---- PAUSE: 切换暂停/恢复 (同 K1) ---- */
    if (strncmp(cmd, "PAUSE", 5) == 0)
    {
        g_paused = !g_paused;
        printf(g_paused ? "PAUSED\r\n" : "RESUMED\r\n");
        return;
    }

    /* ---- CLR: 清除全部数据 (同 K2, 需先 PAUSE) ---- */
    if (strncmp(cmd, "CLR", 3) == 0)
    {
        K2_ClearAction();
        return;
    }

    /* ---- DUMP [start] [n]: 导出已存储的温湿度记录 ---- */
    if (strncmp(cmd, "DUMP", 4) == 0)
    {
        uint32_t total   = DL_GetRecordCount();
        uint32_t start   = 0;
        uint32_t count   = total;
        int     parsed   = 0;
        uint32_t i;

        if (total == 0)
        {
            printf("No records stored.\r\n");
            return;
        }

        /* 解析可选参数 */
        parsed = sscanf(cmd, "DUMP %lu %lu", (unsigned long *)&start, (unsigned long *)&count);
        if (parsed >= 1)
        {
            if (start >= total)
            {
                printf("ERR: start index %lu out of range (0 ~ %lu)\r\n",
                       (unsigned long)start, (unsigned long)(total - 1));
                return;
            }
        }
        if (parsed >= 2)
        {
            if (count == 0 || start + count > total)
                count = total - start;
        }
        else
        {
            count = total - start;
        }

        printf("\r\n=== Dumping %lu records [%lu .. %lu] ===\r\n",
               (unsigned long)count,
               (unsigned long)start,
               (unsigned long)(start + count - 1));

        for (i = 0; i < count; i++)
        {
            float t, h;
            DL_ReadRecords(&t, &h, start + i, 1);

            printf("[%lu] T=%d.%d C  H=%d.%d %%\r\n",
                   (unsigned long)(start + i),
                   (int)t, (int)((t - (int)t) * 10.0f + 0.5f),
                   (int)h, (int)((h - (int)h) * 10.0f + 0.5f));
        }

        printf("========== End of dump ==========\r\n\r\n");
        return;
    }

    /* ---- TEMP <min> <max> ---- */
    if (sscanf(cmd, "%7s %d %d", keyword, &a, &b) >= 3)
    {
        if (strcmp(keyword, "TEMP") == 0)
        {
            if (a < b && a >= -40 && b <= 80)
            {
                g_temp_min = (int16_t)a;
                g_temp_max = (int16_t)b;
                printf("OK: Temperature threshold set to %d ~ %d C\r\n", g_temp_min, g_temp_max);
            }
            else
            {
                printf("ERR: Invalid temp range. Use: TEMP <min> <max> (min<max, -40..80)\r\n");
            }
            return;
        }
    }

    /* ---- HUMI <min> <max> ---- */
    if (sscanf(cmd, "%7s %d %d", keyword, &a, &b) >= 3)
    {
        if (strcmp(keyword, "HUMI") == 0)
        {
            if (a < b && a >= 0 && b <= 100)
            {
                g_humi_min = (int16_t)a;
                g_humi_max = (int16_t)b;
                printf("OK: Humidity threshold set to %d ~ %d %%\r\n", g_humi_min, g_humi_max);
            }
            else
            {
                printf("ERR: Invalid humidity range. Use: HUMI <min> <max> (min<max, 0..100)\r\n");
            }
            return;
        }
    }

    /* ---- FAN <duty>: 手动控制风机 ---- */
    if (sscanf(cmd, "%7s %d", keyword, &a) >= 2)
    {
        if (strcmp(keyword, "FAN") == 0)
        {
            if (a >= 0 && a <= 999)
            {
                Alarm_SetFanManual((uint16_t)a);
                printf("OK: Fan set to %d/999 (%d%%)\r\n", a, (a * 100) / 999);
            }
            else
            {
                printf("ERR: Fan duty must be 0~999\r\n");
            }
            return;
        }
    }

    /* 无法识别的命令 */
    if (cmd[0] != '\0')
    {
        printf("ERR: Unknown command: \"%s\"\r\n", cmd);
        printf("     Type HELP for available commands.\r\n");
    }
}

/**
 * @brief  轮询处理串口命令（在 main 循环中调用）
 */
void SC_Poll(void)
{
    char line[SC_RX_BUF_SIZE];

    /* 逐行处理缓冲区中的所有命令 */
    while (SC_ReadLine(line, sizeof(line)))
    {
        SC_ParseCommand(line);
    }
}
