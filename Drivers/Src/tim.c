#include "tim.h"

/* Bộ đếm TIM2 chạy với tần số 1 MHz */
#define TIM_COUNTER_FREQUENCY_HZ 1000000u

/* Một lần ngắt sau 1000 us = 1 ms */
#define TIM_INTERRUPT_PERIOD_US 1000u

TIM_HandleTypeDef htim2;

/* Lấy tần số clock thực tế cấp cho TIM2 */
static uint32_t TIM2_GetClockFrequency(void)
{
    RCC_ClkInitTypeDef clock_config;
    uint32_t flash_latency;
    uint32_t timer_clock;

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

    timer_clock = HAL_RCC_GetPCLK1Freq();

    /*
     * Nếu APB1 Prescaler khác 1,
     * clock của Timer bằng 2 lần PCLK1.
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

    /* Cấp clock cho TIM2 */
    __HAL_RCC_TIM2_CLK_ENABLE();

    timer_clock = TIM2_GetClockFrequency();

    if (timer_clock < TIM_COUNTER_FREQUENCY_HZ)
    {
        return HAL_ERROR;
    }

    /*
     * Ví dụ:
     * Timer clock = 8 MHz
     * Prescaler = 8 - 1
     * Sau prescaler, timer đếm với tần số 1 MHz.
     */
    prescaler =
        (timer_clock / TIM_COUNTER_FREQUENCY_HZ) - 1u;

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = prescaler;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = TIM_INTERRUPT_PERIOD_US - 1u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Cấu hình ngắt TIM2 */
    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
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

/* Hàm xử lý ngắt TIM2 được gọi từ vector interrupt */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/*
 * HAL gọi hàm này khi Timer hết chu kỳ.
 * Chỉ xử lý sự kiện của TIM2.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        TIM_Driver_TickCallback();
    }
}

/*
 * Callback mặc định dạng weak.
 * TIMER_TASK.c sẽ ghi đè bằng hàm thật.
 */
__weak void TIM_Driver_TickCallback(void)
{
    /* Không xử lý nếu chưa có TIMER_TASK */
}