#ifndef DHT11_H
#define DHT11_H

#include "Global_Enum.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* Các trạng thái của máy trạng thái (FSM) DHT11 */
typedef enum {
    DHT11_STATE_IDLE = 0,
    DHT11_STATE_START_LOW,
    DHT11_STATE_START_HIGH,
    DHT11_STATE_READING,
    DHT11_STATE_COMPLETE,
    DHT11_STATE_ERROR
} DHT11_State_t;

/* Cấu trúc lưu trữ dữ liệu */
typedef struct {
    uint8_t humidity_int;
    uint8_t humidity_dec;
    uint8_t temp_int;
    uint8_t temp_dec;
    uint8_t checksum;
    bool    is_valid;
} DHT11_Data_t;

typedef struct {
    GPIO_TypeDef      *Port;
    uint16_t           Pin;
    IRQn_Type          IRQn;   /* Đường ngắt EXTI tương ứng với Pin (vd PB1 -> EXTI1_IRQn) */
    /* Bộ đếm micro-giây mà driver dùng để đo độ rộng xung và làm watchdog.
     * Truyền vào từ tầng ứng dụng thay vì `extern htim2`: driver không được
     * biết tên biến của board.c, nếu không thì bê sang project khác là gãy. */
    TIM_HandleTypeDef *htim;
} DHT11_Config_t;

/* KHOẢNG THỜI GIAN THEO DATASHEET (đơn vị: micro-giây) */
#define DHT11_START_LOW_TIME_US  18000u  /* Tối thiểu 18 ms */
#define DHT11_TIMEOUT_US         100u    /* Quá 100 us không phản hồi -> Timeout */

/*---------------- HÀM KHỞI TẠO VÀ CẤU HÌNH ----------------*/

/**
 * @brief  Khởi tạo các tham số mặc định cho DHT11 và khởi động bộ đếm.
 * @param  dht: cấu hình chân (Port/Pin/IRQn) lấy từ pin_config.h và handle timer.
 * @retval DEV_SUCCESS nếu cấu hình hợp lệ, DEV_FAIL nếu con trỏ NULL.
 */
Developer_Action_Result_t DHT11_Init(const DHT11_Config_t *dht);

/*---------------- HÀM ĐIỀU KHIỂN & ĐỌC DỮ LIỆU ----------------*/

/**
 * @brief  Gửi tín hiệu Start kéo chân bus xuống Low, khởi động FSM.
 */
void DHT11_StartRequest(void);

/**
 * @brief  Đọc kết quả của FSM; gọi lặp lại từ vòng lặp chính.
 * @param  data: nơi nhận số liệu khi FSM báo COMPLETE.
 * @retval Trạng thái FSM tại thời điểm gọi.
 */
DHT11_State_t DHT11_ReadData(DHT11_Data_t *data);

/*---------------- HÀM NGẮT (INTERRUPT) ----------------*/

/**
 * @brief  Xử lý ngắt TIM2 (Timeout hoặc đếm thời gian cho FSM).
 */
void DHT11_CallbackTIM2(void);

/**
 * @brief  Xử lý ngắt EXTI trên chân bus: đo độ rộng xung và giải mã bit.
 */
void DHT11_CallbackEXTI(uint16_t exti_pin);

#endif /* DHT11_H */
