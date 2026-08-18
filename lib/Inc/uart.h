/*
 * uart.h — Cụm nhận lệnh qua UART: lọc ký tự, dựng khung, tra bảng lệnh.
 *
 * Toàn bộ đường đi của một byte nằm trong uart.c đọc từ trên xuống:
 *
 *      ISR USART1 -> Ring_Buffer -> Text_Filting() -> Frame_Building()
 *                 -> Command_Selecting() -> UART_Print()
 *
 * Ring_Buffer giữ riêng vì là cấu trúc dữ liệu tổng quát; phần lọc ký tự và
 * dựng khung trước đây nằm ở Text_Filter/Frame_Builder/Framing_Base nhưng chỉ
 * có uart.c dùng, nên đã gộp vào đây làm chi tiết nội bộ.
 */
#ifndef UART_H_
#define UART_H_

#include "Global_Enum.h"
#include "Ring_Buffer.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* Đường truyền im lặng bao lâu thì coi như bản tin đã hết, dù app không gửi
 * ký tự xuống dòng nào. Nhiều app terminal Bluetooth để mặc định "no line
 * ending" nên không có mốc này thì lệnh không bao giờ được chốt.
 *
 * 250 ms là rất rộng so với 9600 baud (1 byte ~ 1 ms), nhưng nếu dùng app gửi
 * ngay từng phím vừa gõ thì nên đặt app về CR/LF thay vì nới rộng số này. */
#define UART_FRAME_IDLE_TIMEOUT_MS 250u

/* Im lặng quá lâu thì coi như module Bluetooth đã rời kết nối. */
#define UART_LINK_TIMEOUT_MS       10000u

/* Bộ đệm phát của mỗi handle. Nằm trong handle nên là bộ nhớ tĩnh, không
 * chiếm stack; phải đủ chứa câu trả lời dài nhất của bảng lệnh. */
#define UART_TX_BUFFER_SIZE        256u

typedef enum {
    DEVELOPER_UART_DISCONNECTED = 0,   /* Trạng thái lúc khởi tạo */
    DEVELOPER_UART_CONNECTED
} Developer_UART_Connecting_Status_t;

/* Khung lệnh đang dựng dở. Là chi tiết nội bộ của uart.c — khai ở đây chỉ vì
 * Developer_UART_HandleTypeDef chứa nó theo giá trị. Tầng ứng dụng không cần
 * chạm vào trường nào của struct này. */
typedef struct {
    void    *framing_result_ptr;             /* Bộ đệm khung do app cấp */
    uint8_t  framing_result_data_size_byte;  /* Kích thước một phần tử */
    uint8_t  max_frame_size;                 /* Số phần tử tối đa của bộ đệm */
    uint8_t  framing_result_index;           /* Vị trí ghi kế tiếp */
    bool     framing_result_ready_to_read;   /* Khung đã chốt, chờ xử lý */
} Framing_Result_HandleTypeDef;

typedef struct {
    UART_HandleTypeDef        *hal_huart;
    Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr;
    Framing_Result_HandleTypeDef framing_result;
    char                       tx_buffer[UART_TX_BUFFER_SIZE];
    uint32_t                   last_received_tick;
    Developer_UART_Connecting_Status_t uart_connecting_status;
} Developer_UART_HandleTypeDef;

/**
 * @brief  Khởi tạo handle UART kèm luôn bộ dựng khung của nó.
 *
 * Frame_Builder trước đây phải khởi tạo riêng ở tầng app; nay gộp vào đây để
 * app không còn phải biết tới cụm framing.
 *
 * @param  developer_uart_handler  Handle cần khởi tạo.
 * @param  hal_huart               Ngoại vi UART của HAL.
 * @param  ring_buffer_handler_ptr Bộ đệm vòng mà ISR đẩy byte nhận được vào.
 * @param  frame_buffer            Bộ đệm chứa khung lệnh đang dựng.
 * @param  frame_data_size_byte    Kích thước một phần tử của frame_buffer.
 * @param  max_frame_size          Số phần tử của frame_buffer.
 * @retval DEV_SUCCESS nếu mọi con trỏ hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t Developer_UART_Handler_Init(
    Developer_UART_HandleTypeDef *developer_uart_handler,
    UART_HandleTypeDef *hal_huart,
    Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr,
    void *frame_buffer,
    uint8_t frame_data_size_byte,
    uint8_t max_frame_size);

/**
 * @brief  Gửi một chuỗi định dạng ra UART (không chặn, dùng Transmit_IT).
 *
 * Hàm này là printf: `send_str` là CHUỖI ĐỊNH DẠNG, không phải dữ liệu. Truyền
 * một chuỗi chạy thẳng vào đây thì mọi '%' trong đó sẽ bị đọc là đặc tả định
 * dạng. Dùng UART_Print(h, "%s", chuoi) cho dữ liệu chạy.
 *
 * __attribute__((format)) cho GCC kiểm tra điều đó ngay lúc biên dịch — giữ
 * nguyên attribute này khi sửa chữ ký hàm.
 */
void UART_Print(Developer_UART_HandleTypeDef *developer_uart_handler,
                const char *send_str, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * @brief  Rút hết byte trong bộ đệm vòng, dựng khung và thực thi lệnh đã chốt.
 *
 * Gọi mỗi vòng lặp chính. Không chặn: mỗi lượt xử lý nhiều nhất một lệnh rồi
 * trả quyền về cho vòng lặp.
 */
void UART_Task(Developer_UART_HandleTypeDef *developer_uart_handler);

#endif /* UART_H_ */
