#include "BEEP.h"

void Beep_Init()
{
    GPIO_QuickInit_FirstTime(GPIOC, GPIO_Pin_1, GPIO_Speed_2MHz, GPIO_Mode_Out_PP);
}