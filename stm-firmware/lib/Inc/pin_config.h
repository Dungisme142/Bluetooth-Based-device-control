/*
 * pin_config.h — Bảng chân duy nhất của toàn hệ thống.
 *
 * QUY TẮC: mọi pin phải khai báo ở đây. KHÔNG hardcode pin/port/IRQn trong driver
 * hay trong file logic. Port sang board khác chỉ cần sửa đúng file này.
 *
 * Bảng này bám theo schematic KiCad (U2 — Blue Pill STM32F103C8T6). Schematic
 * là nguồn sự thật; code sai thì sửa code.
 *
 * Tham chiếu: local/PIN_MAP.md
 */
#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "stm32f1xx_hal.h"

/*---------------- LED HEARTBEAT (onboard Blue Pill) ----------------*/
/* PC13 — active LOW. Schematic để trống chân này vì nó không nối ra ngoài,
 * nhưng LED trên board vẫn gắn cứng vào PC13, nên vẫn dùng làm dấu hiệu
 * "firmware còn sống". Đây là LED DUY NHẤT còn lại: LED chỉ báo trạng thái cũ
 * nằm ở PB13, mà PB13 nay là ngõ ra OUT-4. */
#define HEARTBEAT_LED_PORT      GPIOC
#define HEARTBEAT_LED_PIN       GPIO_PIN_13
#define HEARTBEAT_LED_ON_STATE  GPIO_PIN_RESET
#define HEARTBEAT_LED_OFF_STATE GPIO_PIN_SET

/*---------------- DHT11 (1-wire) ----------------*/
/* PA4 — open-drain khi output, IT_FALLING khi input.
 * PA4 nằm trên EXTI line 4 -> vector riêng EXTI4_IRQn, không dùng chung với
 * bất kỳ nút nào (nút nằm ở line 0, 1 và 5..7). */
#define DHT11_PORT      GPIOA
#define DHT11_PIN       GPIO_PIN_4
#define DHT11_EXTI_IRQn EXTI4_IRQn
#define DHT11_EXTI_PRIO 5U /* PHẢI cao hơn (số nhỏ hơn) UART — xem PIN_MAP.md §4 */

/*---------------- CHÂN CẢNH BÁO TỰ ĐỘNG (ALARM) ----------------*/
/* PA8: Điều khiển tự động theo độ ẩm (> 90%) và lỗi cảm biến DHT11 (nhấp nháy 250ms).
 * Active-HIGH. Không nằm trong outputs[] hay bảng lệnh GPIO thủ công. */
#define ALARM_PORT     GPIOA
#define ALARM_PIN      GPIO_PIN_8
#define ALARM_ON_STATE GPIO_PIN_SET
#define ALARM_NAME     "ALARM"
#define ALARM_PIN_NAME "PA8"

/*---------------- 4 NGÕ RA SỐ ĐIỀU KHIỂN THIẾT BỊ ----------------*/
/* OUT-1..OUT-4: PB15, PB14, PB13, PB12.
 * Cả 4 kênh đều nằm trên GPIOB.
 *
 * _ON_STATE tách riêng để đảo cực tính khi tầng ngoài tác động mức THẤP mà
 * không phải sửa logic ở đâu khác.
 * _NAME là nhãn hiện trên OLED (tối đa 5 ký tự).
 * _PIN_NAME là tên chân in trên board. */
#define OUT1_PORT     GPIOB
#define OUT1_PIN      GPIO_PIN_15
#define OUT1_ON_STATE GPIO_PIN_SET
#define OUT1_NAME     "OUT-1"
#define OUT1_PIN_NAME "PB15"

#define OUT2_PORT     GPIOB
#define OUT2_PIN      GPIO_PIN_14
#define OUT2_ON_STATE GPIO_PIN_SET
#define OUT2_NAME     "OUT-2"
#define OUT2_PIN_NAME "PB14"

#define OUT3_PORT     GPIOB
#define OUT3_PIN      GPIO_PIN_13
#define OUT3_ON_STATE GPIO_PIN_SET
#define OUT3_NAME     "OUT-3"
#define OUT3_PIN_NAME "PB13"

#define OUT4_PORT     GPIOB
#define OUT4_PIN      GPIO_PIN_12
#define OUT4_ON_STATE GPIO_PIN_SET
#define OUT4_NAME     "OUT-4"
#define OUT4_PIN_NAME "PB12"

/* Gộp theo port để main.c ghi mức TẮT một lần mỗi port */
#define OUT_GPIOA_PINS (ALARM_PIN)
#define OUT_GPIOB_PINS (OUT1_PIN | OUT2_PIN | OUT3_PIN | OUT4_PIN)

/* Số kênh ngõ ra điều khiển thiết bị: 4 kênh */
#define OUT_COUNT 4U

/*---------------- NÚT NHẤN ----------------*/
/* 5 nút theo schematic: PA5 = UP, PA6 = PREV, PA7 = OK, PB0 = DOWN, PB1 = NEXT.
 * Tất cả nối xuống GND, dùng pull-up nội -> mức nghỉ là HIGH.
 *
 * Phân bố vector: PA5/PA6/PA7 -> EXTI9_5_IRQn (dùng chung); PB0 -> EXTI0_IRQn;
 * PB1 -> EXTI1_IRQn. Cố ý KHÔNG nút nào nằm trên EXTI line 4 vì đó là vector
 * của DHT11 — việc giải mã bit DHT11 đo thời gian ngay trong ISR, thêm nút vào
 * cùng vector sẽ làm sai phép đo mỗi khi người dùng bấm. */
#define BTN1_PORT      GPIOA
#define BTN1_PIN       GPIO_PIN_5
#define BTN1_EXTI_IRQn EXTI9_5_IRQn

#define BTN2_PORT      GPIOA
#define BTN2_PIN       GPIO_PIN_6
#define BTN2_EXTI_IRQn EXTI9_5_IRQn

#define BTN3_PORT      GPIOA
#define BTN3_PIN       GPIO_PIN_7
#define BTN3_EXTI_IRQn EXTI9_5_IRQn

#define BTN4_PORT      GPIOB
#define BTN4_PIN       GPIO_PIN_0
#define BTN4_EXTI_IRQn EXTI0_IRQn

#define BTN5_PORT      GPIOB
#define BTN5_PIN       GPIO_PIN_1
#define BTN5_EXTI_IRQn EXTI1_IRQn

/* Gộp sẵn theo port để main.c gọi HAL_GPIO_Init() một lần mỗi port */
#define BTN_GPIOA_PINS (BTN1_PIN | BTN2_PIN | BTN3_PIN)
#define BTN_GPIOB_PINS (BTN4_PIN | BTN5_PIN)

/* Vai trò UI của từng nút. Tầng UI chỉ nói tới các tên _NEXT/_PREV/_UP/_DOWN/_OK,
 * đổi dây chỉ cần đổi bí danh ở đây. */
#define BTN_UP_PORT        BTN1_PORT
#define BTN_UP_PIN         BTN1_PIN
#define BTN_UP_EXTI_IRQn   BTN1_EXTI_IRQn

#define BTN_PREV_PORT      BTN2_PORT
#define BTN_PREV_PIN       BTN2_PIN
#define BTN_PREV_EXTI_IRQn BTN2_EXTI_IRQn

#define BTN_OK_PORT        BTN3_PORT
#define BTN_OK_PIN         BTN3_PIN
#define BTN_OK_EXTI_IRQn   BTN3_EXTI_IRQn

#define BTN_DOWN_PORT      BTN4_PORT
#define BTN_DOWN_PIN       BTN4_PIN
#define BTN_DOWN_EXTI_IRQn BTN4_EXTI_IRQn

#define BTN_NEXT_PORT      BTN5_PORT
#define BTN_NEXT_PIN       BTN5_PIN
#define BTN_NEXT_EXTI_IRQn BTN5_EXTI_IRQn

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

/*---------------- I2C2 → OLED SSD1306 ----------------*/
/* PB10 = SCL2, PB11 = SDA2 theo schematic.
 *
 * Đây là vị trí mặc định của I2C2 nên KHÔNG cần remap — I2C2 trên F103 không
 * có phương án remap nào khác. I2C2 nằm trên APB1 (PCLK1 = 36 MHz); HAL tự
 * tính CCR từ PCLK1 nên tốc độ bus đúng 400 kHz. */
#define I2C2_SCL_PORT    GPIOB
#define I2C2_SCL_PIN     GPIO_PIN_10
#define I2C2_SDA_PORT    GPIOB
#define I2C2_SDA_PIN     GPIO_PIN_11
#define I2C2_CLOCK_SPEED 400000U /* Fast mode */

/*---------------- USART2 → Bluetooth MKE-M15 ----------------*/
/* PA2 = TX2, PA3 = RX2 theo schematic.
 * RX để PULL-UP cố ý — xem PIN_MAP.md §6.
 *
 * USART2 nằm trên APB1 (PCLK1 = 36 MHz), không phải APB2; HAL tự tính thanh
 * ghi BRR từ PCLK1 nên baud đúng 9600. */
#define BT_UART_INSTANCE USART2
#define BT_UART_TX_PORT  GPIOA
#define BT_UART_TX_PIN   GPIO_PIN_2
#define BT_UART_RX_PORT  GPIOA
#define BT_UART_RX_PIN   GPIO_PIN_3
#define BT_UART_BAUDRATE 9600U /* Mặc định xuất xưởng của MKE-M15 */
#define BT_UART_IRQn     USART2_IRQn
#define BT_UART_PRIO     6U /* PHẢI thấp hơn (số lớn hơn) DHT11_EXTI_PRIO */

#endif /* PIN_CONFIG_H */
