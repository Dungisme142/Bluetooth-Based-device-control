#include "board.h"

/* Tần số bộ đếm TIM2 — 1 MHz để mỗi tick bằng đúng 1 us */
#define TIM2_COUNTER_FREQ_HZ    1000000u

/* Handle ngoại vi dùng chung toàn hệ thống (khai báo extern trong board.h) */
UART_HandleTypeDef huart1;
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim2;

static void Board_GPIO_Init(void);
static void Board_USART1_Init(void);
static void Board_I2C1_Init(void);
static void Board_TIM2_Init(void);

void Board_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    Board_GPIO_Init();
    Board_USART1_Init();
    Board_I2C1_Init();
    Board_TIM2_Init();
}

/*
 * SYSCLK = 72 MHz: HSE 8 MHz (thạch anh trên Blue Pill) -> PLL x9.
 *
 *   HCLK  = 72 MHz (AHB  /1)
 *   PCLK2 = 72 MHz (APB2 /1) — USART1
 *   PCLK1 = 36 MHz (APB1 /2) — TRẦN CỨNG của APB1, không được để /1
 *   TIM2  = 72 MHz (APB1 prescaler != 1 nên clock timer = 2 x PCLK1)
 *
 * FLASH_LATENCY_2 là bắt buộc cho dải 48-72 MHz (RM0008 §3.3.3). Đặt sai
 * latency thì CPU đọc nhầm lệnh từ flash và chết ngay khi chuyển clock.
 *
 * PR đổi cây clock phải rà lại mọi tính toán baud và prescaler của timer.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init.HSEState = RCC_HSE_ON;
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init.PLL.PLLMUL = RCC_PLL_MUL9;     /* 8 MHz x 9 = 72 MHz */

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        Error_Handler();    /* Thạch anh HSE không dao động được */
    }

    rcc_clk_init.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                             RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

static void Board_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /* GPIOB là BẮT BUỘC: relay (PB12), DHT11 (PB15), I2C1 (PB6/PB7).
     * Thiếu clock này thì các chân port B không phản hồi gì cả. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Ghi mức TẮT trước khi init để LED active-low không chớp lúc boot */
    HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, HEARTBEAT_LED_OFF_STATE);
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, STATUS_LED_OFF_STATE);

    /* PC13 — LED heartbeat onboard (active LOW) */
    gpio_init.Pin = HEARTBEAT_LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HEARTBEAT_LED_PORT, &gpio_init);

    /* PA8 — LED chỉ báo trạng thái (active LOW) */
    gpio_init.Pin = STATUS_LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STATUS_LED_PORT, &gpio_init);

    /* PA0 / PA1 — hai nút điều hướng UI. Pull-up nội, nút kéo xuống GND nên
     * bắt cạnh XUỐNG. Không cần điện trở ngoài. */
    gpio_init.Pin = BTN_NEXT_PIN | BTN_PREV_PIN;
    gpio_init.Mode = GPIO_MODE_IT_FALLING;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BTN_NEXT_PORT, &gpio_init);

    /* Ưu tiên ngắt: EXTI của DHT11 PHẢI cao hơn UART (số nhỏ hơn = ưu tiên cao).
     * DHT11 lấy timestamp ngay trong ISR, các bit chỉ cách nhau 77-124 us.
     * Ưu tiên của USART1 đặt trong HAL_UART_MspInit(). */
    HAL_NVIC_SetPriority(DHT11_EXTI_IRQn, DHT11_EXTI_PRIO, 0);

    /* Nút nhấn: ưu tiên thấp nhất trong các ngắt của app. Bật ngay ở đây vì
     * không driver nào "sở hữu" chúng như DHT11 sở hữu PB15. */
    HAL_NVIC_SetPriority(BTN_NEXT_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN_NEXT_EXTI_IRQn);
    HAL_NVIC_SetPriority(BTN_PREV_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN_PREV_EXTI_IRQn);

    /* PB12 (relay) và PB15 (DHT11) cố tình KHÔNG cấu hình ở đây:
     * MKE_M05_RELAY_Init() và DHT11_Init() tự lo, vì chân DHT11 phải đổi
     * qua lại giữa output OD và input EXTI lúc chạy. Xem App_Init(). */
}

static void Board_USART1_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = BT_UART_BAUDRATE;    /* 9600 — mặc định của MKE-M15 */
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

static void Board_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = I2C1_CLOCK_SPEED;   /* 400 kHz Fast-mode */
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
}

/*
 * TIM2 làm đồng hồ micro-giây cho DHT11: bộ đếm PHẢI chạy đúng 1 MHz
 * (1 tick = 1 us) vì DHT11.c đo độ rộng xung trực tiếp bằng giá trị đếm.
 * Prescaler được tính từ clock thực tế thay vì ghi cứng, để đổi SYSCLK
 * không làm sai toàn bộ timing của cảm biến.
 */
static void Board_TIM2_Init(void)
{
    uint32_t timer_clock_hz;

    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Trên STM32F1, nếu APB1 prescaler khác /1 thì clock timer = 2 x PCLK1 */
    timer_clock_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        timer_clock_hz *= 2u;
    }

    if ((timer_clock_hz % TIM2_COUNTER_FREQ_HZ) != 0u) {
        Error_Handler();    /* Không chia ra được 1 MHz chính xác */
    }

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (timer_clock_hz / TIM2_COUNTER_FREQ_HZ) - 1u;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 65535u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}
