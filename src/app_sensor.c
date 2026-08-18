/*
 * app_sensor.c — Đọc DHT11 cho tầng ứng dụng.
 */

#include "app_sensor.h"

#include "app_state.h"
#include "board.h"
#include "DHT11.h"

/* Chờ DHT11 hoàn tất một phép đo: poll mỗi 5 ms, bỏ cuộc sau 500 ms */
#define DHT11_POLL_INTERVAL_MS  5u
#define DHT11_POLL_TIMEOUT_MS   500u

static DHT11_Config_t dht11_cfg = { DHT11_PORT, DHT11_PIN, DHT11_EXTI_IRQn, &htim2 };

Developer_Action_Result_t App_Sensor_Init(void)
{
    return DHT11_Init(&dht11_cfg);
}

Developer_Action_Result_t App_Sensor_ReadOnce(void)
{
    DHT11_Data_t dht_data = {0};
    uint32_t     start_ms;
    uint32_t     last_poll_ms;

    DHT11_StartRequest();

    start_ms = HAL_GetTick();
    /* Lùi một chu kỳ để lần poll đầu tiên chạy ngay, không chờ 5 ms. */
    last_poll_ms = start_ms - DHT11_POLL_INTERVAL_MS;

    while ((HAL_GetTick() - start_ms) < DHT11_POLL_TIMEOUT_MS) {
        DHT11_State_t state;

        /* Giữ nhịp poll bằng hiệu tick thay vì HAL_Delay: không khoá CPU trong
         * SysTick handler và không phụ thuộc độ phân giải của nó. */
        if ((HAL_GetTick() - last_poll_ms) < DHT11_POLL_INTERVAL_MS) {
            continue;
        }
        last_poll_ms += DHT11_POLL_INTERVAL_MS;

        state = DHT11_ReadData(&dht_data);

        if (state == DHT11_STATE_COMPLETE) {
            if (!dht_data.is_valid) {
                return DEV_FAIL;    /* Checksum sai */
            }
            /* temp_dec / humidity_dec bị bỏ qua có ý: DHT11 luôn trả về 0 ở
             * hai trường này. */
            system_state.temperature_c = dht_data.temp_int;
            system_state.humidity_pct = dht_data.humidity_int;
            return DEV_SUCCESS;
        }

        if (state == DHT11_STATE_ERROR) {
            return DEV_FAIL;
        }
    }

    return DEV_FAIL;    /* Quá hạn: cảm biến không hoàn tất phiên đo */
}
