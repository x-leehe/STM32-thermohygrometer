/**
 ******************************************************************************
 * @file    main.c
 * @brief   环境监控主机 — 主程序
 * @note    功能概要:
 *          - DHT11 每秒采集温湿度，LCD 曲线显示
 *          - 每 30 秒自动存储一条记录到 SPI Flash (W25Q64)
 *          - 串口控制台 (115200)，支持 HELP/TEMP/HUMI/THRESH/FAN/DUMP/PAUSE/CLR
 *          - K1(PA0) 暂停/恢复, K2(PC13) 清除全部数据
 *          - 温湿度越限报警 (蜂鸣器 + LED2 + PWM 风机)
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "GPIO.h"
#include "LED.h"
#include "alarm.h"
#include "bsp_ili9341_lcd.h"
#include "data_logger.h"
#include "delay.h"
#include "serial_cmd.h"
#include "thermohygrometer.h"
#include "usart_console.h"
#include <stdio.h>
#include <stm32f10x.h>

/** @brief K1: PA0 — 按下为低电平 */
#define K1_PRESSED (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)

/** @brief K2: PC13 — 按下为低电平 */
#define K2_PRESSED (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0)

/** @brief 30 秒采集标志，由 TIM2 ISR 置位，主循环检测并清零 */
volatile uint8_t g_flag_30s = 0;

/** @brief 暂停标志（K1 切换），TIM2 ISR 据此控制 LED 闪烁 */
volatile uint8_t g_paused = 0;

/**
 * @brief  初始化按键 — K1(PA0), K2(PC13) 上拉输入
 */
static void Button_Init(void)
{
    GPIO_QuickInit_FirstTime(GPIOA, GPIO_Pin_0, GPIO_Speed_50MHz, GPIO_Mode_IPU);
    GPIO_QuickInit_FirstTime(GPIOC, GPIO_Pin_13, GPIO_Speed_50MHz, GPIO_Mode_IPU);
}

/**
 * @brief  初始化 TIM2 — 提供 500ms 定时基准
 * @note   72MHz / (7199+1) / (4999+1) = 2Hz (500ms)
 *         ISR 中每 500ms 翻转 LED1，每 60 次（=30s）置位 g_flag_30s
 */
static void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = 4999;
    TIM_TimeBaseStructure.TIM_Prescaler = 7199;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

/**
 * @brief  K2/CLR 清空动作 — 需在暂停状态下执行
 * @note   由 K2 按键和串口 CLR 命令共同调用
 */
void K2_ClearAction(void)
{
    if (!g_paused)
    {
        printf("ERR: Must PAUSE first before clearing data.\r\n");
        return;
    }
    DL_ClearAll();
    TH_ResetData();
}

/**
 * @brief  处理按键输入（K1 暂停/恢复, K2 清除数据）
 * @param  k1_last  K1 上一轮释放状态指针
 * @param  k2_last  K2 上一轮释放状态指针
 */
static void Process_Buttons(uint8_t *k1_last, uint8_t *k2_last)
{
    /* K1: 切换暂停/恢复 */
    if (K1_PRESSED && *k1_last)
    {
        g_paused = !g_paused;
        printf(g_paused ? "PAUSED\r\n" : "RESUMED\r\n");
    }
    *k1_last = !K1_PRESSED;

    /* K2: 清除全部 Flash 数据 + 清空 LCD 曲线 (需先暂停) */
    if (K2_PRESSED && *k2_last)
    {
        K2_ClearAction();
    }
    *k2_last = !K2_PRESSED;
}

/**
 * @brief  传感器采集 + LCD 刷新 + 串口输出 + 报警检查
 * @note   每秒调用一次
 */
static void Process_Sensor(void)
{
    TH_Update();

    if (!TH_IsDataValid())
        return;

    /* ---- 串口输出当前温湿度 ---- */
    float t = TH_GetTemperature();
    float h = TH_GetHumidity();
    printf("Temperature: %d.%d C  Humidity: %d.%d %%\r\n",
           (int)t, (int)((t - (int)t) * 10.0f + 0.5f),
           (int)h, (int)((h - (int)h) * 10.0f + 0.5f));

    /* ---- 报警检查（仅状态变化时打印）---- */
    static uint8_t last_alarm = 0;
    uint8_t alarm = Alarm_Check(t, h,
                                SC_GetTempMin(), SC_GetTempMax(),
                                SC_GetHumiMin(), SC_GetHumiMax());
    if (alarm != last_alarm)
    {
        printf(alarm ? "*** ALARM ON! ***\r\n" : "ALARM OFF\r\n");
        last_alarm = alarm;
    }
}

/**
 * @brief  30 秒定时日志记录
 * @note   由 g_flag_30s 触发，在 main 循环中调用
 */
static void Process_Logging(void)
{
    if (!g_flag_30s)
        return;
    g_flag_30s = 0;

    if (TH_IsDataValid())
    {
        DL_LogRecord(TH_GetTemperature(), TH_GetHumidity());
    }
}

void Modules_Initialize(void)
{
    delay_init();   /* Systick 定时器，提供 delay_ms / delay_us         */
    ILI9341_Init(); /* LCD 液晶屏 (SPI)                                */
    LED_Init();     /* LED1(PC2), LED2(PC3)                            */

    TH_Init();            /* 温湿度计：绘制 LCD 坐标系 + 初始化 DHT11(PC0)    */
    DL_Init();            /* 数据记录器：SPI Flash W25Q64 + 扫描写入位置      */
    Button_Init();        /* K1(PA0), K2(PC13) 上拉输入                      */
    TIM2_Init();          /* 500ms 定时中断 → LED 闪烁 + 30s 采集计时        */
    Alarm_Init();         /* BEEP(PC1) + LED2(PC3) + 风机 PWM(TIM5 CH3/CH4) */
    USART_Console_Init(); /* 串口硬件初始化 (必须在 SC_Init 之前!)            */
    SC_Init();            /* 串口命令：使能 USART1 RX 中断                    */
}


int main(void)
{
    Modules_Initialize();

    printf("Flash records: %lu\r\n", (unsigned long)DL_GetRecordCount());
    printf("Type HELP for available commands.\r\n");

    uint8_t k1_last = 0; /* K1 上一轮释放标志 (1=已释放) */
    uint8_t k2_last = 0; /* K2 上一轮释放标志             */

    while (1)
    {

        /* ---- 按键检测 ---- */
        Process_Buttons(&k1_last, &k2_last);

        /* ---- 串口命令轮询（暂停时也需要处理 CLR/DUMP 等命令）---- */
        SC_Poll();

        /* ---- 暂停状态：跳过采集，但保持蜂鸣节拍 ---- */
        if (g_paused)
        {
            Alarm_Beep_Update();
            delay_ms(100);
            continue;
        }

        /* ---- 传感器采集 (每秒 1 次) ---- */
        Process_Sensor();

        /* ---- 100ms 节拍循环 (驱动蜂鸣 + 凑满 1 秒) ---- */
        for (uint8_t tick = 0; tick < 10; tick++)
        {
            Alarm_Beep_Update();
            delay_ms(100);
        }

        /* ---- 30 秒定时日志 ---- */
        Process_Logging();
    }
}
