/*
 * ui.h — Tầng giao diện OLED: 4 trang, điều hướng bằng 2 nút phần cứng.
 *
 *   Trang 1  GPIO STATUS   trạng thái các chân đầu ra (relay, 2 LED)
 *   Trang 2  DHT11 SENSOR  nhiệt độ, độ ẩm, tình trạng Bluetooth
 *   Trang 3  UART LOG      4 sự kiện gần nhất kèm mốc thời gian
 *   Trang 4  HƯỚNG DẪN     tập lệnh Bluetooth và cách dùng 2 nút
 *
 * Nút NEXT (PA0) và PREV (PA1) chạy bằng ngắt EXTI. ISR chỉ ghi lại trang mới
 * rồi đặt cờ; toàn bộ việc vẽ nằm trong UI_Task() ở vòng lặp chính — I2C là
 * blocking, không bao giờ được gọi từ trong ngắt.
 */
#ifndef UI_H
#define UI_H

#include "main.h"

#include <stdbool.h>

/* Ảnh chụp trạng thái hệ thống mà UI cần để vẽ một khung hình. Tầng app dựng
 * đầy rồi truyền xuống, UI không tự đọc phần cứng. */
typedef struct {
    /* DHT11 chỉ có độ phân giải 1 độ C / 1 % — nó luôn phát byte thập phân
     * bằng 0. Không lưu phần thập phân ở đây để khỏi hiển thị ".0" giả. */
    uint8_t temperature_c;        /* Nhiệt độ (độ C) */
    uint8_t humidity_pct;         /* Độ ẩm (%) */
    bool    relay_on;             /* true = relay đang đóng */
    bool    status_led_on;        /* true = LED PA8 đang sáng */
    bool    heartbeat_led_on;     /* true = LED PC13 đang sáng */
    bool    bluetooth_connected;  /* true = đã bắt tay được với module BT */
} UI_Data_t;

/**
 * @brief  Khởi tạo SSD1306 và vẽ màn hình chào. Gọi sau Board_Init().
 * @param  hi2c  Bus I2C đang nối với OLED.
 */
void UI_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Vẽ lại màn hình nếu đến kỳ refresh hoặc vừa có nút được bấm.
 *
 * Gọi mỗi vòng lặp chính; hàm tự quyết định có vẽ hay không nên gọi thừa
 * không tốn gì. Một lần vẽ đẩy 1 KB qua I2C 400 kHz mất khoảng 25 ms.
 *
 * @param  now_ms  Mốc thời gian hiện tại (HAL_GetTick()).
 * @param  data    Trạng thái hệ thống mới nhất.
 */
void UI_Task(uint32_t now_ms, const UI_Data_t *data);

/**
 * @brief  Xử lý ngắt của nút điều hướng — gọi từ HAL_GPIO_EXTI_Callback().
 *
 * An toàn khi gọi từ ISR: chỉ đổi số trang và đặt cờ vẽ lại, không dùng I2C.
 * Đã chống dội phím sẵn bằng BTN_DEBOUNCE_MS.
 *
 * @param  exti_pin  Chân vừa sinh ngắt.
 * @retval true nếu đây đúng là một nút của UI (đã xử lý), false nếu không phải.
 */
bool UI_HandleButtonIrq(uint16_t exti_pin);

/**
 * @brief  Ghi một dòng vào nhật ký hiện ở trang UART LOG.
 *
 * Chỉ giữ 4 dòng gần nhất; dòng cũ nhất bị đẩy ra. Chuỗi dài hơn 15 ký tự sẽ
 * bị cắt cho vừa bề ngang màn hình.
 */
void UI_Log(const char *text);

#endif /* UI_H */
