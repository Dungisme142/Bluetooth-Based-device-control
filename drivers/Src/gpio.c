#include "gpio.h"

void GPIO_Init(void)
{
    // GPIO initialization stub
}

void GPIO_WritePin(uint32_t port, uint16_t pin, uint8_t value)
{
    (void)port;
    (void)pin;
    (void)value;
}

uint8_t GPIO_ReadPin(uint32_t port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return 0;
}
