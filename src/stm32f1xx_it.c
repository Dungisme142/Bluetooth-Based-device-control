/*
 * stm32f1xx_it.c — Toàn bộ trình phục vụ ngắt (ISR) của firmware.
 *
 * Các hàm ở đây được bảng vector trong startup_stm32f103xb.s gọi thẳng.
 * Trong startup chúng là .weak alias tới Default_Handler (một vòng lặp vô hạn),
 * nên định nghĩa mạnh ở đây sẽ ghi đè lên.
 *
 * Nguyên tắc: ISR chỉ làm việc tối thiểu rồi đẩy sang HAL. Phần xử lý nằm ở
 * các callback trong main.c (HAL_GPIO_EXTI_Callback,
 * HAL_TIM_PeriodElapsedCallback, HAL_UART_RxCpltCallback...).
 *
 * Mức ưu tiên (số nhỏ = ưu tiên cao), đặt trong board.c và MSP:
 *      TIM2        2   watchdog/timeout của DHT11
 *      EXTI15_10   5   giải mã bit DHT11 — PHẢI cao hơn UART
 *      USART1      6   nhận lệnh Bluetooth
 *      EXTI0/1     7   nút NEXT/PREV chuyển trang UI
 *      SysTick    15   TICK_INT_PRIORITY, thấp nhất
 *
 * PR nào đụng vào ngắt phải kiểm lại bảng này còn đúng không, và số ưu tiên
 * mới có làm đảo thứ tự trên hay không.
 */
#include "board.h"

/*==================== Nhịp hệ thống ====================*/

/**
 * @brief Tăng bộ đếm tick 1 ms của HAL.
 *
 * Thiếu hàm này thì HAL_GetTick() đứng yên vĩnh viễn — firmware treo ngay sau
 * khi boot. Vòng lặp chính trong main.c dựa hoàn toàn vào HAL_GetTick().
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/*==================== Ngắt ngoại vi ====================*/

/**
 * @brief Ngắt nút NEXT (PA0) — sang trang UI kế tiếp.
 *
 * PA0 nằm một mình trên line 0 nên vector này không dùng chung với ai. Xử lý
 * thực tế (chống dội phím + đổi trang) nằm trong HAL_GPIO_EXTI_Callback().
 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN_NEXT_PIN);
}

/**
 * @brief Ngắt nút PREV (PA1) — quay về trang UI trước đó.
 */
void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN_PREV_PIN);
}

/**
 * @brief EXTI line 10..15 dùng chung một vector — DHT11 nằm ở PB15.
 *
 * HAL_GPIO_EXTI_IRQHandler() xoá cờ pending rồi gọi HAL_GPIO_EXTI_Callback()
 * (định nghĩa trong main.c) -> DHT11_CallbackEXTI().
 *
 * Chỉ khai báo đúng chân của DHT11: nếu sau này có thiết bị khác dùng line
 * 10-15 thì thêm một dòng HAL_GPIO_EXTI_IRQHandler(<pin>) ở đây.
 */
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(DHT11_PIN);
}

/**
 * @brief Ngắt USART1 — đường Bluetooth (MKE-M15) @9600.
 *
 * Xử lý cả RXNE (nhận từng byte qua HAL_UART_Receive_IT) lẫn TXE/TC
 * (gửi qua HAL_UART_Transmit_IT trong UART_Print), và cả cờ lỗi ORE/FE/NE.
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/**
 * @brief Ngắt TIM2 — đồng hồ micro-giây của DHT11.
 *
 * HAL_TIM_IRQHandler() gọi HAL_TIM_PeriodElapsedCallback() (trong main.c)
 * -> DHT11_CallbackTIM2(), đóng vai trò watchdog timeout của máy trạng thái.
 */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}
