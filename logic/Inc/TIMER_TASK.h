#ifndef TIMER_TASK_H
#define TIMER_TASK_H

#include <stdint.h>

void Timer_Task_Init(void);
void Timer_Task_Increment(void);
uint8_t Timer_Task_GetSensorFlag(void);
uint8_t Timer_Task_GetTxFlag(void);
void Timer_Task_ClearFlags(void);

#endif
