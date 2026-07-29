#ifndef ACTUATOR_TASK_H
#define ACTUATOR_TASK_H

#include <stdint.h>

/* Chạy task chấp hành để cập nhật trạng thái Relay và LED */
void Actuator_Task_Run(uint8_t relay_state, uint8_t led_state);

/* Xử lý lệnh điều khiển từ task protocol để thay đổi trạng thái Relay */
void Actuator_Task_HandleCommand(uint8_t relay_state);

#endif
