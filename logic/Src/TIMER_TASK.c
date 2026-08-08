#include "TIMER_TASK.h"
#include "tim.h"

static volatile uint32_t g_sensor_counter_ms = 0u;
static volatile uint32_t g_tx_counter_ms = 0u;

static volatile uint8_t g_sensor_flag = 0u;
static volatile uint8_t g_tx_flag = 0u;

void Timer_Task_Init(void)
{
    g_sensor_counter_ms = 0u;
    g_tx_counter_ms = 0u;

    g_sensor_flag = 0u;
    g_tx_flag = 0u;
}

/*
 * Hàm này được gọi mỗi 1 ms.
 * ISR chỉ tăng biến đếm và đặt cờ,
 * không đọc cảm biến hoặc gửi UART trong ngắt.
 */
void Timer_Task_Increment(void)
{
    g_sensor_counter_ms++;
    g_tx_counter_ms++;

    if (g_sensor_counter_ms >= TIMER_TASK_SENSOR_PERIOD_MS)
    {
        g_sensor_counter_ms = 0u;
        g_sensor_flag = 1u;
    }

    if (g_tx_counter_ms >= TIMER_TASK_TX_PERIOD_MS)
    {
        g_tx_counter_ms = 0u;
        g_tx_flag = 1u;
    }
}

uint8_t Timer_Task_GetSensorFlag(void)
{
    return g_sensor_flag;
}

uint8_t Timer_Task_GetTxFlag(void)
{
    return g_tx_flag;
}

void Timer_Task_ClearSensorFlag(void)
{
    g_sensor_flag = 0u;
}

void Timer_Task_ClearTxFlag(void)
{
    g_tx_flag = 0u;
}

void Timer_Task_ClearFlags(void)
{
    g_sensor_flag = 0u;
    g_tx_flag = 0u;
}

/*
 * Ghi đè callback weak trong tim.c.
 * Hàm được thực hiện từ ngắt TIM2 mỗi 1 ms.
 */
void TIM_Driver_TickCallback(void)
{
    Timer_Task_Increment();
}