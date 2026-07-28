#ifndef TIM_H
#define TIM_H

#include <stdint.h>

void TIM_Init(void);
void TIM_SetPeriod(uint32_t ticks);

#endif
