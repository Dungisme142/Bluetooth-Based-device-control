#include "TIMER_TASK.h"
#include "tim.h"

/* Millisecond counters updated from the TIM2 interrupt context. */
static volatile uint32_t g_sensor_counter_ms = 0u;
static volatile uint32_t g_tx_counter_ms = 0u;

/* Software events consumed by foreground tasks. */
static volatile uint8_t g_sensor_flag = 0u;
static volatile uint8_t g_tx_flag = 0u;

/* Enter a short critical section and return the previous IRQ mask state. */
static uint32_t Timer_Task_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/* Restore the IRQ mask state that existed before entering the critical section. */
static void Timer_Task_ExitCritical(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

void Timer_Task_Init(void)
{
    uint32_t primask = Timer_Task_EnterCritical();

    g_sensor_counter_ms = 0u;
    g_tx_counter_ms = 0u;
    g_sensor_flag = 0u;
    g_tx_flag = 0u;

    Timer_Task_ExitCritical(primask);
}

/*
 * Called every 1 ms from TIM2 interrupt context.
 * Keep this function short: only update counters and set flags.
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
    uint32_t primask = Timer_Task_EnterCritical();
    g_sensor_flag = 0u;
    Timer_Task_ExitCritical(primask);
}

void Timer_Task_ClearTxFlag(void)
{
    uint32_t primask = Timer_Task_EnterCritical();
    g_tx_flag = 0u;
    Timer_Task_ExitCritical(primask);
}

void Timer_Task_ClearFlags(void)
{
    uint32_t primask = Timer_Task_EnterCritical();
    g_sensor_flag = 0u;
    g_tx_flag = 0u;
    Timer_Task_ExitCritical(primask);
}

uint8_t Timer_Task_GetAndClearSensorFlag(void)
{
    uint8_t flag;
    uint32_t primask = Timer_Task_EnterCritical();

    flag = g_sensor_flag;
    g_sensor_flag = 0u;

    Timer_Task_ExitCritical(primask);
    return flag;
}

uint8_t Timer_Task_GetAndClearTxFlag(void)
{
    uint8_t flag;
    uint32_t primask = Timer_Task_EnterCritical();

    flag = g_tx_flag;
    g_tx_flag = 0u;

    Timer_Task_ExitCritical(primask);
    return flag;
}

/* Strong implementation overrides the weak callback in tim.c. */
void TIM_Driver_TickCallback(void)
{
    Timer_Task_Increment();
}