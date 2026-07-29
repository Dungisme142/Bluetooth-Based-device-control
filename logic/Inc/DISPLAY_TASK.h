#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <stdint.h>

/* Cập nhật dữ liệu nhiệt độ, độ ẩm và trạng thái thiết bị lên OLED */
void Display_Task_Update(float temperature, float humidity, uint8_t relay_state, uint8_t bluetooth_connected);

/* Làm mới màn hình để hiển thị trạng thái mới nhất */
void Display_Task_Refresh(void);

#endif
