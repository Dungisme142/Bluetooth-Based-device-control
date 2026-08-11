#ifndef UART_H_
#define UART_H_

#include "Framing_Base.h"
#include "Global_Enum.h"
#include "Ring_Buffer.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum{
    developer_uart_connected,
    developer_uart_disconnected
}Developer_UART_Connecting_Status_t;

typedef struct{
    UART_HandleTypeDef *hal_huart;
    Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr;
    Framing_Result_HandleTypeDef *framing_result_handler_ptr;
    uint8_t rx_data; //before ring_buffer
    char tx_buffer[256];
    uint32_t last_received_tick;
    Developer_UART_Connecting_Status_t uart_connecting_status;
}Developer_UART_HandleTypeDef;

Developer_Action_Result_t Developer_UART_Handler_Init(Developer_UART_HandleTypeDef *developer_uart_handler, UART_HandleTypeDef *hal_huart, Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr, Framing_Result_HandleTypeDef *framing_result_handler_ptr);
void UART_Print(Developer_UART_HandleTypeDef *developer_uart_handler, const char* send_str,...);
void UART_Connecting_Status_Check(Developer_UART_HandleTypeDef *developer_uart_handler);
void UART_Task(Developer_UART_HandleTypeDef *developer_uart_handler);

#endif