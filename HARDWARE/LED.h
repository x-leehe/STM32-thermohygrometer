#ifndef __LED_H
#define __LED_H

#include <stm32f10x.h>
#include "GPIO.h"

/*******************************************************/
/*引脚定义*/
#define LED_1_PIN                  GPIO_Pin_2
#define LED_2_PIN                  GPIO_Pin_3
#define LED_GPIO_PORT              GPIOC

/* 带参宏，可以像内联函数一样使用 */
#define LED1(a) if (a)  \
        GPIO_SetBits(LED_GPIO_PORT,LED_1_PIN);\
        else    \
        GPIO_ResetBits(LED_GPIO_PORT,LED_1_PIN)

#define LED2(a) if (a)  \
        GPIO_SetBits(LED_GPIO_PORT,LED_2_PIN);\
        else    \
        GPIO_ResetBits(LED_GPIO_PORT,LED_2_PIN)

/************************************************************/

void LED_Init();

#endif // !__LED_H