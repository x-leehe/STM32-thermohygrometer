#include "DHT11.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////
// DHT11 单总线温湿度传感器驱动实现
// 数据线 DATA 连接在 PC0
//////////////////////////////////////////////////////////////////////////////////

// 将 PC0 配置为推挽输出模式
static void DHT11_Mode_Out_PP(void)
{
    GPIO_QuickInit(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_Speed_50MHz, GPIO_Mode_Out_PP);
}

// 将 PC0 配置为上拉输入模式
static void DHT11_Mode_IPU(void)
{
    GPIO_QuickInit(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_Speed_50MHz, GPIO_Mode_IPU);
}

// MCU 向 DHT11 发送复位（起始）信号
static void DHT11_Rst(void)
{
    DHT11_Mode_Out_PP();        // 配置为输出
    DHT11_DQ_OUT = 0;           // 拉低总线
    delay_ms(20);               // 拉低至少 18ms
    DHT11_DQ_OUT = 1;           // 释放总线
    delay_us(30);               // 主机拉高 20~40us
}

// 等待 DHT11 的应答信号
// 返回值: 0=DHT11 正常应答, 1=DHT11 无应答（超时）
static uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;

    DHT11_Mode_IPU();           // 切换为输入

    // DHT11 拉低总线 40~80us 作为应答
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;

    // DHT11 拉高总线 40~80us
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;

    return 0;
}

// 从 DHT11 读取一个位
// 返回值: 读取到的位值 (0 或 1)
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;

    // 等待变为低电平（上一位结束）
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }

    // 等待变为高电平（数据位开始）
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        delay_us(1);
    }

    // 高电平持续时间决定数据位:
    // 26~28us 表示 '0'，70us 表示 '1'
    delay_us(40);
    if (DHT11_DQ_IN)
        return 1;
    else
        return 0;
}

// 从 DHT11 读取一个字节 (MSB first)
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, byte = 0;

    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        byte |= DHT11_Read_Bit();
    }
    return byte;
}

// 初始化 DHT11 的 IO 口
// 返回值: 0=检测到 DHT11, 1=未检测到 DHT11
uint8_t DHT11_Init(void)
{
    // 开启 GPIOC 时钟
    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);

    DHT11_Rst();                // 发送一次复位信号
    return DHT11_Check();       // 检测应答
}

// 读取一次 DHT11 数据（温度 + 湿度）
// data: 用于保存读取结果的结构体指针
// 返回值: 0=读取成功, 1=读取失败（无应答或校验错误）
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *data)
{
    uint8_t buf[5];
    uint8_t i;

    DHT11_Rst();                // 发送起始信号

    if (DHT11_Check() != 0)     // 等待应答
        return 1;

    // 读取 40bit (5 字节) 数据
    for (i = 0; i < 5; i++)
        buf[i] = DHT11_Read_Byte();

    // 校验: 前 4 字节之和的低 8 位应等于第 5 字节
    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
        return 1;

    data->humi_int  = buf[0];
    data->humi_deci = buf[1];
    data->temp_int  = buf[2];
    data->temp_deci = buf[3];
    data->check_sum = buf[4];

    return 0;
}
