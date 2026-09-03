/*
 * ui.h — Tầng giao diện OLED: 5 trang, điều hướng và điều khiển bằng 5 nút.
 *
 *   Trang 1  HOME      dashboard: nhiệt độ, độ ẩm, 5 ngõ ra, uptime
 *   Trang 2  OUTPUTS   danh sách 5 kênh, con trỏ chọn, OK để bật/tắt
 *   Trang 3  SENSOR    DHT11 chi tiết kèm thanh mức
 *   Trang 4  LOG       12 sự kiện gần nhất, cuộn bằng UP/DOWN
 *   Trang 5  HƯỚNG DẪN tập lệnh Bluetooth và vai trò 5 nút
 *
 * Năm nút chạy bằng ngắt EXTI. ISR chỉ đẩy sự kiện vào hàng đợi rồi thoát;
 * toàn bộ việc vẽ và việc đổi trạng thái nằm trong UI_Task() ở vòng lặp chính
 * — I2C là blocking, không bao giờ được gọi từ trong ngắt.
 *
 * UI KHÔNG tự bật/tắt thiết bị: nó chỉ điền một "đề nghị" vào UI_Request_t và
 * để main.c thi hành. Nhờ vậy ui.c không cần biết gì về tầng app (chiều phụ
 * thuộc là main.c -> ui.h, không có chiều ngược lại).
 */
#ifndef UI_H
#define UI_H

#include "auth.h"
#include "main.h"

#include <stdbool.h>

typedef enum {
    ALARM_STATE_NORMAL = 0,
    ALARM_STATE_HIGH_HUMIDITY,
    ALARM_STATE_DHT_FAULT
} UI_AlarmState_t;

/* Ảnh chụp trạng thái hệ thống mà UI cần để vẽ một khung hình. Tầng app dựng
 * đầy rồi truyền xuống, UI không tự đọc phần cứng. */
typedef struct {
    /* DHT11 chỉ có độ phân giải 1 độ C / 1 % — nó luôn phát byte thập phân
     * bằng 0. Không lưu phần thập phân ở đây để khỏi hiển thị ".0" giả. */
    uint8_t         temperature_c;        /* Nhiệt độ (độ C) */
    uint8_t         humidity_pct;         /* Độ ẩm (%) */
    bool            output_on[OUT_COUNT]; /* Trạng thái 4 kênh ngõ ra */
    bool            heartbeat_led_on;     /* true = LED PC13 đang sáng */
    bool            bluetooth_connected;  /* true = đã bắt tay được với module BT */
    bool            sensor_valid;         /* true = đã từng đọc DHT11 thành công ít nhất 1 lần */
    bool            dht_health_ok;        /* true = cảm biến đang tốt; false = lỗi/quá hạn */
    uint32_t        sensor_age_s;         /* Số giây kể từ lần đọc thành công cuối */
    Auth_Owner_t    auth_owner;           /* AUTH_NONE, AUTH_LOCAL, AUTH_BLE */
    UI_AlarmState_t alarm_state;          /* NORMAL, HIGH_HUMIDITY, DHT_FAULT */
} UI_Data_t;

/* Việc UI muốn tầng app làm sau khung hình vừa vẽ. */
typedef struct {
    bool    toggle_output; /* true = xin đảo trạng thái một kênh */
    uint8_t channel;       /* Kênh cần đảo, 0..OUT_COUNT-1 */
    bool    local_login;   /* true = người dùng đăng nhập thành công bằng keypad */
    bool    user_activity; /* true = có thao tác nút của user để gia hạn timeout */
} UI_Request_t;

/**
 * @brief  Khởi tạo SSD1306 và vẽ màn hình chào. Gọi sau Board_Init().
 * @param  hi2c  Bus I2C đang nối với OLED.
 */
void UI_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Xử lý các nút đang chờ rồi vẽ lại màn hình nếu đến kỳ refresh.
 *
 * Gọi mỗi vòng lặp chính; hàm tự quyết định có vẽ hay không nên gọi thừa
 * không tốn gì. Một lần vẽ đẩy 1 KB qua I2C 400 kHz mất khoảng 25 ms.
 *
 * @param  now_ms  Mốc thời gian hiện tại (HAL_GetTick()).
 * @param  data    Trạng thái hệ thống mới nhất.
 * @param  req     Nhận đề nghị đổi ngõ ra; bên gọi phải thi hành nếu
 *                 req->toggle_output là true. Luôn được ghi đè, kể cả khi
 *                 không có gì để làm.
 */
void UI_Task(uint32_t now_ms, const UI_Data_t *data, UI_Request_t *req);

/**
 * @brief  Xử lý ngắt của nút điều hướng — gọi từ HAL_GPIO_EXTI_Callback().
 *
 * An toàn khi gọi từ ISR: chỉ chống dội phím rồi đẩy sự kiện vào hàng đợi,
 * không dùng I2C và không chạm vào GPIO của thiết bị.
 *
 * @param  exti_pin  Chân vừa sinh ngắt.
 * @retval true nếu đây đúng là một nút của UI (đã xử lý), false nếu không phải.
 */
bool UI_HandleButtonIrq(uint16_t exti_pin);

/**
 * @brief  Ghi một dòng vào nhật ký hiện ở trang LOG.
 *
 * Chỉ giữ UI_LOG_LINES dòng gần nhất; dòng cũ nhất bị đẩy ra. Chuỗi dài hơn
 * bề ngang màn hình sẽ bị cắt.
 *
 * KHÔNG được gọi từ ISR: hàm này ghi vào bộ đệm vòng mà UI_Task() đang đọc.
 */
void UI_Log(const char *text);

#endif /* UI_H */
