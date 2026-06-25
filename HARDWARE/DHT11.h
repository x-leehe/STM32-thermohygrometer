#ifndef __DHT11_H
#define __DHT11_H

#include <stm32f10x.h>
#include "sys.h"
#include "GPIO.h"

//////////////////////////////////////////////////////////////////////////////////
// DHT11 单总线温湿度传感器驱动
// 数据线 DATA 连接在 PC0 (参考 IO_table.md: DHT11/DS18B20接口 DATA)
//////////////////////////////////////////////////////////////////////////////////

// DHT11 数据引脚定义
#define DHT11_GPIO_PORT             GPIOC
#define DHT11_GPIO_PIN              GPIO_Pin_0
#define DHT11_GPIO_CLK              RCC_APB2Periph_GPIOC

// 利用 sys.h 中的位带操作，对 PC0 进行单一 IO 读写
#define DHT11_DQ_OUT                PCout(0)    // 数据线输出
#define DHT11_DQ_IN                 PCin(0)     // 数据线输入

// DHT11 一次读取的数据结构
typedef struct
{
    uint8_t humi_int;       // 湿度的整数部分
    uint8_t humi_deci;      // 湿度的小数部分
    uint8_t temp_int;       // 温度的整数部分
    uint8_t temp_deci;      // 温度的小数部分
    uint8_t check_sum;      // 校验和
} DHT11_Data_TypeDef;

// 对外接口函数
uint8_t DHT11_Init(void);                           // 初始化 DHT11，返回 0:成功 1:无应答
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *data);  // 读取一次数据，返回 0:成功 1:失败

#endif // !__DHT11_H
