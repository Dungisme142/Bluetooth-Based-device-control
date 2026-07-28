#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <stdint.h>

void Display_Task_Update(float temperature, float humidity, uint8_t relay_state, uint8_t bluetooth_connected);
void Display_Task_Refresh(void);

#endif
