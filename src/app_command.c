/*
 * app_command.c — NỘI DUNG bảng lệnh Bluetooth: Command_Menu[] và các handler.
 *
 * CƠ CHẾ tra bảng (khớp tên lệnh, tách tham số) nằm ở lib/Src/Command_Selector.c.
 * Handler phải chạm vào trạng thái ứng dụng nên chúng thuộc về tầng app: để
 * chung trong lib/ thì file thư viện buộc phải biết tới system_state và relay,
 * và mất luôn khả năng dùng lại ở project khác.
 *
 * Thêm một lệnh mới = thêm một handler ở đây và một dòng trong Command_Menu[].
 * Nhớ cập nhật trang HƯỚNG DẪN trong ui.c cho khớp.
 */

#include "app_state.h"

#include "Command_Selector.h"

#include <stdio.h>

static void Command_ON(char *return_msg, const char *args);
static void Command_OFF(char *return_msg, const char *args);
static void Command_STATUS(char *return_msg, const char *args);
static void Command_TEMP(char *return_msg, const char *args);
static void Command_HUM(char *return_msg, const char *args);
static void Command_AUTO(char *return_msg, const char *args);

/* UART_Task() tra cứu bảng này qua khai báo extern trong Command_Selector.h.
 * Cố tình KHÔNG static: đây là phần "nội dung" mà tầng lib mong đợi app cấp. */
Command_HandleTypeDef Command_Menu[] = {
    {"ON",     Command_ON},      /* Đóng relay */
    {"OFF",    Command_OFF},     /* Mở relay */
    {"STATUS", Command_STATUS},  /* Nhiệt độ, độ ẩm, relay, kết nối */
    {"TEMP",   Command_TEMP},    /* Chỉ nhiệt độ */
    {"HUM",    Command_HUM},     /* Chỉ độ ẩm */
    {"AUTO",   Command_AUTO}     /* Chế độ tự động — chưa triển khai */
};

uint8_t Command_Menu_Size = (uint8_t)(sizeof(Command_Menu) / sizeof(Command_Menu[0]));

/*==================== Handler của từng lệnh ====================*/

/* Sáu handler dưới đây có đôi chỗ gần giống nhau (ON/OFF chỉ khác một tham số,
 * TEMP/HUM chỉ khác bộ trường). Ở quy mô 6 lệnh, viết tay vẫn rõ ràng hơn là
 * gộp thành bảng có tham số; khi bảng lệnh vượt ~10 mục thì nên chuyển. */

static void Command_ON(char *return_msg, const char *args)
{
    (void)args;
    App_State_SetRelay(true);
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "RELAY_ON\r\n");
}

static void Command_OFF(char *return_msg, const char *args)
{
    (void)args;
    App_State_SetRelay(false);
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "RELAY_OFF\r\n");
}

static void Command_STATUS(char *return_msg, const char *args)
{
    (void)args;
    (void)App_State_FormatStatus(return_msg, COMMAND_RETURN_MSG_SIZE,
                                 APP_STATUS_FIELDS_ALL);
}

static void Command_TEMP(char *return_msg, const char *args)
{
    (void)args;
    (void)App_State_FormatStatus(return_msg, COMMAND_RETURN_MSG_SIZE,
                                 APP_STATUS_FIELD_TEMP);
}

static void Command_HUM(char *return_msg, const char *args)
{
    (void)args;
    (void)App_State_FormatStatus(return_msg, COMMAND_RETURN_MSG_SIZE,
                                 APP_STATUS_FIELD_HUM);
}

static void Command_AUTO(char *return_msg, const char *args)
{
    (void)args;
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "AUTO_MODE_READY\r\n");
}
