#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_Init(void);
void UART_SendString(const char *text);
uint16_t UART_Receive(char *buffer, uint16_t size);

#endif
