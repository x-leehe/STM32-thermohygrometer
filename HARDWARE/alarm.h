#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"

//////////////////////////////////////////////////////////////////////////////////
// 温湿度超限报警模块
//
// 功能:
//   - 根据串口设置的阈值判断温湿度是否超限
//   - 超限时: BEEP(PC1) 蜂鸣节拍 + LED2(PC3) 亮 + PWM风机(PA6/PA7) 启动
//   - 正常时: 全部关闭
//
// 硬件映射:
//   BEEP:   PC1  (需接上 PC1-BEEP 跳线帽)
//   指示灯:  PC3  (板载 D5 蓝色 LED, 低电平点亮)
//   风机PWM: PA2  (TIM5_CH3, 调速) + PA3 (TIM5_CH4, 固定0%=GND)
//           ⚠ 需拔掉 PA2/PA3 与 EEPROM 的跳线帽
//////////////////////////////////////////////////////////////////////////////////

/* ========================== 对外接口 ========================== */

void Alarm_Init(void);

/**
 * @brief  执行一次报警检查 (只控 LED2 + 风机, BEEP 由 Alarm_Beep_Update 独立驱动)
 * @retval 1=处于报警状态, 0=正常
 */
uint8_t Alarm_Check(float temperature, float humidity,
                    int16_t temp_min, int16_t temp_max,
                    int16_t humi_min, int16_t humi_max);

/**
 * @brief  蜂鸣节拍更新 (每 ~100ms 调用一次)
 * @note   报警时以 "- - - - .... .... .... ...." 循环鸣响
 *         Dash: 300ms ON / 100ms OFF, Dot: 100ms ON / 100ms OFF
 */
void Alarm_Beep_Update(void);

/**
 * @brief  手动控制风机 (用于测试, 0=关, 1~999=占空比)
 */
void Alarm_SetFanManual(uint16_t duty);

#endif /* __ALARM_H */
