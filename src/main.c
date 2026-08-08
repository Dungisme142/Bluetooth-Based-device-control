/* USER CODE BEGIN Header */
/* ... */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------*/
#include "main.h"

/* Private includes ------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef ---------------------------------------------------------*/
/* Private define ------------------------------------------------------*/
/* Private macro -------------------------------------------------------*/

/* Private variables ---------------------------------------------------*/
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;
/* ... các handle của peripheral khác */

/* USER CODE BEGIN PV */
// Biến toàn cục của bạn ở đây
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------*/
void SystemClock_Config (void);
static void MX_GPIO_Init (void);
static void MX_USART1_UART_Init (void);
static void MX_TIM2_Init (void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ----------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Các hàm tự viết (callback, xử lý, ...) đặt ở đây
/* USER CODE END 0 */

int main (void) {
    /* MCU Configuration--------------------------------------------------*/

    /* Reset các ngoại vi, khởi tạo Flash & hệ thống ngắt */
    HAL_Init ();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* Cấu hình xung clock hệ thống */
    SystemClock_Config ();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    /* Khởi tạo tất cả ngoại vi đã cấu hình */
    MX_GPIO_Init ();
    MX_USART1_UART_Init ();
    MX_TIM2_Init ();

    /* USER CODE BEGIN 2 */
    // Code khởi tạo thêm của bạn: bật timer, gửi log, khởi tạo cảm biến...
    HAL_UART_Transmit (&huart1, (uint8_t*)"Start\r\n", 7, HAL_MAX_DELAY);
    /* USER CODE END 2 */

    /* Vòng lặp chính */
    while (1) {
        /* USER CODE BEGIN WHILE */

        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        /* USER CODE END 3 */
    }
}

/**
 * @brief Cấu hình xung clock hệ thống
 */
void SystemClock_Config (void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    // Cấu hình nguồn dao động (HSE/HSI/PLL...)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    // ...

    HAL_RCC_OscConfig (&RCC_OscInitStruct);
    HAL_RCC_ClockConfig (&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/**
 * @brief Khởi tạo USART1
 */
static void MX_USART1_UART_Init (void) {
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init (&huart1) != HAL_OK) {
        Error_Handler ();
    }
}

/**
 * @brief Khởi tạo các chân GPIO
 */
static void MX_GPIO_Init (void) {
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOA_CLK_ENABLE ();
    __HAL_RCC_GPIOC_CLK_ENABLE ();

    // Cấu hình từng chân theo yêu cầu (LED, nút nhấn...)
}

/**
 * @brief Xử lý lỗi
 */
void Error_Handler (void) {
    __disable_irq ();
    while (1) {
        // Có thể nhấp nháy LED báo lỗi ở đây
    }
}