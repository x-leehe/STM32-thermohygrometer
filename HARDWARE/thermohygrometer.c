#include "thermohygrometer.h"
#include "delay.h"
#include <stdio.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////////////
// 温湿度计显示模块实现
// 屏幕上半绘制温度坐标系 (0~50°C)，下半绘制湿度坐标系 (20~90%)
// 实时从 DHT11 读取数据并用折线图展示历史趋势
//////////////////////////////////////////////////////////////////////////////////

// ========================== 颜色定义 (RGB565) ==========================
#define TH_COLOR_AXIS 0xFFFF       // 坐标轴（白色）
#define TH_COLOR_GRID 0x39E7       // 网格点（暗灰）
#define TH_COLOR_TEMP_BG 0x0000    // 温度区背景（黑色）
#define TH_COLOR_HUMI_BG 0x0000    // 湿度区背景（黑色）
#define TH_COLOR_TEMP_CURVE 0xF800 // 温度曲线（红色）
#define TH_COLOR_HUMI_CURVE 0x001F // 湿度曲线（蓝色）
#define TH_COLOR_SEP_LINE 0x8410   // 中间分隔线（暗灰）
#define TH_COLOR_TEXT 0xFFFF       // 文字（白色）
#define TH_COLOR_TEMP_LABEL 0xFFE0 // 温度标签（黄色）
#define TH_COLOR_HUMI_LABEL 0x07FF // 湿度标签（浅蓝）

// ========================== 环形数据缓冲区 ==========================
// 循环数组：新数据写入 g_head 位置，g_head 递增并回绕
// 显示顺序：从最旧 (g_start) 到最新 (g_head-1)，始终填满 x0→x1
static float g_temp_buf[TH_HISTORY_MAX];
static float g_humi_buf[TH_HISTORY_MAX];
static uint16_t g_head  = 0;   // 下一笔写入位置
static uint16_t g_count = 0;   // 当前有效数据点数

// 最新一次成功读取的温湿度值（供外部模块获取）
static float g_latest_temp = 0.0f;
static float g_latest_humi = 0.0f;
static uint8_t g_data_valid = 0;

// 用于保存/恢复 LCD 颜色上下文
static uint16_t g_saved_text_color;
static uint16_t g_saved_back_color;

// ========================== 内部辅助函数声明 ==========================
static void TH_SaveColors(void);
static void TH_RestoreColors(void);
static void TH_DrawTempCoordSystem(void);
static void TH_DrawHumiCoordSystem(void);
static void TH_PlotTempCurve(void);
static void TH_PlotHumiCurve(void);
static void TH_DrawSeparatorLine(void);
static void TH_ShowCurrentValues(float temp, float humi);
static const char *TH_GetComfortEmoticon(float temp, float humi);

/*
 * 高效网格虚线：用填充矩形批量写入，替代逐点 SetPointPixel。
 * ILI9341_DrawRectangle(filled=1) 内部只开窗一次 + 连续写 RAM，
 * 比循环调用 SetPointPixel（每次开 1×1 窗口）减少大量 8080 总线命令开销。
 * 每段虚线宽 4px，间隔 2px，形成清晰的虚线网格。
 */
static void TH_DrawGridDash(uint16_t x0, uint16_t x1, uint16_t y)
{
    uint16_t gx;
    for (gx = x0; gx + 4 <= x1; gx += 6)
    {
        ILI9341_DrawRectangle(gx, y, 4, 1, 1);
    }
    // 尾部不足 4px 的残余
    if (gx < x1)
    {
        ILI9341_DrawRectangle(gx, y, x1 - gx, 1, 1);
    }
}

// ========================== 颜色上下文管理 ==========================

static void TH_SaveColors(void)
{
    LCD_GetColors(&g_saved_text_color, &g_saved_back_color);
}

static void TH_RestoreColors(void)
{
    LCD_SetColors(g_saved_text_color, g_saved_back_color);
}

// ========================== 对外接口实现 ==========================

/**
 * @brief  初始化温湿度计模块
 * @note   清屏、绘制两个坐标系框架、初始化 DHT11 传感器
 */
void TH_Init(void)
{
    // 全屏填充纯黑背景
    LCD_SetBackColor(BLACK);
    ILI9341_Clear(0, 0, TH_SCREEN_W, TH_SCREEN_H);

    // 绘制坐标系框架
    TH_DrawCoordSystem();

    // 初始化 DHT11
    DHT11_Init();

    // 初始化环形缓冲区
    g_head  = 0;
    g_count = 0;
}

/**
 * @brief  清空历史曲线数据并重绘空坐标系
 * @note   不重新初始化 DHT11，仅清除显示
 */
void TH_ResetData(void)
{
    g_head  = 0;
    g_count = 0;
    TH_DrawCoordSystem();
}

/**
 * @brief  读取传感器数据并更新图表
 * @note   每次调用读取一次 DHT11 数据，追加到历史曲线
 */
void TH_Update(void)
{
    DHT11_Data_TypeDef dht11_data;
    float temp, humi;

    // 读取 DHT11，失败则跳过本次更新
    if (DHT11_Read_Data(&dht11_data) != 0)
        return;

    // 计算浮点温湿度值
    temp = (float)dht11_data.temp_int + (float)dht11_data.temp_deci * 0.1f;
    humi = (float)dht11_data.humi_int + (float)dht11_data.humi_deci * 0.1f;

    // 边界裁剪
    if (temp < TH_TEMP_MIN)
        temp = TH_TEMP_MIN;
    if (temp > TH_TEMP_MAX)
        temp = TH_TEMP_MAX;
    if (humi < TH_HUMI_MIN)
        humi = TH_HUMI_MIN;
    if (humi > TH_HUMI_MAX)
        humi = TH_HUMI_MAX;

    // 写入环形缓冲区（循环数组，无数据搬移）
    g_temp_buf[g_head] = temp;
    g_humi_buf[g_head] = humi;
    g_head = (g_head + 1) % TH_HISTORY_MAX;
    if (g_count < TH_HISTORY_MAX)
        g_count++;

    // 保存最新值，供外部模块获取
    g_latest_temp = temp;
    g_latest_humi = humi;
    g_data_valid  = 1;

    // 重绘：先画坐标系框架，再画曲线
    TH_DrawCoordSystem();
    TH_PlotTempCurve();
    TH_PlotHumiCurve();

    // 左上角显示实时数值
    TH_ShowCurrentValues(temp, humi);
}

/**
 * @brief  获取最新温度值
 * @retval 温度 (°C)，若从未成功读取则返回 0
 */
float TH_GetTemperature(void)
{
    return g_latest_temp;
}

/**
 * @brief  获取最新湿度值
 * @retval 湿度 (%)，若从未成功读取则返回 0
 */
float TH_GetHumidity(void)
{
    return g_latest_humi;
}

/**
 * @brief  查询是否至少成功读取过一次传感器数据
 * @retval 0=无效, 1=有效
 */
uint8_t TH_IsDataValid(void)
{
    return g_data_valid;
}

/**
 * @brief  绘制两个坐标系框架（分隔线 + 两个坐标轴）
 */
void TH_DrawCoordSystem(void)
{
    TH_DrawSeparatorLine();
    TH_DrawTempCoordSystem();
    TH_DrawHumiCoordSystem();
}

// ========================== 内部绘制函数 ==========================

/**
 * @brief  绘制屏幕中间的水平分隔线
 */
static void TH_DrawSeparatorLine(void)
{
    TH_SaveColors();
    LCD_SetTextColor(TH_COLOR_SEP_LINE);
    ILI9341_DrawLine(0, TH_CHART_MID_Y, TH_SCREEN_W - 1, TH_CHART_MID_Y);
    TH_RestoreColors();
}

/**
 * @brief  绘制温度坐标系（上半区域）
 *         Y 轴范围: 0 ~ 50°C，每 10°C 一条刻度线
 */
static void TH_DrawTempCoordSystem(void)
{
    uint16_t x0 = TH_TEMP_AREA_X0;
    uint16_t y0 = TH_TEMP_AREA_Y0;
    uint16_t x1 = TH_TEMP_AREA_X1;
    uint16_t y1 = TH_TEMP_AREA_Y1;
    uint16_t w = x1 - x0;
    uint16_t h = y1 - y0;
    int16_t i;
    char buf[8];

    TH_SaveColors();

    // ---- 背景填充 ----
    LCD_SetBackColor(TH_COLOR_TEMP_BG);
    ILI9341_Clear(x0, y0, w, h);

    // ---- 坐标轴（Y轴在右侧，曲线从右向左生长）----
    LCD_SetTextColor(TH_COLOR_AXIS);
    ILI9341_DrawLine(x1, y0, x1, y1); // Y 轴（右侧纵轴）
    ILI9341_DrawLine(x0, y1, x1, y1); // X 轴（底部横轴）

    // ---- Y 轴刻度与网格：0, 10, 20, 30, 40, 50 ----
    for (i = 0; i <= 50; i += 10)
    {
        uint16_t y = y1 - (uint16_t)((uint32_t)i * h / 50);

        // 刻度短线（轴右侧）
        ILI9341_DrawLine(x1, y, x1 + 5, y);

        // 网格虚线
        {
            LCD_SetTextColor(TH_COLOR_GRID);
            TH_DrawGridDash(x0, x1 - 1, y);
            LCD_SetTextColor(TH_COLOR_AXIS);
        }

        // Y 轴标签（轴右侧）
        sprintf(buf, "%d", i);
        LCD_SetTextColor(TH_COLOR_TEXT);
        LCD_SetBackColor(TH_COLOR_TEMP_BG);
        ILI9341_DispString_EN(x1 + 5, y - 8, buf);
    }

    // ---- Y 轴顶部箭头 ----
    LCD_SetTextColor(TH_COLOR_AXIS);
    ILI9341_DrawLine(x1, y0, x1 - 3, y0 + 6);
    ILI9341_DrawLine(x1, y0, x1 + 3, y0 + 6);

    // ---- X 轴左侧箭头（曲线向左生长）----
    ILI9341_DrawLine(x0, y1, x0 + 6, y1 - 3);
    ILI9341_DrawLine(x0, y1, x0 + 6, y1 + 3);

    TH_RestoreColors();
}

/**
 * @brief  绘制湿度坐标系（下半区域）
 *         Y 轴范围: 20% ~ 90%，每 10% 一条刻度线
 */
static void TH_DrawHumiCoordSystem(void)
{
    uint16_t x0 = TH_HUMI_AREA_X0;
    uint16_t y0 = TH_HUMI_AREA_Y0;
    uint16_t x1 = TH_HUMI_AREA_X1;
    uint16_t y1 = TH_HUMI_AREA_Y1;
    uint16_t w = x1 - x0;
    uint16_t h = y1 - y0;
    int16_t i;
    char buf[8];

    TH_SaveColors();

    // ---- 背景填充 ----
    LCD_SetBackColor(TH_COLOR_HUMI_BG);
    ILI9341_Clear(x0, y0, w, h);

    // ---- 坐标轴（Y轴在右侧）----
    LCD_SetTextColor(TH_COLOR_AXIS);
    ILI9341_DrawLine(x1, y0, x1, y1); // Y 轴（右侧）
    ILI9341_DrawLine(x0, y1, x1, y1); // X 轴

    // ---- Y 轴刻度：20%, 30%, ..., 90% ----
    for (i = 20; i <= 90; i += 10)
    {
        uint16_t y = y1 - (uint16_t)((uint32_t)(i - 20) * h / 70);

        // 刻度短线（轴右侧）
        ILI9341_DrawLine(x1, y, x1 + 5, y);

        // 网格虚线
        {
            LCD_SetTextColor(TH_COLOR_GRID);
            TH_DrawGridDash(x0, x1 - 1, y);
            LCD_SetTextColor(TH_COLOR_AXIS);
        }

        // Y 轴标签（轴右侧）
        sprintf(buf, "%d%%", i);
        LCD_SetTextColor(TH_COLOR_TEXT);
        LCD_SetBackColor(TH_COLOR_HUMI_BG);
        ILI9341_DispString_EN(x1 + 5, y - 8, buf);
    }

    // ---- 箭头 ----
    LCD_SetTextColor(TH_COLOR_AXIS);
    ILI9341_DrawLine(x1, y0, x1 - 3, y0 + 6);   // Y 轴顶部箭头
    ILI9341_DrawLine(x1, y0, x1 + 3, y0 + 6);
    ILI9341_DrawLine(x0, y1, x0 + 6, y1 - 3);   // X 轴左侧箭头
    ILI9341_DrawLine(x0, y1, x0 + 6, y1 + 3);

    TH_RestoreColors();
}

/**
 * @brief  在温度坐标系上绘制历史曲线（折线图）
 */
static void TH_PlotTempCurve(void)
{
    if (g_count < 2)
        return;

    uint16_t x0 = TH_TEMP_AREA_X0; // 左侧边界（最旧数据）
    uint16_t x1 = TH_TEMP_AREA_X1; // 右侧 Y 轴（最新数据紧贴此处）
    uint16_t y1 = TH_TEMP_AREA_Y1; // 底部 Y（对应 0°C）
    uint16_t w = x1 - x0;
    uint16_t h = TH_TEMP_AREA_Y1 - TH_TEMP_AREA_Y0;
    uint16_t i;

    // 环形缓冲区：最旧数据的起始索引
    uint16_t g_start = (g_count < TH_HISTORY_MAX) ? 0 : g_head;

    TH_SaveColors();
    LCD_SetTextColor(TH_COLOR_TEMP_CURVE);

    for (i = 0; i < g_count - 1; i++)
    {
        uint16_t idx0 = (g_start + i) % TH_HISTORY_MAX;
        uint16_t idx1 = (g_start + i + 1) % TH_HISTORY_MAX;

        // 最新数据始终在右侧 Y 轴 (x1)，历史数据逐次向左推开
        uint16_t x0_p = x1 - (uint32_t)(g_count - 1 - i) * w / (TH_HISTORY_MAX - 1);
        uint16_t y0_p = y1 - (uint16_t)((uint32_t)((uint16_t)(g_temp_buf[idx0] * 10)) * h / ((TH_TEMP_MAX - TH_TEMP_MIN) * 10));

        uint16_t x1_p = x1 - (uint32_t)(g_count - 2 - i) * w / (TH_HISTORY_MAX - 1);
        uint16_t y1_p = y1 - (uint16_t)((uint32_t)((uint16_t)(g_temp_buf[idx1] * 10)) * h / ((TH_TEMP_MAX - TH_TEMP_MIN) * 10));

        ILI9341_DrawLine(x0_p, y0_p, x1_p, y1_p);
    }

    TH_RestoreColors();
}

/**
 * @brief  在湿度坐标系上绘制历史曲线（折线图）
 */
static void TH_PlotHumiCurve(void)
{
    if (g_count < 2)
        return;

    uint16_t x0 = TH_HUMI_AREA_X0; // 左侧边界（最旧数据）
    uint16_t x1 = TH_HUMI_AREA_X1; // 右侧 Y 轴（最新数据紧贴此处）
    uint16_t y1 = TH_HUMI_AREA_Y1; // 底部 Y（对应 20%）
    uint16_t w = x1 - x0;
    uint16_t h = TH_HUMI_AREA_Y1 - TH_HUMI_AREA_Y0;
    uint16_t i;

    // 环形缓冲区：最旧数据的起始索引
    uint16_t g_start = (g_count < TH_HISTORY_MAX) ? 0 : g_head;

    TH_SaveColors();
    LCD_SetTextColor(TH_COLOR_HUMI_CURVE);

    for (i = 0; i < g_count - 1; i++)
    {
        uint16_t idx0 = (g_start + i) % TH_HISTORY_MAX;
        uint16_t idx1 = (g_start + i + 1) % TH_HISTORY_MAX;

        // 最新数据始终在右侧 Y 轴 (x1)，历史数据逐次向左推开
        uint16_t x0_p = x1 - (uint32_t)(g_count - 1 - i) * w / (TH_HISTORY_MAX - 1);
        uint16_t y0_p = y1 - (uint16_t)((uint32_t)((uint16_t)(g_humi_buf[idx0] * 10) - TH_HUMI_MIN * 10) * h / ((TH_HUMI_MAX - TH_HUMI_MIN) * 10));

        uint16_t x1_p = x1 - (uint32_t)(g_count - 2 - i) * w / (TH_HISTORY_MAX - 1);
        uint16_t y1_p = y1 - (uint16_t)((uint32_t)((uint16_t)(g_humi_buf[idx1] * 10) - TH_HUMI_MIN * 10) * h / ((TH_HUMI_MAX - TH_HUMI_MIN) * 10));

        ILI9341_DrawLine(x0_p, y0_p, x1_p, y1_p);
    }

    TH_RestoreColors();
}

/**
 * @brief  米家风格舒适度颜文字：根据温湿度返回对应的 ASCII 表情
 * @note   参照米家蓝牙温湿度计的 3×3 舒适度矩阵：
 *         温度: <19°C(冷) / 19~27°C(舒适) / >27°C(热)
 *         湿度: <20%(干) / 20~85%(正常) / >85%(湿)
 */
static const char *TH_GetComfortEmoticon(float temp, float humi)
{
    // 温度分级 (对应默认阈值 26~35°C)
    int t_lvl; // 0=冷, 1=舒适, 2=热
    if (temp < 26.0f)
        t_lvl = 0;
    else if (temp <= 35.0f)
        t_lvl = 1;
    else
        t_lvl = 2;

    // 湿度分级 (不变)
    int h_lvl; // 0=干, 1=正常, 2=湿
    if (humi < 20.0f)
        h_lvl = 0;
    else if (humi <= 85.0f)
        h_lvl = 1;
    else
        h_lvl = 2;

    // 3x3 颜文字矩阵
    //          t=冷(0)       t=舒适(1)      t=热(2)
    // h=干(0)  (X_X)         (-_-)          (>_<)
    // h=正常(1)(-_-)         (^_^)          (-_-)
    // h=湿(2)  (>_<)         (-_-;)         (X_X)
    static const char *emoticon[3][3] = {
        {"(X_X)", "(-_-)", "(>_<)"}, // h = Dry
        {"(-_-)", "(^_^)", "(-_-)"}, // h = Normal
        {"(>_<)", "(-_-;)", "(X_X)"} // h = Wet
    };

    return emoticon[h_lvl][t_lvl];
}

/**
 * @brief  在温度与湿度图表之间的分隔带上显示当前温湿度数值 + 舒适度颜文字
 * @note   位于温度图底(138)与分隔线(160)之间，避免遮挡坐标轴
 * @note   使用整数格式化（避免 newlib-nano 不支持 %f 的问题）
 */
static void TH_ShowCurrentValues(float temp, float humi)
{
    char buf[48];
    uint16_t disp_y = 144; // 16px 字高，y=144~160 刚好在分隔线上方
    const char *face = TH_GetComfortEmoticon(temp, humi);

    // 分离整数和小数部分（DHT11 精度 0.1°C / 0.1%）
    int t_int = (int)temp;
    int t_deci = (int)((temp - t_int) * 10.0f + 0.5f);
    int h_int = (int)humi;
    int h_deci = (int)((humi - h_int) * 10.0f + 0.5f);

    TH_SaveColors();

    // 先用黑色清除该行背景（避免网格点残留）
    LCD_SetBackColor(BLACK);
    ILI9341_Clear(0, disp_y, TH_SCREEN_W, 16);

    // 紧凑格式 + 颜文字居中显示
    sprintf(buf, "T:%d.%dC H:%d.%d%% %s", t_int, t_deci, h_int, h_deci, face);

    LCD_SetTextColor(TH_COLOR_TEXT);
    LCD_SetBackColor(BLACK);

    // 估算字符串像素宽度，水平居中（Font8x16 每字符约 8px）
    uint16_t len = strlen(buf);
    uint16_t str_w = len * 8;
    uint16_t x_start = (TH_SCREEN_W - str_w) / 2;
    if (x_start < 2)
        x_start = 2;

    ILI9341_DispString_EN(x_start, disp_y, buf);

    TH_RestoreColors();
}
