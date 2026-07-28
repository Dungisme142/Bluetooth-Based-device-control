#ifndef ACTUATOR_TASK_H
#define ACTUATOR_TASK_H

#include <stdint.h>

void Actuator_Task_Run(uint8_t relay_state, uint8_t led_state);
void Actuator_Task_HandleCommand(uint8_t relay_state);

#endif
