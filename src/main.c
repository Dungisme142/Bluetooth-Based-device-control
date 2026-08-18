/*
 * main.c — Vòng lặp chính, khởi tạo tầng ứng dụng và các HAL callback.
 *
 * File này CỐ TÌNH không chứa thân của bất kỳ tính năng nào:
 *
 *      src/app_state.c    trạng thái hệ thống, relay, LED, chuỗi báo trạng thái
 *      src/app_sensor.c   đọc DHT11
 *      src/app_comm.c     kênh Bluetooth (USART1)
 *      src/app_command.c  bảng lệnh và các handler
 *      src/ui.c           4 trang OLED và 2 nút điều hướng
 *
 * Phần khởi tạo phần cứng nằm ở board.c (clock, GPIO, USART1, I2C1, TIM2) và
 * stm32f1xx_hal_msp.c (cấu hình mức thấp của từng ngoại vi).
 */
#include "board.h"

#include "app_comm.h"
#include "app_sensor.h"
#include "app_state.h"
#include "DHT11.h"
#include "ui.h"

/*==================== Hằng số cấu hình ====================*/

/* Chu kỳ các tác vụ nền (ms) */
#define SENSOR_PERIOD_MS        2000u   /* Đọc DHT11 */
#define STATUS_PERIOD_MS        3000u   /* Báo trạng thái về điện thoại */
#define HEARTBEAT_PERIOD_MS     1000u   /* Nhấp nháy LED PC13 báo còn sống */

static void App_Init(void);

/*==================== Vòng lặp chính ====================*/

int main(void)
{
    uint32_t  now_ms;
    uint32_t  last_sensor_tick_ms;
    uint32_t  last_status_tick_ms;
    uint32_t  last_heartbeat_tick_ms;
    bool      bluetooth_logged = false;
    UI_Data_t ui_data;

    Board_Init();
    App_Init();

    last_sensor_tick_ms = HAL_GetTick();
    last_status_tick_ms = last_sensor_tick_ms;
    last_heartbeat_tick_ms = last_sensor_tick_ms;

    while (1) {
        now_ms = HAL_GetTick();

        /* Mọi mốc thời gian đều so bằng hiệu (now - last): HAL_GetTick() là
         * uint32_t và tràn sau ~49,7 ngày, phép trừ unsigned vẫn đúng khi tràn
         * còn phép cộng (now >= last + T) thì không. */
        if ((now_ms - last_sensor_tick_ms) >= SENSOR_PERIOD_MS) {
            UI_Log((App_Sensor_ReadOnce() == DEV_SUCCESS) ? "DHT OK" : "DHT FAIL");
            last_sensor_tick_ms = now_ms;
        }

        if ((now_ms - last_status_tick_ms) >= STATUS_PERIOD_MS) {
            App_Comm_SendStatus();
            last_status_tick_ms = now_ms;
        }

        /* Nhấp nháy PC13 mỗi giây: dấu hiệu nhìn-là-biết vòng lặp chính còn
         * chạy. Đứng yên = treo ở Error_Handler hoặc một ISR nào đó. */
        if ((now_ms - last_heartbeat_tick_ms) >= HEARTBEAT_PERIOD_MS) {
            App_State_ToggleHeartbeat();
            last_heartbeat_tick_ms = now_ms;
        }

        /* Ghi nhật ký lần bắt tay đầu tiên với module BT ở đây chứ không ở
         * trong HAL_UART_RxCpltCallback(): UI_Log() dùng chung bộ đệm vòng với
         * UI_Task(), gọi từ ISR sẽ tranh chấp với lúc đang vẽ. */
        if (system_state.bluetooth_connected && !bluetooth_logged) {
            bluetooth_logged = true;
            UI_Log("BT LINK UP");
        }

        App_Comm_Task();

        App_State_Snapshot(&ui_data);
        UI_Task(now_ms, &ui_data);
    }
}

/*==================== Khởi tạo tầng ứng dụng ====================*/

static void App_Init(void)
{
    if (App_State_Init() != DEV_SUCCESS) {
        Error_Handler();
    }

    if (App_Sensor_Init() != DEV_SUCCESS) {
        Error_Handler();
    }

    UI_Init(&hi2c1);

    if (App_Comm_Init() != DEV_SUCCESS) {
        Error_Handler();
    }

    UI_Log("BOOT OK");
}

/*==================== Callback ngắt của HAL ====================*/

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        App_Comm_OnRxComplete();
    }
}

/**
 * @brief Khởi động lại việc nhận sau khi USART1 gặp lỗi.
 *
 * Khi bị tràn (ORE) — dễ xảy ra vì ISR của DHT11 có thể chiếm CPU lâu hơn một
 * khung 9600 baud — HAL báo lỗi và HUỶ luôn phiên Receive_IT. Không bắt lại ở
 * đây thì RXNE sẽ không bao giờ nổi nữa và Bluetooth "chết câm" sau lần tràn
 * đầu tiên, dù mọi thứ khác vẫn chạy bình thường.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        App_Comm_OnRxError();
    }
}

/**
 * @brief Điểm đến chung của mọi ngắt EXTI (DHT11 trên PB15, hai nút PA0/PA1).
 *
 * Thử nút trước vì UI_HandleButtonIrq() trả về ngay khi chân không phải của nó;
 * chân nào không ai nhận thì bỏ qua.
 */
void HAL_GPIO_EXTI_Callback(uint16_t exti_pin)
{
    if (UI_HandleButtonIrq(exti_pin)) {
        return;
    }

    DHT11_CallbackEXTI(exti_pin);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        DHT11_CallbackTIM2();
    }
}
