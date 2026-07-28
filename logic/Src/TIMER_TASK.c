#include "TIMER_TASK.h"

static volatile uint32_t g_timer_counter = 0u;
static volatile uint8_t g_sensor_flag = 0u;
static volatile uint8_t g_tx_flag = 0u;

void Timer_Task_Init(void)
{
    g_timer_counter = 0u;
    g_sensor_flag = 0u;
    g_tx_flag = 0u;
}

void Timer_Task_Increment(void)
{
    g_timer_counter++;

    if ((g_timer_counter % 200u) == 0u)
    {
        g_sensor_flag = 1u;
    }

    if ((g_timer_counter % 300u) == 0u)
    {
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

void Timer_Task_ClearFlags(void)
{
    g_sensor_flag = 0u;
    g_tx_flag = 0u;
}
