/*
 * app_state.h — Trạng thái của ứng dụng và cách trình bày nó thành chuỗi.
 *
 * Một chỗ duy nhất giữ "hệ thống đang thế nào". Mọi module app khác đọc/ghi
 * qua đây thay vì tự nuôi bản sao riêng.
 */
#ifndef APP_STATE_H
#define APP_STATE_H

#include "Global_Enum.h"
#include "ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Gom các giá trị đọc được gần nhất vào một chỗ thay vì rải biến rời rạc. */
typedef struct {
    /* Chỉ giữ phần nguyên: DHT11 có độ phân giải 1 độ C / 1 %, hai byte thập
     * phân trong khung 40 bit của nó luôn bằng 0 (đó là đặc tính cảm biến,
     * không phải lỗi giải mã). */
    uint8_t temperature_c;        /* Nhiệt độ đọc lần cuối (độ C) */
    uint8_t humidity_pct;         /* Độ ẩm đọc lần cuối (%) */
    bool    relay_on;             /* true = relay đang đóng */
    bool    status_led_on;        /* true = LED PA8 đang sáng */
    bool    heartbeat_led_on;     /* true = LED PC13 đang sáng */
    bool    bluetooth_connected;  /* true = đã nhận được byte từ module BT */
} System_State_t;

/* Bản duy nhất — định nghĩa trong app_state.c */
extern System_State_t system_state;

/* Các trường có thể có trong chuỗi báo trạng thái. Dùng macro dịch bit chứ
 * không dùng enum: giá trị enum là danh sách liên tiếp, không phải bit độc lập. */
#define APP_STATUS_FIELD_TEMP   (1u << 0)
#define APP_STATUS_FIELD_HUM    (1u << 1)
#define APP_STATUS_FIELD_RELAY  (1u << 2)
#define APP_STATUS_FIELD_LINK   (1u << 3)
#define APP_STATUS_FIELDS_ALL   (APP_STATUS_FIELD_TEMP | APP_STATUS_FIELD_HUM | \
                                 APP_STATUS_FIELD_RELAY | APP_STATUS_FIELD_LINK)

/**
 * @brief  Khởi tạo relay về mức an toàn (mở) và đồng bộ trạng thái.
 * @retval DEV_SUCCESS nếu relay khởi tạo được, DEV_FAIL nếu không.
 */
Developer_Action_Result_t App_State_Init(void);

/**
 * @brief  Đóng/mở relay, cập nhật LED chỉ báo và ghi nhật ký UI.
 */
void App_State_SetRelay(bool on);

/**
 * @brief  Nhấp nháy LED heartbeat và ghi lại mức hiện tại của nó.
 */
void App_State_ToggleHeartbeat(void);

/**
 * @brief  Ghi chuỗi báo trạng thái vào `out` theo các trường được chọn.
 *
 * Một chỗ duy nhất quyết định định dạng của chuỗi này. Trước đây nó được viết
 * lại ở SendStatusToPhone() và Command_STATUS() với hai định dạng đã trôi khỏi
 * nhau (một bên có "BT=", một bên không).
 *
 * @param  out       Buffer nhận kết quả, luôn được kết thúc bằng '\0'.
 * @param  out_size  Kích thước buffer.
 * @param  fields    Tổ hợp các cờ APP_STATUS_FIELD_*.
 * @retval Số ký tự đã ghi (không kể '\0').
 */
size_t App_State_FormatStatus(char *out, size_t out_size, uint8_t fields);

/**
 * @brief  Sao trạng thái sang dạng mà ui.c cần để vẽ một khung hình.
 *
 * UI không đọc trực tiếp System_State_t: hai tầng giữ struct riêng để đổi bố
 * cục màn hình không kéo theo sửa tầng ứng dụng và ngược lại.
 */
void App_State_Snapshot(UI_Data_t *out);

#endif /* APP_STATE_H */
