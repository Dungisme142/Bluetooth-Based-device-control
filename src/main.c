#include "main.h"
#include "uart.h"
#include "DHT11.h"
#include "SSD1306.h"
#include "Command_Selector.h"
#include "Frame_Builder.h"
#include "Ring_Buffer.h"

#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;

#define DHT11_PORT      GPIOA
#define DHT11_PIN       GPIO_PIN_2
#define RELAY_PORT      GPIOA
#define RELAY_PIN       GPIO_PIN_0
#define LED_PORT        GPIOC
#define LED_PIN         GPIO_PIN_13

#define UART_RX_BUFFER_SIZE     128u
#define UART_FRAME_BUFFER_SIZE  128u

static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t uart_frame_buffer[UART_FRAME_BUFFER_SIZE];
static uint8_t uart_rx_byte = 0u;

static Ring_Buffer_HandleTypeDef ring_buffer_handler;
static Framing_Result_HandleTypeDef framing_result_handler;
static Developer_UART_HandleTypeDef developer_uart_handler;
static ssd1306_t oled_display;

static uint8_t relay_state = 0u;
static uint8_t bluetooth_connected = 0u;
static uint8_t last_temp = 0u;
static uint8_t last_humidity = 0u;
static uint32_t last_sensor_tick_ms = 0u;
static uint32_t last_status_tick_ms = 0u;

extern DHT11_Config_t g_dht_pin;
static DHT11_Config_t dht11_cfg = { DHT11_PORT, DHT11_PIN };

static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
void Error_Handler(void);
static void OLED_InitScreen(void);
static void OLED_ShowStatus(void);
static uint8_t DHT11_ReadOnce(void);
static void SendStatusToPhone(void);

static void Command_ON(char *return_msg, const char *args);
static void Command_OFF(char *return_msg, const char *args);
static void Command_STATUS(char *return_msg, const char *args);
static void Command_TEMP(char *return_msg, const char *args);
static void Command_HUM(char *return_msg, const char *args);
static void Command_AUTO(char *return_msg, const char *args);

uint8_t Command_Menu_Size = 6u;
Command_HandleTypeDef Command_Menu[] = {
    {"ON", Command_ON},
    {"OFF", Command_OFF},
    {"STATUS", Command_STATUS},
    {"TEMP", Command_TEMP},
    {"HUM", Command_HUM},
    {"AUTO", Command_AUTO}
};

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio_init.Pin = RELAY_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_PORT, &gpio_init);

    gpio_init.Pin = LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio_init);

    gpio_init.Pin = DHT11_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &gpio_init);

    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
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

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
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

static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 7u;
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

static void OLED_InitScreen(void)
{
    if (SSD1306_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    oled_display.width = SSD1306_WIDTH;
    oled_display.height = SSD1306_HEIGHT;
    SSD1306_Clear(&oled_display);
    SSD1306_WriteString(&oled_display, 0, 0, "Booting...", SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(&hi2c1, &oled_display);
}

static void OLED_ShowStatus(void)
{
    char line1[24];
    char line2[24];
    char line3[24];

    snprintf(line1, sizeof(line1), "TEMP:%uC", last_temp);
    snprintf(line2, sizeof(line2), "HUM:%u%%", last_humidity);
    snprintf(line3, sizeof(line3), "RELAY:%s BT:%s",
             relay_state ? "ON" : "OFF",
             bluetooth_connected ? "OK" : "NO");

    SSD1306_Clear(&oled_display);
    SSD1306_WriteString(&oled_display, 0, 0, line1, SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&oled_display, 0, 16, line2, SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&oled_display, 0, 32, line3, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(&hi2c1, &oled_display);
}

static uint8_t DHT11_ReadOnce(void)
{
    DHT11_Data_t dht_data = {0};

    DHT11_StartRequest(&dht11_cfg);

    for (uint32_t i = 0u; i < 100u; i++) {
        DHT11_State_t state = DHT11_ReadData(&dht_data);

        if (state == DHT11_STATE_COMPLETE) {
            if (dht_data.is_valid) {
                last_temp = dht_data.temp_int;
                last_humidity = dht_data.humidity_int;
                return 1u;
            }
            return 0u;
        }

        if (state == DHT11_STATE_ERROR) {
            return 0u;
        }

        HAL_Delay(5u);
    }

    return 0u;
}

static void SendStatusToPhone(void)
{
    char msg[128];
    snprintf(msg, sizeof(msg), "TEMP=%uC HUM=%u%% RELAY=%s\r\n",
             last_temp,
             last_humidity,
             relay_state ? "ON" : "OFF");
    UART_Print(&developer_uart_handler, msg);
}

static void Command_ON(char *return_msg, const char *args)
{
    (void)args;
    relay_state = 1u;
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    snprintf(return_msg, 256, "RELAY_ON\r\n");
}

static void Command_OFF(char *return_msg, const char *args)
{
    (void)args;
    relay_state = 0u;
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    snprintf(return_msg, 256, "RELAY_OFF\r\n");
}

static void Command_STATUS(char *return_msg, const char *args)
{
    (void)args;
    snprintf(return_msg, 256, "TEMP=%uC HUM=%u%% RELAY=%s BT=%s\r\n",
             last_temp,
             last_humidity,
             relay_state ? "ON" : "OFF",
             bluetooth_connected ? "OK" : "NO");
}

static void Command_TEMP(char *return_msg, const char *args)
{
    (void)args;
    snprintf(return_msg, 256, "TEMP=%uC\r\n", last_temp);
}

static void Command_HUM(char *return_msg, const char *args)
{
    (void)args;
    snprintf(return_msg, 256, "HUM=%u%%\r\n", last_humidity);
}

static void Command_AUTO(char *return_msg, const char *args)
{
    (void)args;
    snprintf(return_msg, 256, "AUTO_MODE_READY\r\n");
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    rcc_osc_init.HSIState = RCC_HSI_ON;
    rcc_osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    rcc_osc_init.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        Error_Handler();
    }

    rcc_clk_init.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                             RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

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

        gpio_init.Pin = GPIO_PIN_9;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        gpio_init.Pin = GPIO_PIN_10;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (Ring_Buffer_Write_SingleData(&ring_buffer_handler, &uart_rx_byte) == DEV_SUCCESS) {
            bluetooth_connected = 1u;
            developer_uart_handler.last_received_tick = HAL_GetTick();
            developer_uart_handler.uart_connecting_status = developer_uart_connected;
        }
        HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t exti_pin)
{
    if (exti_pin == DHT11_PIN) {
        DHT11_CallbackEXTI(exti_pin);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        DHT11_CallbackTIM2();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

int main(void)
{
    uint32_t now_ms = 0u;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();

    dht11_cfg.Port = DHT11_PORT;
    dht11_cfg.Pin = DHT11_PIN;
    g_dht_pin = dht11_cfg;
    DHT11_Init();

    OLED_InitScreen();

    Ring_Buffer_Init(&ring_buffer_handler, uart_rx_buffer, UART_RX_BUFFER_SIZE, sizeof(uint8_t));
    Frame_Builder_Init(&framing_result_handler, uart_frame_buffer, sizeof(uint8_t), UART_FRAME_BUFFER_SIZE);
    Developer_UART_Handler_Init(&developer_uart_handler, &huart1, &ring_buffer_handler, &framing_result_handler);

    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u);
    UART_Print(&developer_uart_handler, "HC-05 ready\r\n");

    last_sensor_tick_ms = HAL_GetTick();
    last_status_tick_ms = HAL_GetTick();

    while (1) {
        now_ms = HAL_GetTick();

        if ((now_ms - last_sensor_tick_ms) >= 2000u) {
            if (DHT11_ReadOnce() == 1u) {
                OLED_ShowStatus();
            }
            last_sensor_tick_ms = now_ms;
        }

        if ((now_ms - last_status_tick_ms) >= 3000u) {
            SendStatusToPhone();
            last_status_tick_ms = now_ms;
        }

        UART_Task(&developer_uart_handler);
        HAL_Delay(10u);
    }
}