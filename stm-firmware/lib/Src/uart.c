/*
 * uart.c — Nhận lệnh qua UART: lọc ký tự -> dựng khung -> tra bảng lệnh.
 *
 * Đọc từ trên xuống là đúng thứ tự một byte đi qua:
 *
 *      Text_Filting()   phân loại ký tự vừa nhận (giữ / xoá / bỏ / kết thúc)
 *      Frame_Building() dồn ký tự vào bộ đệm khung, chốt khung khi hết câu
 *      UART_Task()      rút bộ đệm vòng, gọi hai hàm trên, phát lệnh đi
 */

#include "uart.h"

#include "command_selector.h"
#include "global_enum.h"
#include "ring_buffer.h"
#include "stm32f1xx_hal.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*==================== Lọc ký tự ====================*/

/* Ký tự vừa nhận thuộc loại nào. Nội bộ uart.c — tầng app không thấy. */
typedef enum {
    FILTING_RESULT_SAVE_AND_CONTINUE = 0,
    FILTING_RESULT_BACKSPACE,
    FILTING_RESULT_IGNORE,
    FILTING_RESULT_FINISH,
    FILTING_RESULT_ERROR
} Filting_Result_t;

#define UART_ASCII_BELL 0x07u

static Filting_Result_t Text_Filting(const uint8_t *current_processed_data)
{
    if (current_processed_data == NULL) {
        return FILTING_RESULT_ERROR;
    }

    /* Nhận cả CR lẫn LF làm dấu kết thúc lệnh. Mỗi app terminal Bluetooth gửi
     * một kiểu khác nhau: có app CR, có app LF, có app cả cặp CRLF. Với CRLF,
     * ký tự thứ hai sinh ra một khung RỖNG — UART_Frame_Dispatch() bỏ qua khung
     * rỗng nên nó không bị hiểu nhầm là lệnh sai. */
    if ((*current_processed_data == '\r') || (*current_processed_data == '\n')) {
        return FILTING_RESULT_FINISH;
    }
    if (*current_processed_data == '\b') {
        return FILTING_RESULT_BACKSPACE;
    }
    if (*current_processed_data == UART_ASCII_BELL) {
        return FILTING_RESULT_IGNORE;
    }
    return FILTING_RESULT_SAVE_AND_CONTINUE;
}

/*==================== Dựng khung ====================*/

/**
 * @brief Con trỏ tới ô thứ `index` của bộ đệm khung.
 *
 * Tách riêng vì cùng biểu thức số học con trỏ này trước đây được viết tay ba
 * lần trong ba nhánh của Frame_Building().
 */
static uint8_t *Frame_Slot_Ptr(const Framing_Result_HandleTypeDef *frame, uint8_t index)
{
    return (uint8_t *)frame->framing_result_ptr +
           (uint32_t)index * frame->framing_result_data_size_byte;
}

/**
 * @brief Chốt khung: ghi ký tự kết thúc rồi báo cho UART_Task() đến lấy.
 */
static void Frame_Commit(Framing_Result_HandleTypeDef *frame)
{
    memset(Frame_Slot_Ptr(frame, frame->framing_result_index), '\0',
           frame->framing_result_data_size_byte);
    frame->framing_result_ready_to_read = true;
}

/**
 * @brief Lùi một ký tự, có chặn dưới ở 0 để chỉ số không quay vòng.
 */
static void Frame_Backspace(Framing_Result_HandleTypeDef *frame)
{
    if (frame->framing_result_index > 0u) {
        frame->framing_result_index--;
    }
}

/**
 * @brief Khởi tạo bộ dựng khung.
 */
static Developer_Action_Result_t Frame_Builder_Init(Framing_Result_HandleTypeDef *frame,
                                                    void *framing_result_ptr,
                                                    uint8_t framing_result_data_size_byte,
                                                    uint8_t max_frame_size)
{
    if ((frame == NULL) || (framing_result_ptr == NULL) ||
        (framing_result_data_size_byte == 0u) || (max_frame_size == 0u)) {
        return DEV_FAIL;
    }

    frame->framing_result_ptr = framing_result_ptr;
    frame->framing_result_data_size_byte = framing_result_data_size_byte;
    frame->max_frame_size = max_frame_size;
    frame->framing_result_index = 0u;
    frame->framing_result_ready_to_read = false;

    return DEV_SUCCESS;
}

/**
 * @brief Đưa một ký tự đã phân loại vào khung đang dựng.
 *
 * Chỉ số chạy trong [0, max_frame_size]. Giá trị max_frame_size là mốc "khung
 * đã tràn": không ghi thêm ký tự nào nữa và cũng không cho chốt khung, vì ô
 * cuối cùng phải để dành cho ký tự kết thúc chuỗi.
 *
 * @retval DEV_FAIL nếu khung tràn mà vẫn đòi chốt, hoặc gặp ký tự lỗi.
 */
static Developer_Action_Result_t Frame_Building(Framing_Result_HandleTypeDef *frame,
                                                Filting_Result_t filting_result,
                                                const void *current_processed_data)
{
    if (frame == NULL) {
        return DEV_FAIL;
    }

    switch (filting_result) {
    case FILTING_RESULT_IGNORE:
        return DEV_SUCCESS;

    case FILTING_RESULT_BACKSPACE:
        Frame_Backspace(frame);
        return DEV_SUCCESS;

    case FILTING_RESULT_FINISH:
        if (frame->framing_result_index >= frame->max_frame_size) {
            return DEV_FAIL;    /* Khung đã tràn, không còn chỗ cho ký tự kết thúc */
        }
        Frame_Commit(frame);
        return DEV_SUCCESS;

    case FILTING_RESULT_SAVE_AND_CONTINUE:
        if ((current_processed_data == NULL) ||
            (frame->framing_result_index >= (uint8_t)(frame->max_frame_size - 1u))) {
            /* Đánh dấu tràn và giữ nguyên ở đó: tăng tiếp thì chỉ số uint8_t sẽ
             * quay vòng qua 255 rồi về 0 và ký tự lại được ghi đè lên đầu khung. */
            frame->framing_result_index = frame->max_frame_size;
            return DEV_SUCCESS;
        }
        memcpy(Frame_Slot_Ptr(frame, frame->framing_result_index),
               current_processed_data, frame->framing_result_data_size_byte);
        frame->framing_result_index++;
        return DEV_SUCCESS;

    case FILTING_RESULT_ERROR:
    default:
        return DEV_FAIL;
    }
}

/*==================== Handle UART ====================*/

Developer_Action_Result_t Developer_UART_Handler_Init(
    Developer_UART_HandleTypeDef *developer_uart_handler,
    UART_HandleTypeDef *hal_huart,
    Ring_Buffer_HandleTypeDef *ring_buffer_handler_ptr,
    void *frame_buffer,
    uint8_t frame_data_size_byte,
    uint8_t max_frame_size)
{
    if ((developer_uart_handler == NULL) || (hal_huart == NULL) ||
        (ring_buffer_handler_ptr == NULL)) {
        return DEV_FAIL;
    }

    if (Frame_Builder_Init(&developer_uart_handler->framing_result, frame_buffer,
                           frame_data_size_byte, max_frame_size) != DEV_SUCCESS) {
        return DEV_FAIL;
    }

    developer_uart_handler->hal_huart = hal_huart;
    developer_uart_handler->ring_buffer_handler_ptr = ring_buffer_handler_ptr;
    developer_uart_handler->tx_buffer[0] = '\0';
    developer_uart_handler->last_received_tick = 0u;
    developer_uart_handler->uart_connecting_status = DEVELOPER_UART_DISCONNECTED;

    return DEV_SUCCESS;
}

void UART_Print(Developer_UART_HandleTypeDef *developer_uart_handler,
                const char *send_str, ...)
{
    va_list args;

    if ((developer_uart_handler == NULL) || (send_str == NULL)) {
        return;
    }

    /* Đang bận phát dở câu trước thì bỏ câu này: ghi đè tx_buffer lúc
     * Transmit_IT còn đang đọc nó sẽ làm câu đang gửi bị lẫn. */
    if (developer_uart_handler->hal_huart->gState != HAL_UART_STATE_READY) {
        return;
    }

    va_start(args, send_str);
    vsnprintf(developer_uart_handler->tx_buffer,
              sizeof(developer_uart_handler->tx_buffer), send_str, args);
    va_end(args);

    (void)HAL_UART_Transmit_IT(developer_uart_handler->hal_huart,
                               (uint8_t *)developer_uart_handler->tx_buffer,
                               (uint16_t)strlen(developer_uart_handler->tx_buffer));
}

/* Im lặng quá lâu thì hạ cờ kết nối. Chỉ uart.c dùng nên để static. */
static void UART_Connecting_Status_Check(Developer_UART_HandleTypeDef *developer_uart_handler)
{
    uint32_t now_ms = HAL_GetTick();

    if ((developer_uart_handler->uart_connecting_status == DEVELOPER_UART_CONNECTED) &&
        ((now_ms - developer_uart_handler->last_received_tick) >= UART_LINK_TIMEOUT_MS)) {
        developer_uart_handler->uart_connecting_status = DEVELOPER_UART_DISCONNECTED;
        UART_Print(developer_uart_handler, "Disconnected\r\n");
    }
}

/* Dọn khung đang dở để sẵn sàng nhận khung kế tiếp. */
static void UART_Frame_Reset(Framing_Result_HandleTypeDef *frame)
{
    frame->framing_result_ready_to_read = false;
    frame->framing_result_index = 0u;
}

/* Trả khung vừa chốt cho bảng lệnh rồi dọn dẹp. */
static void UART_Frame_Dispatch(Developer_UART_HandleTypeDef *developer_uart_handler)
{
    Framing_Result_HandleTypeDef *frame = &developer_uart_handler->framing_result;

    /* Khung rỗng không phải là lệnh — bỏ qua chứ không báo "Invalid Command".
     * Trường hợp hay gặp nhất: app gửi CRLF, ký tự '\r' chốt lệnh thật rồi
     * '\n' còn lại chốt thêm một khung rỗng ngay sau đó. */
    if (frame->framing_result_index > 0u) {
        char return_msg[COMMAND_RETURN_MSG_SIZE];

        return_msg[0] = '\0';
        (void)Command_Selecting(Command_Menu, Command_Menu_Size, return_msg,
                                (const char *)frame->framing_result_ptr);

        /* "%s" là bắt buộc: return_msg là dữ liệu chạy chứ không phải chuỗi
         * định dạng. Đưa thẳng vào vị trí format thì dấu '%' trong những câu
         * như "HUM=61%" sẽ bị vsnprintf hiểu là đặc tả định dạng và đọc đối số
         * không tồn tại. */
        UART_Print(developer_uart_handler, "%s", return_msg);
    }

    UART_Frame_Reset(frame);
}

void UART_Task(Developer_UART_HandleTypeDef *developer_uart_handler)
{
    Framing_Result_HandleTypeDef *frame;
    uint8_t current_processed_data = 0u;

    if (developer_uart_handler == NULL) {
        return;
    }

    UART_Connecting_Status_Check(developer_uart_handler);

    frame = &developer_uart_handler->framing_result;

    /* Rút liên tục chứ không một byte mỗi lượt: main loop có lúc bị
     * App_Sensor_ReadOnce() giữ tới 500 ms và SSD1306_UpdateScreen() giữ ~25 ms,
     * xử lý nhỏ giọt thì bộ đệm vòng đầy trước khi kịp tiêu thụ. */
    while (Ring_Buffer_Read_SingleData(developer_uart_handler->ring_buffer_handler_ptr,
                                       &current_processed_data) == DEV_SUCCESS) {
        if (Frame_Building(frame, Text_Filting(&current_processed_data),
                           &current_processed_data) != DEV_SUCCESS) {
            UART_Print(developer_uart_handler, "Fail, try again!\r\n");
            UART_Frame_Reset(frame);
            return;
        }

        if (frame->framing_result_ready_to_read) {
            UART_Frame_Dispatch(developer_uart_handler);
            /* Mỗi lượt một lệnh: nhường lại cho main loop để HAL kịp gửi xong
             * câu trả lời trước khi có câu tiếp theo. */
            return;
        }
    }

    /* Tới đây bộ đệm đã rỗng. Còn khung dở dang mà đường truyền im lặng đủ lâu
     * thì coi như app không gửi ký tự kết thúc — tự chốt khung.
     *
     * Phải đợi bộ đệm rỗng mới kiểm tra: last_received_tick đo thời điểm byte
     * về tới ISR, không phải lúc nó được xử lý. Còn byte chưa rút mà đã chốt
     * thì lệnh bị cắt cụt (vd đang kẹt đọc cảm biến 500 ms thì cả bản tin nằm
     * sẵn trong bộ đệm, mốc thời gian đã qua từ lâu). */
    if ((frame->framing_result_index > 0u) &&
        ((HAL_GetTick() - developer_uart_handler->last_received_tick) >=
         UART_FRAME_IDLE_TIMEOUT_MS)) {
        if (Frame_Building(frame, FILTING_RESULT_FINISH, NULL) != DEV_SUCCESS) {
            /* Khung tràn: báo lỗi thay vì lặng lẽ vứt bỏ. */
            UART_Print(developer_uart_handler, "Fail, try again!\r\n");
            UART_Frame_Reset(frame);
            return;
        }
        UART_Frame_Dispatch(developer_uart_handler);
    }
}
