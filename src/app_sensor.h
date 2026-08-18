/*
 * app_sensor.h — Tầng ứng dụng của cảm biến DHT11.
 *
 * Giữ cấu hình chân của cảm biến và biến máy trạng thái không chặn của driver
 * thành một phép đo gọn cho vòng lặp chính.
 */
#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include "Global_Enum.h"

/**
 * @brief  Khởi tạo driver DHT11 với chân trong pin_config.h và bộ đếm TIM2.
 * @retval DEV_SUCCESS nếu driver nhận cấu hình, DEV_FAIL nếu không.
 */
Developer_Action_Result_t App_Sensor_Init(void);

/**
 * @brief  Chạy một phép đo DHT11 từ đầu đến cuối và cập nhật system_state.
 *
 * CHẶN tới ~500 ms — đây là tác vụ nặng nhất của superloop. Ở 9600 baud nghĩa
 * là tới 500 byte có thể đổ về trong lúc main loop không rút được; bộ đệm vòng
 * 128 byte và vòng while rút liên tục trong UART_Task() là để bù cho việc này.
 * ĐỪNG thêm tác vụ chặn thứ ba vào vòng lặp chính.
 *
 * @retval DEV_SUCCESS nếu đọc được giá trị hợp lệ, DEV_FAIL nếu lỗi/quá hạn.
 */
Developer_Action_Result_t App_Sensor_ReadOnce(void);

#endif /* APP_SENSOR_H */
