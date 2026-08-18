/*
 * board.h — Tầng phần cứng (BSP).
 *
 * Chứa toàn bộ phần khởi tạo MCU và ngoại vi: clock hệ thống, GPIO cố định,
 * USART1, I2C1, TIM2. Tầng app KHÔNG được tự cấu hình ngoại vi, chỉ dùng
 * các handle export ở đây.
 */
#ifndef BOARD_H
#define BOARD_H

#include "main.h"

/* Handle ngoại vi — định nghĩa trong board.c */
extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef  hi2c1;
extern TIM_HandleTypeDef  htim2;

/**
 * @brief  Khởi tạo HAL, clock hệ thống và toàn bộ ngoại vi của board.
 *         Phải gọi đầu tiên trong main(), trước App_Init().
 */
void Board_Init(void);

/**
 * @brief  Cấu hình clock hệ thống: HSE 8 MHz -> PLL x9 -> SYSCLK 72 MHz.
 *         Tách riêng vì HAL gọi lại sau khi thoát chế độ low-power.
 */
void SystemClock_Config(void);

/**
 * @brief  Bẫy lỗi không hồi phục được: tắt ngắt và treo vĩnh viễn.
 */
void Error_Handler(void);

#endif /* BOARD_H */
