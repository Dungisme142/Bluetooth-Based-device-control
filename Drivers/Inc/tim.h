#ifndef TIM_H
#define TIM_H

#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;

/* Cấu hình TIM2 tạo ngắt mỗi 1 ms */
HAL_StatusTypeDef TIM_Driver_Init(void);

/* Bắt đầu và dừng TIM2 ở chế độ ngắt */
HAL_StatusTypeDef TIM_Driver_Start(void);
HAL_StatusTypeDef TIM_Driver_Stop(void);

/*
 * Hàm callback được gọi mỗi 1 ms.
 * TIMER_TASK.c sẽ định nghĩa lại hàm này.
 */
void TIM_Driver_TickCallback(void);

#endif