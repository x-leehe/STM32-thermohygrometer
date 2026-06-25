#ifndef __GPIO_H
#define __GPIO_H

/*
* 控制使能/失能的宏，
* 低电平使能，设置ON=0，OFF=1
* 若高电使能，设置ON=1，OFF=0 即可
*/
#define ON  0
#define OFF 1

#include <stm32f10x.h>

void GPIO_QuickInit(GPIO_TypeDef* GPIOx, uint16_t Pin, GPIOSpeed_TypeDef Speed, GPIOMode_TypeDef Mode);
void GPIO_QuickInit_FirstTime(GPIO_TypeDef* GPIOx, uint16_t Pin, GPIOSpeed_TypeDef Speed, GPIOMode_TypeDef Mode);

#endif