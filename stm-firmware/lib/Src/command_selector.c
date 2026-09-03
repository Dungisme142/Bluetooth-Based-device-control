/*
 * command_selector.c — CƠ CHẾ tra bảng lệnh: khớp tên lệnh, tách tham số,
 * gọi handler tương ứng.
 *
 * NỘI DUNG bảng lệnh (Command_Menu[] và các handler) nằm ở src/app_command.c.
 * Tách hai thứ này ra vì handler phải chạm vào trạng thái ứng dụng; để chung
 * ở đây thì file trong lib/ buộc phải biết tới tầng app và mất khả năng dùng lại.
 */

#include "command_selector.h"

#include "global_enum.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

Developer_Action_Result_t Command_Selecting(const Command_HandleTypeDef *command_menu,
                                            uint8_t command_menu_size,
                                            char *return_msg,
                                            const char *received_cmd)
{
    uint8_t i;

    if ((command_menu == NULL) || (return_msg == NULL) || (received_cmd == NULL)) {
        return DEV_FAIL;
    }

    for (i = 0u; i < command_menu_size; i++) {
        size_t cmd_len = strlen(command_menu[i].command);

        if (strncmp(received_cmd, command_menu[i].command, cmd_len) != 0) {
            continue;
        }

        /* Khớp tiền tố chưa đủ: "ONLINE" cũng bắt đầu bằng "ON". Chỉ nhận khi
         * ngay sau tên lệnh là hết chuỗi (không tham số) hoặc một dấu cách
         * (phần còn lại là tham số). */
        if (received_cmd[cmd_len] == '\0') {
            command_menu[i].command_executing(return_msg, NULL);
            return DEV_SUCCESS;
        }
        if (received_cmd[cmd_len] == ' ') {
            command_menu[i].command_executing(return_msg, received_cmd + cmd_len + 1u);
            return DEV_SUCCESS;
        }
    }

    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "Invalid Command\r\n");
    return DEV_FAIL;
}
