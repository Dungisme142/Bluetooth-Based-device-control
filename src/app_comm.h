/*
 * app_comm.h — Đường Bluetooth của ứng dụng.
 *
 * Sở hữu toàn bộ bộ đệm và handle của kênh UART1, để không module nào khác
 * phải biết tới chúng. Các HAL callback trong main.c chỉ gọi hai hàm ISR ở
 * cuối file này.
 */
#ifndef APP_COMM_H
#define APP_COMM_H

#include "Global_Enum.h"

/**
 * @brief  Khởi tạo bộ đệm vòng, handle UART và mở phiên nhận đầu tiên.
 * @retval DEV_SUCCESS nếu mọi thành phần khởi tạo được, DEV_FAIL nếu không.
 */
Developer_Action_Result_t App_Comm_Init(void);

/**
 * @brief  Xử lý byte đã nhận và thực thi lệnh đã chốt. Gọi mỗi vòng lặp chính.
 */
void App_Comm_Task(void);

/**
 * @brief  Gửi chuỗi báo trạng thái đầy đủ về điện thoại.
 */
void App_Comm_SendStatus(void);

/**
 * @brief  Gửi một chuỗi dữ liệu chạy (không phải chuỗi định dạng) ra UART.
 */
void App_Comm_SendText(const char *text);

/*---------------- Gọi từ ngữ cảnh ngắt ----------------*/

/**
 * @brief  Đẩy byte vừa nhận vào bộ đệm vòng rồi mở lại phiên nhận.
 */
void App_Comm_OnRxComplete(void);

/**
 * @brief  Khởi động lại việc nhận sau khi USART1 gặp lỗi (tràn ORE...).
 */
void App_Comm_OnRxError(void);

#endif /* APP_COMM_H */
