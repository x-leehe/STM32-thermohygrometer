#include "LED.h"

void LED_Init()
{
    GPIO_QuickInit_FirstTime(LED_GPIO_PORT, LED_1_PIN | LED_2_PIN, GPIO_Speed_2MHz, GPIO_Mode_Out_PP);
    GPIO_SetBits(LED_GPIO_PORT, LED_1_PIN | LED_2_PIN);
}