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
 * Mức ưu tiên (số nhỏ = ưu tiên cao), đặt trong MX_*_Init() và MSP của main.c:
 *      TIM2        2   watchdog/timeout của DHT11
 *      EXTI4       5   giải mã bit DHT11 (PA4) — PHẢI cao hơn UART
 *      USART2      6   nhận lệnh Bluetooth
 *      EXTI9_5     7   nút NEXT (PA5), DOWN (PA6), OK (PA7)
 *      EXTI0       7   nút UP (PB0)
 *      EXTI1       7   nút PREV (PB1)
 *      SysTick    15   TICK_INT_PRIORITY, thấp nhất
 *
 * PR nào đụng vào ngắt phải kiểm lại bảng này còn đúng không, và số ưu tiên
 * mới có làm đảo thứ tự trên hay không.
 */
#include "main.h"

/* Handle ngoại vi — định nghĩa trong main.c */
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim2;

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
 * @brief EXTI line 4 — DHT11 nằm ở PA4.
 *
 * HAL_GPIO_EXTI_IRQHandler() xoá cờ pending rồi gọi HAL_GPIO_EXTI_Callback()
 * (định nghĩa trong main.c) -> DHT11_CallbackEXTI().
 *
 * Line 4 có vector riêng, cố ý không chia sẻ với nút nào: phép đo thời gian
 * bit của DHT11 diễn ra ngay trong ISR này.
 */
void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(DHT11_PIN);
}

/**
 * @brief EXTI line 5..9 dùng chung một vector — nút NEXT (PA5), DOWN (PA6)
 *        và OK (PA7).
 *
 * Phải gọi cho từng chân: HAL_GPIO_EXTI_IRQHandler() chỉ xử lý đúng line
 * tương ứng với mask truyền vào và bỏ qua nếu cờ pending chưa bật.
 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN1_PIN);
    HAL_GPIO_EXTI_IRQHandler(BTN2_PIN);
    HAL_GPIO_EXTI_IRQHandler(BTN3_PIN);
}

/**
 * @brief EXTI line 0 — nút UP (PB0). Vector riêng, không chia sẻ với ai.
 *
 * Xử lý thực tế (chống dội phím + đổi trang) nằm trong HAL_GPIO_EXTI_Callback().
 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN4_PIN);
}

/**
 * @brief EXTI line 1 — nút PREV (PB1). Vector riêng, không chia sẻ với ai.
 */
void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN5_PIN);
}

/**
 * @brief Ngắt USART2 — đường Bluetooth (MKE-M15) @9600 trên PA2/PA3.
 *
 * Xử lý cả RXNE (nhận từng byte qua HAL_UART_Receive_IT) lẫn TXE/TC
 * (gửi qua HAL_UART_Transmit_IT trong UART_Print), và cả cờ lỗi ORE/FE/NE.
 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
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
