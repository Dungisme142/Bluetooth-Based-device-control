/*
 * pin_config.h — Bảng chân duy nhất của toàn hệ thống.
 *
 * QUY TẮC: mọi pin phải khai báo ở đây. KHÔNG hardcode pin/port/IRQn trong driver
 * hay trong file logic. Port sang board khác chỉ cần sửa đúng file này.
 *
 * Tham chiếu: local/PIN_MAP.md
 */
#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "stm32f1xx_hal.h"

/*---------------- LED HEARTBEAT (onboard Blue Pill) ----------------*/
/* PC13 — active LOW */
#define HEARTBEAT_LED_PORT      GPIOC
#define HEARTBEAT_LED_PIN       GPIO_PIN_13
#define HEARTBEAT_LED_ON_STATE  GPIO_PIN_RESET
#define HEARTBEAT_LED_OFF_STATE GPIO_PIN_SET

/*---------------- LED CHỈ BÁO (MKE_M01_LED) ----------------*/
/* PB13 — active LOW */
#define STATUS_LED_PORT      GPIOB
#define STATUS_LED_PIN       GPIO_PIN_13
#define STATUS_LED_ON_STATE  GPIO_PIN_RESET
#define STATUS_LED_OFF_STATE GPIO_PIN_SET

/*---------------- DHT11 (1-wire) ----------------*/
/* PB1 — open-drain khi output, IT_FALLING khi input.
 * PB1 nằm trên EXTI line 1 -> vector riêng EXTI1_IRQn, không dùng chung với
 * bất kỳ nút nào (nút nằm ở line 4 và 5..8). */
#define DHT11_PORT      GPIOB
#define DHT11_PIN       GPIO_PIN_1
#define DHT11_EXTI_IRQn EXTI1_IRQn
#define DHT11_EXTI_PRIO 5U /* PHẢI cao hơn (số nhỏ hơn) UART — xem PIN_MAP.md §4 */

/*---------------- RELAY (MKE_M05) ----------------*/
/* PB12 — đồng thời là kênh 1 của bảng ngõ ra bên dưới */
#define RELAY_PORT GPIOB
#define RELAY_PIN  GPIO_PIN_12

/*---------------- 5 NGÕ RA SỐ ĐIỀU KHIỂN THIẾT BỊ ----------------*/
/* Kênh 1 là relay PB12 sẵn có; 4 kênh còn lại là chân trống của Blue Pill.
 *
 * Vì sao chọn PB14/PB15/PB10/PB11: cùng nằm trên GPIOB (clock đã bật sẵn),
 * không đụng SWD (PA13/PA14), không đụng JTAG (PA15/PB3/PB4), không đụng
 * EXTI line 1 của DHT11, và các chức năng thay thế của chúng (SPI2, I2C2,
 * USART3) đều không được dùng trong firmware này.
 *
 * _ON_STATE tách riêng để đảo cực tính khi lắp module relay tác động mức
 * THẤP mà không phải sửa logic ở đâu khác.
 * _NAME là nhãn hiện trên OLED — để ở đây để nhãn và chân không trôi khỏi nhau
 * (tối đa 5 ký tự cho vừa bố cục trang OUTPUTS).
 * _PIN_NAME là tên chân in trên board, cũng hiện trên OLED. */
#define OUT1_PORT     RELAY_PORT
#define OUT1_PIN      RELAY_PIN
#define OUT1_ON_STATE GPIO_PIN_SET
#define OUT1_NAME     "RELAY"
#define OUT1_PIN_NAME "PB12"

#define OUT2_PORT     GPIOB
#define OUT2_PIN      GPIO_PIN_14
#define OUT2_ON_STATE GPIO_PIN_SET
#define OUT2_NAME     "LAMP"
#define OUT2_PIN_NAME "PB14"

#define OUT3_PORT     GPIOB
#define OUT3_PIN      GPIO_PIN_15
#define OUT3_ON_STATE GPIO_PIN_SET
#define OUT3_NAME     "FAN"
#define OUT3_PIN_NAME "PB15"

#define OUT4_PORT     GPIOB
#define OUT4_PIN      GPIO_PIN_10
#define OUT4_ON_STATE GPIO_PIN_SET
#define OUT4_NAME     "AUX1"
#define OUT4_PIN_NAME "PB10"

#define OUT5_PORT     GPIOB
#define OUT5_PIN      GPIO_PIN_11
#define OUT5_ON_STATE GPIO_PIN_SET
#define OUT5_NAME     "AUX2"
#define OUT5_PIN_NAME "PB11"

/* Gộp theo port để board.c ghi mức TẮT một lần trước khi các chân thành output */
#define OUT_GPIOB_PINS (OUT1_PIN | OUT2_PIN | OUT3_PIN | OUT4_PIN | OUT5_PIN)

/* Số kênh — cả tầng app lẫn tầng UI đều lấy con số này từ đây, không ai tự
 * khai lại. Thêm kênh = thêm một khối OUTn_* ở trên, tăng số này, và thêm một
 * dòng vào ui_outputs[] trong src/ui.c cùng app_outputs[] trong src/app_state.c. */
#define OUT_COUNT 5U

/*---------------- NÚT NHẤN ----------------*/
/* 5 nút: PA4, PA5, PA6, PA7, PB8. Tất cả nối xuống GND, dùng pull-up nội ->
 * ngắt cạnh XUỐNG (IT_FALLING), mức nghỉ là HIGH.
 *
 * Phân bố vector: PA4 -> EXTI4_IRQn (riêng); PA5/PA6/PA7/PB8 -> EXTI9_5_IRQn
 * (dùng chung). Cố ý KHÔNG nút nào nằm trên EXTI line 1 vì đó là vector của
 * DHT11 — việc giải mã bit DHT11 đo thời gian ngay trong ISR, thêm nút vào
 * cùng vector sẽ làm sai phép đo mỗi khi người dùng bấm. */
#define BTN1_PORT      GPIOA
#define BTN1_PIN       GPIO_PIN_4
#define BTN1_EXTI_IRQn EXTI4_IRQn

#define BTN2_PORT      GPIOA
#define BTN2_PIN       GPIO_PIN_5
#define BTN2_EXTI_IRQn EXTI9_5_IRQn

#define BTN3_PORT      GPIOA
#define BTN3_PIN       GPIO_PIN_6
#define BTN3_EXTI_IRQn EXTI9_5_IRQn

#define BTN4_PORT      GPIOA
#define BTN4_PIN       GPIO_PIN_7
#define BTN4_EXTI_IRQn EXTI9_5_IRQn

#define BTN5_PORT      GPIOB
#define BTN5_PIN       GPIO_PIN_8
#define BTN5_EXTI_IRQn EXTI9_5_IRQn

/* Gộp sẵn theo port để board.c gọi HAL_GPIO_Init() một lần mỗi port */
#define BTN_GPIOA_PINS (BTN1_PIN | BTN2_PIN | BTN3_PIN | BTN4_PIN)
#define BTN_GPIOB_PINS (BTN5_PIN)

/* Vai trò UI của từng nút. Tầng UI chỉ nói tới các tên _NEXT/_PREV/_UP/_DOWN/_OK,
 * đổi dây chỉ cần đổi bí danh ở đây. */
#define BTN_NEXT_PORT      BTN1_PORT
#define BTN_NEXT_PIN       BTN1_PIN
#define BTN_NEXT_EXTI_IRQn BTN1_EXTI_IRQn

#define BTN_PREV_PORT      BTN2_PORT
#define BTN_PREV_PIN       BTN2_PIN
#define BTN_PREV_EXTI_IRQn BTN2_EXTI_IRQn

#define BTN_UP_PORT        BTN3_PORT
#define BTN_UP_PIN         BTN3_PIN
#define BTN_UP_EXTI_IRQn   BTN3_EXTI_IRQn

#define BTN_DOWN_PORT      BTN4_PORT
#define BTN_DOWN_PIN       BTN4_PIN
#define BTN_DOWN_EXTI_IRQn BTN4_EXTI_IRQn

#define BTN_OK_PORT        BTN5_PORT
#define BTN_OK_PIN         BTN5_PIN
#define BTN_OK_EXTI_IRQn   BTN5_EXTI_IRQn

/* Thấp hơn (số lớn hơn) cả DHT11 lẫn UART: ISR của nút chỉ đặt một cờ, hoãn
 * vài chục micro-giây không ảnh hưởng gì. */
#define BTN_EXTI_PRIO 7U

/* Thời gian chờ tiếp điểm hết nảy sau mỗi lần đổi mức (ms). Trong khoảng này
 * mọi cạnh của CHÍNH nút đó bị bỏ qua; hết khoảng thì ui.c đọc lại mức thật
 * của chân để chốt trạng thái.
 *
 * Mốc thời gian tính riêng cho từng nút: một mốc dùng chung sẽ nuốt mất thao
 * tác hai nút liên tiếp — chọn kênh rồi bấm OK ngay sau đó.
 *
 * 25 ms: tiếp điểm nút bấm phổ thông hết nảy trong khoảng 5-10 ms. Không đặt
 * lớn hơn nhiều được, vì khoảng này cũng là thời gian tối thiểu của một cú
 * chạm — nhấn rồi nhả nhanh hơn thế thì lần nhả sẽ bị bỏ qua. */
#define BTN_DEBOUNCE_MS 25U

/*---------------- I2C1 → OLED SSD1306 ----------------*/
/* PB6 = SCL, PB7 = SDA */
#define I2C1_SCL_PORT    GPIOB
#define I2C1_SCL_PIN     GPIO_PIN_6
#define I2C1_SDA_PORT    GPIOB
#define I2C1_SDA_PIN     GPIO_PIN_7
#define I2C1_CLOCK_SPEED 400000U /* Fast mode */

/*---------------- USART1 → Bluetooth MKE-M15 ----------------*/
/* PA9 = TX, PA10 = RX (pull-up cố ý — xem PIN_MAP.md §6) */
#define BT_UART_TX_PORT  GPIOA
#define BT_UART_TX_PIN   GPIO_PIN_9
#define BT_UART_RX_PORT  GPIOA
#define BT_UART_RX_PIN   GPIO_PIN_10
#define BT_UART_BAUDRATE 9600U /* Mặc định xuất xưởng của MKE-M15 */
#define BT_UART_IRQn     USART1_IRQn
#define BT_UART_PRIO     6U /* PHẢI thấp hơn (số lớn hơn) DHT11_EXTI_PRIO */

#endif /* PIN_CONFIG_H */
