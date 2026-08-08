#ifndef TIM_H
#define TIM_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* TIM2 is used as the 1 ms system time base for TIMER_TASK. */
    extern TIM_HandleTypeDef htim2;

    /* Configure TIM2 to generate one update interrupt every 1 ms. */
    HAL_StatusTypeDef TIM_Driver_Init(void);

    /* Start / stop TIM2 in interrupt mode. */
    HAL_StatusTypeDef TIM_Driver_Start(void);
    HAL_StatusTypeDef TIM_Driver_Stop(void);

    /*
     * Called by the TIM2 interrupt path every 1 ms.
     * tim.c provides a weak default implementation.
     * TIMER_TASK.c overrides it with the real scheduler tick handler.
     */
    void TIM_Driver_TickCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* TIM_H */