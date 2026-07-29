#ifndef PROTOCOL_TASK_H
#define PROTOCOL_TASK_H

#include <stddef.h>

/* Nhận lệnh từ Bluetooth/UART, phân tích và tạo phản hồi cho hệ thống */
void Protocol_Task_HandleCommand(const char *command, char *response, size_t response_size);

#endif
