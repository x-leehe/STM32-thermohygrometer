#ifndef __SERIAL_CMD_H
#define __SERIAL_CMD_H

#include "stm32f10x.h"

//////////////////////////////////////////////////////////////////////////////////
// 串口命令解析模块
//
// 通过 USART1 接收命令，支持设置温湿度报警阈值及查询当前阈值。
//
// 命令格式（以回车换行 \r\n 结束）：
//   TEMP <min> <max>   设置温度报警阈值 (整数, °C)，例: TEMP 10 35
//   HUMI <min> <max>   设置湿度报警阈值 (整数,  %)，例: HUMI 30 70
//   THRESH             查询当前温湿度报警阈值
//   FAN <duty>         手动控制风机, 0=关 999=全速, 例: FAN 500 (50%)
//   DUMP [start] [n]   导出已存储的温湿度记录, 例: DUMP 0 100
//   PAUSE              暂停/恢复数据采集 (同 K1)
//   CLR                清除全部存储数据 (同 K2)
//   HELP               查看可用命令列表
//
// 使用方式:
//   1. 初始化: SC_Init()
//   2. 在 main 循环中轮询: SC_Poll()
//   3. 外部获取阈值: SC_GetTempMin() / SC_GetTempMax() 等
//////////////////////////////////////////////////////////////////////////////////

/* 串口接收环形缓冲区大小 */
#define SC_RX_BUF_SIZE    64

/* ========================== 对外接口 ========================== */

void SC_Init(void);                              // 初始化（配置 USART1 RX 中断）

/* 阈值查询接口 */
int16_t SC_GetTempMin(void);                     // 温度下限 (°C)
int16_t SC_GetTempMax(void);                     // 温度上限 (°C)
int16_t SC_GetHumiMin(void);                     // 湿度下限 (%)
int16_t SC_GetHumiMax(void);                     // 湿度上限 (%)

/* 轮询处理（在 main 循环中调用） */
void SC_Poll(void);

/* K2/CLR 清空动作（需在暂停状态下调用，由 main.c 实现） */
void K2_ClearAction(void);

/* USART1 接收中断回调（由 stm32f10x_it.c 调用） */
void SC_ISR_ReceiveChar(uint8_t ch);

#endif /* __SERIAL_CMD_H */
