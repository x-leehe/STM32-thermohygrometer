/**
 ******************************************************************************
 * @file    alarm.c
 * @brief   温湿度超限报警 — 声(BEEP) 光(LED2) + PWM 风机(PA2/PA3)
 *
 * 硬件:
 *   BEEP:  PC1, 推挽输出, 高电平鸣响
 *   LED2:  PC3, 推挽输出, 低电平点亮 (D5 蓝色)
 *   风机:  PA3 (TIM5_CH4, PWM调速), PA2 (TIM5_CH3, 固定0%=GND, 形成压差)
 *
 *   TIM5 配置: PSC=71, ARR=999 → 1kHz PWM
 *           PA3(CH4): 0~999 调速, PA2(CH3): 固定 0 (参考地)
 *   ⚠ PA2/PA3 需拔掉 EEPROM 跳线帽 (I2C 冲突)
 ******************************************************************************
 */

#include "alarm.h"
#include "BEEP.h"
#include "GPIO.h"
#include "LED.h"

/* ========================== 内部状态 ========================== */

static uint8_t g_last_alarm_state = 0;   // 上一轮报警状态（用于日志输出）
static uint8_t g_alarm_active    = 0;   // 当前是否处于报警状态 (BEEP 机用)

/* ========================== 风机软启动 (防电流冲击 + 死区补偿) ========================== */
#define FAN_RAMP_STEP   100     // 每 100ms 爬升步长
#define FAN_DUTY_MIN    350     // 启动门槛 (低于此值电机不转)
static uint16_t g_target_fan = 0;    // 目标占空比
static uint16_t g_current_fan = 0;   // 当前实际占空比
static uint8_t  g_fan_manual  = 0;   // 手动模式标志 (1=手动, 报警变化时清零)

/* ========================== 蜂鸣节拍状态机 (100ms/tick) ========================== */
// 节拍: Dash=ON(3) OFF(1)=4ticks, Dot=ON(1) OFF(1)=2ticks
// 循环: 4×Dash + 4×Dot = 24ticks = 2.4s
#define BEEP_DASH_TICKS  4    // Dash 总 ticks
#define BEEP_DASH_ON     3    // Dash ON ticks
#define BEEP_DOT_TICKS   2    // Dot 总 ticks
#define BEEP_DOT_ON      1    // Dot ON ticks
#define BEEP_PHASES      8    // 4 dashes + 4 dots

static uint8_t g_beep_phase   = 0;   // 0~3=dash, 4~7=dot
static uint8_t g_beep_subtick = 0;   // 当前 phase 内的 tick 计数

/* ========================== 内部辅助函数 ========================== */

/** @brief 统一 TIM 时基配置 */
static void _TIM_BaseSetup(TIM_TypeDef *TIMx, u16 Period, u16 Prescaler, u16 ClockDiv, u16 CounterMode)
{
    TIM_TimeBaseInitTypeDef cfg = {
        .TIM_Period = Period,
        .TIM_Prescaler = Prescaler,
        .TIM_ClockDivision = ClockDiv,
        .TIM_CounterMode = CounterMode};
    TIM_TimeBaseInit(TIMx, &cfg);
}

/** @brief 统一 TIM OC 通道 PWM 配置并开启预装载 */
static void _TIM_OCx_PWM_Init(TIM_TypeDef *TIMx, u8 Channel, u16 Pulse, u16 OCMode, u16 OCPolarity)
{
    TIM_OCInitTypeDef cfg = {
        .TIM_OCMode = OCMode,
        .TIM_OutputState = TIM_OutputState_Enable,
        .TIM_OCPolarity = OCPolarity,
        .TIM_Pulse = Pulse};

    switch (Channel)
    {
    case 1:
        TIM_OC1Init(TIMx, &cfg);
        TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
        break;
    case 2:
        TIM_OC2Init(TIMx, &cfg);
        TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Enable);
        break;
    case 3:
        TIM_OC3Init(TIMx, &cfg);
        TIM_OC3PreloadConfig(TIMx, TIM_OCPreload_Enable);
        break;
    case 4:
        TIM_OC4Init(TIMx, &cfg);
        TIM_OC4PreloadConfig(TIMx, TIM_OCPreload_Enable);
        break;
    }
}

/* ========================== 电机 PWM 控制 (PA2/PA3) ========================== */

/** @brief 风机 PWM 频率: 72MHz / (71+1) / (999+1) = 1kHz */
#define FAN_PWM_PSC   71
#define FAN_PWM_ARR   999
#define FAN_DUTY_MAX  500   // 报警模式最高占空比 (50%), 避免电机全速拉低电压导致 LCD 白屏

/**
 * @brief  初始化 TIM5 CH3(PA2) + CH4(PA3) 为 PWM 输出
 */
static void Alarm_PWM_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    /* PA3: TIM5_CH4 — PWM 调速, PA2: TIM5_CH3 — 固定低电平 (形成压差) */
    GPIO_QuickInit(GPIOA, GPIO_Pin_2, GPIO_Speed_50MHz, GPIO_Mode_AF_PP);
    GPIO_QuickInit(GPIOA, GPIO_Pin_3, GPIO_Speed_50MHz, GPIO_Mode_AF_PP);

    /* TIM5 时基: 1kHz */
    _TIM_BaseSetup(TIM5, FAN_PWM_ARR, FAN_PWM_PSC, TIM_CKD_DIV1, TIM_CounterMode_Up);

    /* CH3 (PA2): PWM1, 占空比固定 0% — 始终低电平, 作为电机参考地 */
    _TIM_OCx_PWM_Init(TIM5, 3, 0, TIM_OCMode_PWM1, TIM_OCPolarity_High);
    /* CH4 (PA3): PWM1, 高有效, 初始占空比 0% */
    _TIM_OCx_PWM_Init(TIM5, 4, 0, TIM_OCMode_PWM1, TIM_OCPolarity_High);

    TIM_Cmd(TIM5, ENABLE);
}

/**
 * @brief  设置风机目标占空比 (软启动: 由 Alarm_Beep_Update 逐步爬升)
 * @param  duty  0=停转, FAN_PWM_ARR(999)=全速
 */
static void Alarm_SetFanSpeed(uint16_t duty)
{
    if (duty > FAN_PWM_ARR)
        duty = FAN_PWM_ARR;

    /* 非零目标低于启动门槛时，钳位到 FAN_DUTY_MIN */
    if (duty > 0 && duty < FAN_DUTY_MIN)
        duty = FAN_DUTY_MIN;

    g_target_fan = duty;
}

/**
 * @brief  风机软启动爬升 (由 Alarm_Beep_Update 每 100ms 调用)
 */
static void Alarm_Fan_Ramp(void)
{
    /* 停机：立即关断 */
    if (g_target_fan == 0) {
        g_current_fan = 0;
        TIM_SetCompare4(TIM5, 0);
        return;
    }

    uint16_t prev = g_current_fan;

    /* 首次启动：先跳到 FAN_DUTY_MIN 克服静摩擦，再缓升 */
    if (g_current_fan == 0 && g_target_fan >= FAN_DUTY_MIN) {
        g_current_fan = FAN_DUTY_MIN;
        TIM_SetCompare4(TIM5, g_current_fan);
    }

    /* 向目标爬升/下降 */
    if (g_current_fan < g_target_fan) {
        g_current_fan += FAN_RAMP_STEP;
        if (g_current_fan > g_target_fan)
            g_current_fan = g_target_fan;
    } else if (g_current_fan > g_target_fan) {
        if (g_current_fan > FAN_RAMP_STEP)
            g_current_fan -= FAN_RAMP_STEP;
        else
            g_current_fan = 0;
    }

    if (g_current_fan != prev)
        TIM_SetCompare4(TIM5, g_current_fan);
}

/**
 * @brief  手动控制风机 (用于串口命令测试)
 * @param  duty  0=关停, 1~999=占空比
 */
void Alarm_SetFanManual(uint16_t duty)
{
    g_fan_manual = 1;
    Alarm_SetFanSpeed(duty);
}

/* ========================== 对外接口 ========================== */

/**
 * @brief  初始化报警模块所有外设
 */
void Alarm_Init(void)
{
    /* 蜂鸣器初始化 (PC1, 推挽输出) */
    Beep_Init();

    /* 确保 LED2 已初始化（LED_Init 初始化了 PC2 + PC3） */
    // LED_Init() 已在 main 中提前调用

    /* PWM 风机初始化 (PA2/PA3) */
    Alarm_PWM_Init();

    /* 初始状态：全部关闭 */
    BEEP(0); // BEEP 高电平鸣响, 0=关
    LED2(OFF);
    Alarm_SetFanSpeed(0);

    g_last_alarm_state = 0;
}

/**
 * @brief  执行一次报警检查并控制外设
 * @retval 1=处于报警状态, 0=正常
 */
uint8_t Alarm_Check(float temperature, float humidity,
                    int16_t temp_min, int16_t temp_max,
                    int16_t humi_min, int16_t humi_max)
{
    uint8_t alarm = 0;

    /* 判断是否触发报警 */
    if (temperature < (float)temp_min || temperature > (float)temp_max)
        alarm = 1;

    if (humidity < (float)humi_min || humidity > (float)humi_max)
        alarm = 1;

    /* 报警状态变化时，退出手动模式，恢复自动控制 */
    static uint8_t prev_alarm = 0;
    if (alarm != prev_alarm) {
        g_fan_manual = 0;
        prev_alarm = alarm;
    }

    /* 控制外设 (BEEP 由 Alarm_Beep_Update 独立驱动) */
    if (alarm)
    {
        LED2(ON);                          // D5 蓝色 LED 亮 (低电平有效)
    }
    else
    {
        LED2(OFF);
    }

    /* 风机：手动模式保持用户设定，自动模式跟随报警状态 */
    if (!g_fan_manual) {
        Alarm_SetFanSpeed(alarm ? FAN_DUTY_MAX : 0);
    }

    g_alarm_active = alarm;
    return alarm;
}

/* ========================== 蜂鸣节拍更新 ========================== */

/**
 * @brief  蜂鸣节拍状态机 (每 ~100ms 调用一次)
 * @note   报警时输出 "- - - - .... .... .... ...." 循环
 *         Dash: 300ms ON + 100ms OFF, Dot: 100ms ON + 100ms OFF
 */
void Alarm_Beep_Update(void)
{
    /* ---- 风机软启动爬升 (每 100ms) ---- */
    Alarm_Fan_Ramp();

    /* ---- 蜂鸣节拍 ---- */
    if (!g_alarm_active) {
        BEEP(0);
        g_beep_phase   = 0;
        g_beep_subtick = 0;
        return;
    }

    uint8_t is_dash = (g_beep_phase < 4);
    uint8_t ticks   = is_dash ? BEEP_DASH_TICKS : BEEP_DOT_TICKS;
    uint8_t on_end  = is_dash ? BEEP_DASH_ON     : BEEP_DOT_ON;

    BEEP(g_beep_subtick < on_end ? 1 : 0);

    if (++g_beep_subtick >= ticks) {
        g_beep_subtick = 0;
        if (++g_beep_phase >= BEEP_PHASES)
            g_beep_phase = 0;
    }
}
