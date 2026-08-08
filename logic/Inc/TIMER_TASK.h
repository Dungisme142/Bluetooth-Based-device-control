#ifndef TIMER_TASK_H
#define TIMER_TASK_H

#include <stdint.h>

/* Chu kỳ đọc cảm biến: 2000 ms */
#define TIMER_TASK_SENSOR_PERIOD_MS 2000u

/* Chu kỳ gửi dữ liệu Bluetooth: 3000 ms */
#define TIMER_TASK_TX_PERIOD_MS 3000u

/* Khởi tạo biến đếm và các cờ thời gian */
void Timer_Task_Init(void);

/* Được gọi mỗi 1 ms từ ngắt TIM2 */
void Timer_Task_Increment(void);

/* Kiểm tra cờ yêu cầu đọc cảm biến */
uint8_t Timer_Task_GetSensorFlag(void);

/* Kiểm tra cờ yêu cầu truyền dữ liệu */
uint8_t Timer_Task_GetTxFlag(void);

/* Xóa riêng từng cờ sau khi xử lý */
void Timer_Task_ClearSensorFlag(void);
void Timer_Task_ClearTxFlag(void);

/* Xóa đồng thời cả hai cờ */
void Timer_Task_ClearFlags(void);

#endif