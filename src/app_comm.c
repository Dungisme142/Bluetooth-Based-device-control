/*
 * app_comm.c — Kênh Bluetooth (USART1) của ứng dụng.
 */

#include "app_comm.h"

#include "app_state.h"
#include "board.h"
#include "Ring_Buffer.h"
#include "uart.h"

#include <stddef.h>

#define UART_RX_BUFFER_SIZE     128u
#define UART_FRAME_BUFFER_SIZE  128u

/* Đủ rộng cho chuỗi trạng thái dài nhất (~45 ký tự) kèm CRLF. */
#define APP_STATUS_TEXT_SIZE     64u

/* Buffer cấp phát tĩnh, kích thước bằng macro — không có malloc sau init. */
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t uart_frame_buffer[UART_FRAME_BUFFER_SIZE];

/* Ô nhận một byte mà HAL_UART_Receive_IT() ghi vào. ISR đọc rồi đẩy ngay sang
 * bộ đệm vòng, nên không có ai khác chạm vào nó cùng lúc. */
static uint8_t uart_rx_byte;

static Ring_Buffer_HandleTypeDef    ring_buffer_handler;
static Developer_UART_HandleTypeDef developer_uart_handler;

Developer_Action_Result_t App_Comm_Init(void)
{
    if (Ring_Buffer_Init(&ring_buffer_handler, uart_rx_buffer, UART_RX_BUFFER_SIZE,
                         sizeof(uint8_t)) != DEV_SUCCESS) {
        return DEV_FAIL;
    }

    if (Developer_UART_Handler_Init(&developer_uart_handler, &huart1, &ring_buffer_handler,
                                    uart_frame_buffer, sizeof(uint8_t),
                                    UART_FRAME_BUFFER_SIZE) != DEV_SUCCESS) {
        return DEV_FAIL;
    }

    if (HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u) != HAL_OK) {
        return DEV_FAIL;
    }

    UART_Print(&developer_uart_handler, "HC-05 ready\r\n");
    return DEV_SUCCESS;
}

void App_Comm_Task(void)
{
    UART_Task(&developer_uart_handler);
}

void App_Comm_SendStatus(void)
{
    char status[APP_STATUS_TEXT_SIZE];

    (void)App_State_FormatStatus(status, sizeof(status), APP_STATUS_FIELDS_ALL);

    /* "%s" là bắt buộc: status là dữ liệu chạy chứ không phải chuỗi định dạng.
     * Nó chứa dấu '%' (từ "HUM=61%"), đưa thẳng vào vị trí format thì vsnprintf
     * bên trong UART_Print() sẽ đọc nó là đặc tả định dạng và lấy đối số không
     * tồn tại. */
    UART_Print(&developer_uart_handler, "%s", status);
}

void App_Comm_SendText(const char *text)
{
    if (text == NULL) {
        return;
    }
    UART_Print(&developer_uart_handler, "%s", text);
}

void App_Comm_OnRxComplete(void)
{
    if (Ring_Buffer_Write_SingleData(&ring_buffer_handler, &uart_rx_byte) == DEV_SUCCESS) {
        system_state.bluetooth_connected = true;
        developer_uart_handler.last_received_tick = HAL_GetTick();
        developer_uart_handler.uart_connecting_status = DEVELOPER_UART_CONNECTED;
    }

    /* Mở lại phiên nhận kể cả khi bộ đệm đầy: bỏ một byte còn hơn ngừng nhận. */
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u);
}

void App_Comm_OnRxError(void)
{
    /* Đọc SR rồi DR là trình tự xoá cờ ORE trên F1; HAL_UART_IRQHandler đã đọc
     * SR, đọc nốt DR để chắc chắn cờ được xoá và bỏ byte hỏng đi. */
    (void)huart1.Instance->DR;

    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u);
}
