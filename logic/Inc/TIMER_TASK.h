#ifndef TIMER_TASK_H
#define TIMER_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Periodic application events generated from the 1 ms TIM2 tick. */
#define TIMER_TASK_SENSOR_PERIOD_MS (2000u)
#define TIMER_TASK_TX_PERIOD_MS (3000u)

    /* Reset counters and software flags. Call before starting TIM2. */
    void Timer_Task_Init(void);

    /* Called once per 1 ms from TIM_Driver_TickCallback(). */
    void Timer_Task_Increment(void);

    /* Read-only flag accessors. */
    uint8_t Timer_Task_GetSensorFlag(void);
    uint8_t Timer_Task_GetTxFlag(void);

    /* Clear individual flags or both flags. */
    void Timer_Task_ClearSensorFlag(void);
    void Timer_Task_ClearTxFlag(void);
    void Timer_Task_ClearFlags(void);

    /*
     * Recommended foreground APIs: atomically read and clear one flag.
     * Return 1u when the event was pending, otherwise return 0u.
     */
    uint8_t Timer_Task_GetAndClearSensorFlag(void);
    uint8_t Timer_Task_GetAndClearTxFlag(void);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_TASK_H */