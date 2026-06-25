#ifndef __BEEP_H
#define __BEEP_H

#include <stm32f10x.h>
#include "GPIO.h"

#define BEEP_PIN                    GPIO_Pin_1
#define BEEP_GPIO_PORT               GPIOC

#define BEEP(a) if (a)  \
        GPIO_SetBits(BEEP_GPIO_PORT,BEEP_PIN);\
        else    \
        GPIO_ResetBits(BEEP_GPIO_PORT,BEEP_PIN)

void Beep_Init();

#endif // !__BEEP_H
