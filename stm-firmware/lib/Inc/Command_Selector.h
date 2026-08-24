/*
 * Command_Selector.h — Cơ chế tra bảng lệnh nhận qua đường truyền.
 *
 * Bảng lệnh cụ thể là nội dung của tầng ứng dụng, không thuộc lib/.
 */
#ifndef COMMAND_SELECTOR_H_
#define COMMAND_SELECTOR_H_

#include "Global_Enum.h"
#include <stdint.h>

/* Kích thước buffer trả lời cấp cho mỗi handler. Bên gọi (UART_Task) cấp phát
 * đúng kích thước này, nên handler PHẢI dùng nó làm giới hạn của snprintf. */
#define COMMAND_RETURN_MSG_SIZE 256u

/**
 * @brief  Chữ ký của một handler lệnh.
 * @param  return_msg  Buffer COMMAND_RETURN_MSG_SIZE byte để ghi câu trả lời.
 * @param  args        Phần đứng sau tên lệnh, hoặc NULL nếu lệnh không có tham số.
 */
typedef void (*Command_Executing_t)(char *return_msg, const char *args);

typedef struct {
    const char         *command;
    Command_Executing_t command_executing;
} Command_HandleTypeDef;

/* Bảng lệnh của ứng dụng — định nghĩa trong src/app_command.c */
extern Command_HandleTypeDef Command_Menu[];
extern uint8_t Command_Menu_Size;

/**
 * @brief  Tìm lệnh khớp với chuỗi nhận được rồi gọi handler của nó.
 *
 * Không khớp lệnh nào thì ghi "Invalid Command" vào return_msg.
 *
 * @param  command_menu       Bảng lệnh.
 * @param  command_menu_size  Số mục của bảng.
 * @param  return_msg         Buffer COMMAND_RETURN_MSG_SIZE byte nhận câu trả lời.
 * @param  received_cmd       Chuỗi lệnh đã kết thúc bằng '\0'.
 * @retval DEV_SUCCESS nếu đã gọi được một handler, DEV_FAIL nếu không.
 */
Developer_Action_Result_t Command_Selecting(const Command_HandleTypeDef *command_menu,
                                            uint8_t command_menu_size,
                                            char *return_msg,
                                            const char *received_cmd);

#endif /* COMMAND_SELECTOR_H_ */
