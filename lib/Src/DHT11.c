/*
 * DHT11 driver for STM32F103.
 * Handles EXTI/TIM callbacks, state transitions, and raw data parsing.
 */

#include "dht11.h"

extern TIM_HandleTypeDef htim2;
volatile DHT11_State_t current_state = DHT11_STATE_IDLE;
DHT11_Data_t dht_data = {0};
volatile uint8_t raw_data[5] = {0};
volatile uint8_t bit_index = 0;
volatile uint8_t byte_index = 0;
DHT11_Config_t g_dht_pin;

/**
 * @brief Khởi tạo các biến, trạng thái ban đầu cho DHT11 và bật Timer 2.
 */
void DHT11_Init(void) {
    // Reset toàn bộ mảng dữ liệu thô và các chỉ số đếm về 0
    for (int i = 0; i < 5; i++) {
        raw_data[i] = 0;
    }
    bit_index = 0;
    byte_index = 0;

    // Đặt trạng thái hệ thống về rảnh rỗi và khởi động Timer
    current_state = DHT11_STATE_IDLE;
    HAL_TIM_Base_Start(&htim2);

    // Cấu hình chân giao tiếp DHT11 thành Input mặc định ban đầu
    DHT11_SetInput(&g_dht_pin);
}

/**
 * @brief Cấu hình chân GPIO của DHT11 thành ngõ vào (Input) kích hoạt ngắt EXTI sườn xuống.
 */
void DHT11_SetInput(DHT11_Config_t *dht) {
    // Khai báo và thiết lập chế độ Input Pull-up với ngắt EXTI sườn xuống
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = dht->Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(dht->Port, &GPIO_InitStruct);

    // Xóa cờ ngắt cũ và cho phép ngắt EXTI trên hệ thống NVIC
    __HAL_GPIO_EXTI_CLEAR_IT(dht->Pin);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

/**
 * @brief Cấu hình chân GPIO của DHT11 thành ngõ ra (Output) Open-Drain để MCU kéo bus.
 */
void DHT11_SetOutput(DHT11_Config_t *dht) {
    // Tắt ngắt EXTI để tránh việc MCU tự kích hoạt ngắt khi đổi mức logic
    HAL_NVIC_DisableIRQ(EXTI2_IRQn);

    // Khai báo và thiết lập chế độ Output Open-Drain, tốc độ cao
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = dht->Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(dht->Port, &GPIO_InitStruct);
}

/**
 * @brief Tạo khoảng trễ chặn (blocking delay) tính bằng micro-giây sử dụng Timer 2.
 */
void delay_us(uint16_t us) {
    // Lấy mốc thời gian hiện tại và xoay vòng chờ đến khi đạt đủ số micro-giây
    uint16_t start_time = __HAL_TIM_GET_COUNTER(&htim2);
    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start_time) <= us) {
    }
}

/**
 * @brief Bắt đầu chu kỳ giao tiếp bằng cách kéo bus xuống LOW tối thiểu 18ms.
 */
void DHT11_StartRequest(DHT11_Config_t *dht) {
    // Chỉ thực hiện yêu cầu mới nếu hệ thống đang rảnh hoặc đã hoàn thành/lỗi chu kỳ trước
    if (current_state != DHT11_STATE_IDLE &&
        current_state != DHT11_STATE_COMPLETE &&
        current_state != DHT11_STATE_ERROR) {
        return;
    }

    // Lưu cấu hình phần cứng và xóa bộ nhớ đệm
    g_dht_pin.Pin = dht->Pin;
    g_dht_pin.Port = dht->Port;
    bit_index = 0;
    byte_index = 0;
    for (int i = 0; i < 5; i++) {
        raw_data[i] = 0;
    }

    // Chuyển chân sang Output và kéo LOW để phát lệnh Start
    DHT11_SetOutput(dht);
    HAL_GPIO_WritePin(dht->Port, dht->Pin, GPIO_PIN_RESET);
    current_state = DHT11_STATE_START_LOW;

    // Cài đặt Timer 2 đếm 18000us (18ms) để ngắt khi đủ thời gian kéo LOW
    HAL_TIM_Base_Stop_IT(&htim2);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim2, 18000);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim2);
}

/**
 * @brief Bộ lọc tín hiệu sơ bộ kiểm tra tính hợp lệ của độ rộng xung (Tuỳ chọn sử dụng).
 */
bool DHT11_InputFilter(uint16_t pulse_width_us) {
    // Loại bỏ các xung quá ngắn (nhiễu) hoặc quá dài (lỗi tín hiệu)
    if (pulse_width_us < 10) {
        return false;
    }
    if (pulse_width_us > 100) {
        return false;
    }
    return true;
}

/**
 * @brief Xử lý ngắt Timer 2 để chuyển đổi trạng thái FSM và làm Watchdog giám sát Timeout.
 */
void DHT11_CallbackTIM2(void) {
    switch (current_state) {
        case DHT11_STATE_START_LOW:
            // Đã đếm đủ 18ms kéo LOW, chuyển chân thành Input để nhường bus cho DHT11
            HAL_TIM_Base_Stop_IT(&htim2);
            DHT11_SetInput(&g_dht_pin);
            current_state = DHT11_STATE_START_HIGH;

            // Chuyển Timer thành Watchdog giám sát (500us). Quá thời gian này sẽ báo lỗi Timeout.
            __HAL_TIM_SET_COUNTER(&htim2, 0);
            __HAL_TIM_SET_AUTORELOAD(&htim2, 500);
            __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
            HAL_TIM_Base_Start_IT(&htim2);
            break;
            
        case DHT11_STATE_START_HIGH:
        case DHT11_STATE_READING:
            // Ngắt Timer xảy ra khi đang chờ phản hồi hoặc đang đọc dở dang (bị đứt kết nối) -> Báo lỗi
            HAL_TIM_Base_Stop_IT(&htim2);
            current_state = DHT11_STATE_ERROR;
            break;
            
        default:
            break;
    }
}

/**
 * @brief Xử lý ngắt ngoài EXTI để đo khoảng thời gian giữa các sườn xuống và giải mã bit.
 */
void DHT11_CallbackEXTI(uint16_t GPIO_Pin) {
    // Lấy khoảng thời gian từ nhịp sườn xuống trước đó và lập tức reset Timer
    uint16_t time_elapsed = __HAL_TIM_GET_COUNTER(&htim2);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    
    switch (current_state) {
        case DHT11_STATE_START_HIGH:
            // Sườn xuống đầu tiên là tín hiệu DHT11 báo sẵn sàng truyền dữ liệu
            current_state = DHT11_STATE_READING;
            bit_index = 0;
            byte_index = 0;
            break;
            
        case DHT11_STATE_READING:
            // Dựa vào độ rộng xung để xác định đây là nhịp mồi, bit 0 hay bit 1
            if (140 <= time_elapsed && time_elapsed <= 180) {
                // Nhịp mồi (ACK), không tiến hành ghi dữ liệu
            } else if (60 <= time_elapsed && time_elapsed <= 100) {
                // Xung ứng với bit 0
                raw_data[byte_index] &= ~(1 << (7 - bit_index));
                bit_index++;
            } else if (100 < time_elapsed && time_elapsed <= 140) {
                // Xung ứng với bit 1
                raw_data[byte_index] |= (1 << (7 - bit_index));
                bit_index++;
            } else {
                // Xung có độ dài phi lý -> Báo lỗi và dừng Watchdog
                current_state = DHT11_STATE_ERROR;
                HAL_TIM_Base_Stop_IT(&htim2);
                break;
            }
            
            // Đóng gói từng bit vào byte, chuyển byte mới khi đủ 8 bit
            if (bit_index >= 8) {
                bit_index = 0;
                byte_index++;
            }
            
            // Hoàn thành đọc toàn bộ 40 bit, dừng Watchdog và báo FSM hoàn tất
            if (byte_index >= 5) {
                current_state = DHT11_STATE_COMPLETE;
                HAL_TIM_Base_Stop_IT(&htim2);
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief Phân tích 40 bit thô, kiểm tra tính toàn vẹn Checksum và gán số liệu.
 */
void DHT11_ParseData(void) {
    // Cộng 4 byte đầu tiên để tính tổng kiểm tra
    uint8_t sum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];

    // Đối chiếu tổng với byte thứ 5 (Checksum) để quyết định dữ liệu có hợp lệ hay không
    if (sum == raw_data[4]) {
        dht_data.humidity_int = raw_data[0];
        dht_data.humidity_dec = raw_data[1];
        dht_data.temp_int = raw_data[2];
        dht_data.temp_dec = raw_data[3];
        dht_data.checksum = raw_data[4];
        dht_data.is_valid = true;
    } else {
        dht_data.is_valid = false;
    }
}

/**
 * @brief Đọc kết quả từ máy trạng thái, hàm này được gọi liên tục trong main.
 */
DHT11_State_t DHT11_ReadData(DHT11_Data_t *data) {
    DHT11_State_t state_to_return = current_state;

    if (current_state == DHT11_STATE_COMPLETE) {
        // Giải mã 40 bit và chép dữ liệu hoàn chỉnh sang con trỏ cấu trúc của người dùng
        DHT11_ParseData();
        data->humidity_int = dht_data.humidity_int;
        data->humidity_dec = dht_data.humidity_dec;
        data->temp_int = dht_data.temp_int;
        data->temp_dec = dht_data.temp_dec;
        data->checksum = dht_data.checksum;
        data->is_valid = dht_data.is_valid;
        
        // Reset hệ thống về trạng thái sẵn sàng cho chu kỳ tới
        current_state = DHT11_STATE_IDLE;
        
    } else if (current_state == DHT11_STATE_ERROR) {
        // Hệ thống gặp lỗi trong quá trình đọc, reset trạng thái và báo dữ liệu không hợp lệ
        current_state = DHT11_STATE_IDLE;
        data->is_valid = false;
    }
    
    return state_to_return;
}