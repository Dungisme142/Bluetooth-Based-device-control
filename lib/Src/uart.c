#include "uart.h"
#include "Command_Selector.h"
#include "Frame_Builder.h"
#include "Global_Enum.h"
#include "Ring_Buffer.h"
#include "Text_Filter.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

Developer_Action_Result_t Developer_UART_Handler_Init(Developer_UART_HandleTypeDef *developer_uart_handler, UART_HandleTypeDef *hal_huart, Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr, Framing_Result_HandleTypeDef *framing_result_handler_ptr){
    if((developer_uart_handler == NULL) || (hal_huart == NULL) || (ring_buffer_handler_ptr == NULL) || (framing_result_handler_ptr == NULL)){
        return DEV_FAIL;
    }
    developer_uart_handler->hal_huart = hal_huart;
    developer_uart_handler->ring_buffer_handler_ptr = ring_buffer_handler_ptr;
    developer_uart_handler->framing_result_handler_ptr = framing_result_handler_ptr;
    developer_uart_handler->last_received_tick = 0;
    developer_uart_handler->uart_connecting_status = developer_uart_disconnected;
    return DEV_SUCCESS;
}


void UART_Print(Developer_UART_HandleTypeDef *developer_uart_handler, const char* send_str,...){
    if(developer_uart_handler->hal_huart->gState != HAL_UART_STATE_READY){
        return;
    }
    va_list args;
    va_start(args, send_str);
    vsnprintf(developer_uart_handler->tx_buffer, sizeof(developer_uart_handler->tx_buffer), send_str, args);
    va_end(args);
    HAL_UART_Transmit_IT(developer_uart_handler->hal_huart, (uint8_t*)developer_uart_handler->tx_buffer, strlen(developer_uart_handler->tx_buffer));
}

void UART_Connecting_Status_Check(Developer_UART_HandleTypeDef *developer_uart_handler){
    uint32_t current_tick = HAL_GetTick();

    if(current_tick - developer_uart_handler->last_received_tick >= 10000 && developer_uart_handler->uart_connecting_status == developer_uart_connected){
        developer_uart_handler->uart_connecting_status = developer_uart_disconnected;
        UART_Print(developer_uart_handler, "Disconnected\r\n");
    }
    
}

extern uint8_t Command_Menu_Size;
extern Command_HandleTypeDef Command_Menu[];

void UART_Task(Developer_UART_HandleTypeDef *developer_uart_handler){
    if(developer_uart_handler == NULL){
        return;
    }
    UART_Connecting_Status_Check(developer_uart_handler);
    
    uint8_t current_processed_data;
    if(Ring_Buffer_Read_SingleData(developer_uart_handler->ring_buffer_handler_ptr, &current_processed_data) == DEV_SUCCESS){
        if(Frame_Building(developer_uart_handler->framing_result_handler_ptr, Text_Filting(&current_processed_data), &current_processed_data) == DEV_SUCCESS){
            if(developer_uart_handler->framing_result_handler_ptr->framing_result_ready_to_read == DEV_TRUE){
                char return_msg[256];
                Command_Selecting(Command_Menu, Command_Menu_Size, return_msg, (char*)developer_uart_handler->framing_result_handler_ptr->framing_result_ptr);
                UART_Print(developer_uart_handler, return_msg);
                //reset
                developer_uart_handler->framing_result_handler_ptr->framing_result_ready_to_read = DEV_FALSE;
                developer_uart_handler->framing_result_handler_ptr->framing_result_index = 0;
            }
        }
        else{
            UART_Print(developer_uart_handler, "Fail, try again!\r\n");
            //reset
            developer_uart_handler->framing_result_handler_ptr->framing_result_ready_to_read = DEV_FALSE;
            developer_uart_handler->framing_result_handler_ptr->framing_result_index = 0;
        }
    }
}