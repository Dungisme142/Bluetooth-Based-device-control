#ifndef DHT11_H
#define DHT11_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

// Các trạng thái của máy trạng thái (FSM) DHT11
typedef enum {
    DHT11_STATE_IDLE = 0,
    DHT11_STATE_START_LOW,
    DHT11_STATE_START_HIGH,
    DHT11_STATE_READING,
    DHT11_STATE_COMPLETE,
    DHT11_STATE_ERROR
} DHT11_State_t;

// Cấu trúc lưu trữ dữ liệu
typedef struct {
    uint8_t humidity_int;
    uint8_t humidity_dec;
    uint8_t temp_int;
    uint8_t temp_dec;
    uint8_t checksum;
    bool is_valid;
} DHT11_Data_t;

typedef struct{
    GPIO_TypeDef* Port;
    uint16_t Pin;
}   DHT11_Config_t;
/* KHOẢNG THỜI GIAN THEO DATASHEET (đơn vị: micro-giây) */
#define DHT11_START_LOW_TIME_US  18000 // Tối thiểu 18ms
#define DHT11_TIMEOUT_US         100   // Quá 100us không phản hồi -> Timeout

/*---------------- HÀM KHỞI TẠO VÀ CẤU HÌNH ----------------*/

/**
 * @brief  Khởi tạo các tham số mặc định cho DHT11
 * @param  None
 * @retval None
 */
void DHT11_Init(void);

void DHT11_SetInput(DHT11_Config_t *dht);
void DHT11_SetOutput(DHT11_Config_t *dht);

/*---------------- HÀM ĐIỀU KHIỂN & ĐỌC DỮ LIỆU ----------------*/

/**
 * @brief  Gửi tín hiệu Start kéo chân bus xuống Low, khởi động FSM
 */
void DHT11_StartRequest(DHT11_Config_t *dht);
DHT11_State_t DHT11_ReadData(DHT11_Data_t *data);

/*---------------- HÀM TIỆN ÍCH & NGẮT (INTERRUPT) ----------------*/

/**
 * @brief  Hàm tạo trễ bằng Timer (blocking ngắn) hoặc dùng để check time
 * @param  us: Số micro-giây cần delay
 */
void delay_us(uint16_t us);

/**
 * @brief  Xử lý ngắt TIM2 (Timeout hoặc đếm thời gian cho FSM)
 */
void DHT11_CallbackTIM2(void);
bool DHT11_InputFilter(uint16_t pulse_width_us);
void DHT11_ParseData(void);
void DHT11_CallbackEXTI(uint16_t GPIO_Pin);
#endif /* DHT11_H */