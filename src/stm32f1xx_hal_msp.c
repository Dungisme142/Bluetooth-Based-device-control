/*
 * stm32f1xx_hal_msp.c — Cấu hình mức thấp của từng ngoại vi.
 *
 * HAL tự gọi các hàm này từ bên trong HAL_xxx_Init()/DeInit(). Nơi duy nhất
 * được phép bật clock ngoại vi, cấu hình chân AF và NVIC của ngoại vi đó.
 */
#include "board.h"

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();

        gpio_init.Pin = BT_UART_TX_PIN;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BT_UART_TX_PORT, &gpio_init);

        /* PA10 = RX — PULL-UP cố ý: module chưa cấp nguồn / đứt dây thì chân
         * thả nổi sẽ nhặt nhiễu và báo framing error liên tục. */
        gpio_init.Pin = BT_UART_RX_PIN;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(BT_UART_RX_PORT, &gpio_init);

        /* PHẢI thấp hơn (số lớn hơn) DHT11_EXTI_PRIO — xem pin_config.h */
        HAL_NVIC_SetPriority(BT_UART_IRQn, BT_UART_PRIO, 0);
        HAL_NVIC_EnableIRQ(BT_UART_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(BT_UART_TX_PORT, BT_UART_TX_PIN);
        HAL_GPIO_DeInit(BT_UART_RX_PORT, BT_UART_RX_PIN);
        HAL_NVIC_DisableIRQ(BT_UART_IRQn);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init.Pin = I2C1_SCL_PIN | I2C1_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(I2C1_SCL_PORT, &gpio_init);

        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(I2C1_SCL_PORT, I2C1_SCL_PIN | I2C1_SDA_PIN);
    }
}
