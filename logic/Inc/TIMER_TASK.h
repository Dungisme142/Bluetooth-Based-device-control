#ifndef TIMER_TASK_H
#define TIMER_TASK_H

#include <stdint.h>

/* Khởi tạo timer và cấu hình ban đầu cho hệ thống định thời */
void Timer_Task_Init(void);

/* Tăng biến đếm timer mỗi chu kỳ tick */
void Timer_Task_Increment(void);

/* Trả về cờ báo cho việc đọc cảm biến */
uint8_t Timer_Task_GetSensorFlag(void);

/* Trả về cờ báo cho việc truyền dữ liệu */
uint8_t Timer_Task_GetTxFlag(void);

/* Xóa các cờ báo sau khi đã xử lý */
void Timer_Task_ClearFlags(void);

#endif
