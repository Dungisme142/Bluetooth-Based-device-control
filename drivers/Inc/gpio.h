#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

void GPIO_Init(void);
void GPIO_WritePin(uint32_t port, uint16_t pin, uint8_t value);
uint8_t GPIO_ReadPin(uint32_t port, uint16_t pin);

#endif
