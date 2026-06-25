#include "GPIO.h"

void GPIO_QuickInit(GPIO_TypeDef* GPIOx, uint16_t Pin, GPIOSpeed_TypeDef Speed, GPIOMode_TypeDef Mode)
{
    GPIO_InitTypeDef GPIOInit;
    GPIOInit.GPIO_Mode  = Mode;
    GPIOInit.GPIO_Pin   = Pin;
    GPIOInit.GPIO_Speed = Speed;
    GPIO_Init(GPIOx, &GPIOInit);
}

void GPIO_QuickInit_FirstTime(GPIO_TypeDef* GPIOx, uint16_t Pin, GPIOSpeed_TypeDef Speed, GPIOMode_TypeDef Mode)
{
    if (GPIOx == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (GPIOx == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (GPIOx == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    else if (GPIOx == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    else if (GPIOx == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    else if (GPIOx == GPIOF) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
    else if (GPIOx == GPIOG) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
    GPIO_QuickInit(GPIOx, Pin, Speed, Mode);
}