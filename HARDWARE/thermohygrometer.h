#ifndef __THERMOHYGROMETER_H
#define __THERMOHYGROMETER_H

#include <stm32f10x.h>
#include "bsp_ili9341_lcd.h"
#include "DHT11.h"

//////////////////////////////////////////////////////////////////////////////////
// 温湿度计显示模块
// 在 ILI9341 屏幕上绘制温度和湿度两个坐标系，实时刷新 DHT11 数据
//////////////////////////////////////////////////////////////////////////////////

// ========================== 坐标系统布局参数 ==========================

// 屏幕尺寸（竖屏 240×320）
#define TH_SCREEN_W 240
#define TH_SCREEN_H 320

// 图表区域划分（屏幕上半: 温度，下半: 湿度）
#define TH_CHART_TOP_Y    0
#define TH_CHART_MID_Y    (TH_SCREEN_H / 2) // 120，分界线
#define TH_CHART_BOTTOM_Y TH_SCREEN_H       // 240

// 每个图表内部边距（Y 轴在右侧，标签在轴右侧）
#define TH_CHART_MARGIN_LEFT   12 // 左侧边距（曲线向左增长到此处）
#define TH_CHART_MARGIN_RIGHT  42 // 右侧 Y 轴 + 标签区
#define TH_CHART_MARGIN_TOP    14 // 顶部边距
#define TH_CHART_MARGIN_BOTTOM 22 // 底部 X 轴标签区

// 温度图表绘图区（上半区域）
#define TH_TEMP_AREA_X0 TH_CHART_MARGIN_LEFT
#define TH_TEMP_AREA_Y0 (TH_CHART_TOP_Y + TH_CHART_MARGIN_TOP)
#define TH_TEMP_AREA_X1 (TH_SCREEN_W - TH_CHART_MARGIN_RIGHT)
#define TH_TEMP_AREA_Y1 (TH_CHART_MID_Y - TH_CHART_MARGIN_BOTTOM)

// 湿度图表绘图区（下半区域）
#define TH_HUMI_AREA_X0 TH_CHART_MARGIN_LEFT
#define TH_HUMI_AREA_Y0 (TH_CHART_MID_Y + TH_CHART_MARGIN_TOP)
#define TH_HUMI_AREA_X1 (TH_SCREEN_W - TH_CHART_MARGIN_RIGHT)
#define TH_HUMI_AREA_Y1 (TH_CHART_BOTTOM_Y - TH_CHART_MARGIN_BOTTOM)

// 温度范围
#define TH_TEMP_MIN 0
#define TH_TEMP_MAX 50

// 湿度范围
#define TH_HUMI_MIN 20
#define TH_HUMI_MAX 90

// 每个图表最多保存的历史数据点数
#define TH_HISTORY_MAX 60

// ========================== 对外接口 ==========================

void TH_Init(void);            // 初始化：绘制两个坐标系
void TH_Update(void);          // 读取传感器并刷新图表
void TH_DrawCoordSystem(void); // 单独绘制/重绘坐标系框架
void TH_ResetData(void);       // 清空曲线数据并重绘空坐标系
float TH_GetTemperature(void); // 获取最新温度值 (°C)
float TH_GetHumidity(void);    // 获取最新湿度值 (%)
uint8_t TH_IsDataValid(void);  // 数据是否有效（至少成功读取过一次）

#endif // !__THERMOHYGROMETER_H
