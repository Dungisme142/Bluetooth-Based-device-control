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
#define HEARTBEAT_LED_PORT        GPIOC
#define HEARTBEAT_LED_PIN         GPIO_PIN_13
#define HEARTBEAT_LED_ON_STATE    GPIO_PIN_RESET
#define HEARTBEAT_LED_OFF_STATE   GPIO_PIN_SET

/*---------------- LED CHỈ BÁO (MKE_M01_LED) ----------------*/
/* PA8 — active LOW */
#define STATUS_LED_PORT           GPIOA
#define STATUS_LED_PIN            GPIO_PIN_8
#define STATUS_LED_ON_STATE       GPIO_PIN_RESET
#define STATUS_LED_OFF_STATE      GPIO_PIN_SET

/*---------------- DHT11 (1-wire) ----------------*/
/* PB15 — open-drain khi output, IT_FALLING khi input.
 * PB15 thuộc nhóm EXTI15_10 (không phải EXTI2). */
#define DHT11_PORT                GPIOB
#define DHT11_PIN                 GPIO_PIN_15
#define DHT11_EXTI_IRQn           EXTI15_10_IRQn
#define DHT11_EXTI_PRIO           5U    /* PHẢI cao hơn (số nhỏ hơn) UART — xem PIN_MAP.md §4 */

/*---------------- RELAY (MKE_M05) ----------------*/
/* PB12 */
#define RELAY_PORT                GPIOB
#define RELAY_PIN                 GPIO_PIN_12

/*---------------- NÚT NHẤN ĐIỀU HƯỚNG UI ----------------*/
/* PA0 = NEXT, PA1 = PREV. Nút nối xuống GND, dùng pull-up nội -> ngắt cạnh
 * XUỐNG (IT_FALLING), mức nghỉ là HIGH.
 *
 * Cố ý KHÔNG dùng chân nào thuộc nhóm EXTI15_10: nhóm đó dùng chung một vector
 * với DHT11 (PB15), mà việc giải mã bit DHT11 đo thời gian ngay trong ISR —
 * thêm nút vào cùng vector sẽ làm sai phép đo mỗi khi người dùng bấm.
 * EXTI0/EXTI1 có vector riêng nên hoàn toàn tách biệt. */
#define BTN_NEXT_PORT             GPIOA
#define BTN_NEXT_PIN              GPIO_PIN_0
#define BTN_NEXT_EXTI_IRQn        EXTI0_IRQn

#define BTN_PREV_PORT             GPIOA
#define BTN_PREV_PIN              GPIO_PIN_1
#define BTN_PREV_EXTI_IRQn        EXTI1_IRQn

/* Thấp hơn (số lớn hơn) cả DHT11 lẫn UART: ISR của nút chỉ đặt một cờ, hoãn
 * vài chục micro-giây không ảnh hưởng gì. */
#define BTN_EXTI_PRIO             7U

/* Bỏ qua các cạnh xuống tiếp theo trong khoảng này để chống dội phím (ms) */
#define BTN_DEBOUNCE_MS           200U

/*---------------- I2C1 → OLED SSD1306 ----------------*/
/* PB6 = SCL, PB7 = SDA */
#define I2C1_SCL_PORT             GPIOB
#define I2C1_SCL_PIN              GPIO_PIN_6
#define I2C1_SDA_PORT             GPIOB
#define I2C1_SDA_PIN              GPIO_PIN_7
#define I2C1_CLOCK_SPEED          400000U   /* Fast mode */

/*---------------- USART1 → Bluetooth MKE-M15 ----------------*/
/* PA9 = TX, PA10 = RX (pull-up cố ý — xem PIN_MAP.md §6) */
#define BT_UART_TX_PORT           GPIOA
#define BT_UART_TX_PIN            GPIO_PIN_9
#define BT_UART_RX_PORT           GPIOA
#define BT_UART_RX_PIN            GPIO_PIN_10
#define BT_UART_BAUDRATE          9600U     /* Mặc định xuất xưởng của MKE-M15 */
#define BT_UART_IRQn              USART1_IRQn
#define BT_UART_PRIO              6U        /* PHẢI thấp hơn (số lớn hơn) DHT11_EXTI_PRIO */

#endif /* PIN_CONFIG_H */
