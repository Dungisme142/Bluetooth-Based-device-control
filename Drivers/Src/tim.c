#include "tim.h"

/* TIM2 counter runs at 1 MHz -> one counter tick = 1 us. */
#define TIM_COUNTER_FREQUENCY_HZ (1000000u)

/* 1000 us = 1 ms between update interrupts. */
#define TIM_INTERRUPT_PERIOD_US (1000u)

TIM_HandleTypeDef htim2;

/* Return the real clock frequency supplied to TIM2. */
static uint32_t TIM2_GetClockFrequency(void)
{
    RCC_ClkInitTypeDef clock_config;
    uint32_t flash_latency;
    uint32_t timer_clock;

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);
    timer_clock = HAL_RCC_GetPCLK1Freq();

    /*
     * On STM32F1, when APB1 prescaler is not DIV1,
     * the timer clock is 2 x PCLK1.
     */
    if (clock_config.APB1CLKDivider != RCC_HCLK_DIV1)
    {
        timer_clock *= 2u;
    }

    return timer_clock;
}

HAL_StatusTypeDef TIM_Driver_Init(void)
{
    uint32_t timer_clock;
    uint32_t prescaler;

    __HAL_RCC_TIM2_CLK_ENABLE();

    timer_clock = TIM2_GetClockFrequency();

    /* The selected clock must be able to produce an exact 1 MHz counter. */
    if ((timer_clock < TIM_COUNTER_FREQUENCY_HZ) ||
        ((timer_clock % TIM_COUNTER_FREQUENCY_HZ) != 0u))
    {
        return HAL_ERROR;
    }

    prescaler = (timer_clock / TIM_COUNTER_FREQUENCY_HZ) - 1u;

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = prescaler;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = TIM_INTERRUPT_PERIOD_US - 1u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    return HAL_OK;
}

HAL_StatusTypeDef TIM_Driver_Start(void)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);

    return HAL_TIM_Base_Start_IT(&htim2);
}

HAL_StatusTypeDef TIM_Driver_Stop(void)
{
    return HAL_TIM_Base_Stop_IT(&htim2);
}

/* Vector-table handler for TIM2. */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* HAL callback after a timer period has elapsed. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        TIM_Driver_TickCallback();
    }
}

/*
 * Default callback: intentionally empty.
 * TIMER_TASK.c provides a strong definition that overrides this weak one.
 */
__weak void TIM_Driver_TickCallback(void)
{
    /* No action when TIMER_TASK is not linked. */
}