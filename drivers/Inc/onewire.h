#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>

void ONEWIRE_Init(void);
uint8_t ONEWIRE_ReadByte(void);
void ONEWIRE_WriteByte(uint8_t data);

#endif
