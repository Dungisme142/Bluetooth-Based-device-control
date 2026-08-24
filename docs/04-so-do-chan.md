# 04 — Sơ đồ chân và ngoại vi

Nguồn sự thật của bảng này là [`stm-firmware/lib/Inc/pin_config.h`](../stm-firmware/lib/Inc/pin_config.h).
Mọi pin/port/IRQn phải khai báo ở đó; driver không được hardcode. Đổi board chỉ cần sửa
đúng một file ấy.

## 4.1 Bảng chân đầy đủ

| Chân | Chức năng | Hướng / Chế độ | Pull | Ghi chú |
|---|---|---|---|---|
| **PA2** | USART2_TX → MKE-M15 RX | AF Push-Pull, HIGH speed | — | Baud 9600 |
| **PA3** | USART2_RX ← MKE-M15 TX | Input | **PULLUP** | Pull-up cố ý, chống nhiễu khi module chưa cấp nguồn |
| **PA4** | DHT11 DATA | Đổi qua lại: Output-OD ↔ Input EXTI falling | PULLUP khi input | `EXTI4_IRQn` — vector riêng, không chia với nút nào |
| **PA5** | Nút **NEXT** | Input EXTI both edges | PULLUP | `EXTI9_5_IRQn` |
| **PA6** | Nút **DOWN** | Input EXTI both edges | PULLUP | `EXTI9_5_IRQn` |
| **PA7** | Nút **OK** | Input EXTI both edges | PULLUP | `EXTI9_5_IRQn` |
| **PA8** | **OUT-1** | Output Push-Pull | NOPULL | Kênh duy nhất nằm trên GPIOA |
| **PA13** | SWDIO | — | — | Dành riêng cho nạp/debug |
| **PA14** | SWCLK | — | — | Dành riêng cho nạp/debug |
| **PB0** | Nút **UP** | Input EXTI both edges | PULLUP | `EXTI0_IRQn` |
| **PB1** | Nút **PREV** | Input EXTI both edges | PULLUP | `EXTI1_IRQn` |
| **PB6** | I2C1_SCL → OLED | AF Open-Drain, HIGH speed | — | 400 kHz |
| **PB7** | I2C1_SDA → OLED | AF Open-Drain, HIGH speed | — | 400 kHz |
| **PB12** | **OUT-5** | Output Push-Pull | NOPULL | |
| **PB13** | **OUT-4** | Output Push-Pull | NOPULL | Trước đây là LED chỉ báo, nay là kênh ngõ ra |
| **PB14** | **OUT-3** | Output Push-Pull | NOPULL | |
| **PB15** | **OUT-2** | Output Push-Pull | NOPULL | |
| **PC13** | LED nhịp tim (onboard) | Output Push-Pull, LOW speed | NOPULL | **Active LOW** |

### Chân còn trống

`PA0`, `PA1`, `PA9`, `PA10`, `PA11`, `PA12`, `PA15`, `PB3`, `PB4`, `PB5`, `PB8`, `PB9`,
`PB10`, `PB11`.

`PA15` / `PB3` / `PB4` chỉ dùng được nhờ `__HAL_AFIO_REMAP_SWJ_NOJTAG()` trong
`HAL_MspInit()` (`main.c:688`) — lệnh này giải phóng chân JTAG nhưng **giữ nguyên SWD**.

## 4.2 Bảng ngoại vi

| Ngoại vi | Cấu hình | Dùng cho |
|---|---|---|
| **RCC** | HSE 8 MHz → PLL ×9 → SYSCLK **72 MHz**<br/>AHB /1 → HCLK 72 MHz<br/>APB1 /2 → PCLK1 **36 MHz**<br/>APB2 /1 → PCLK2 72 MHz<br/>`FLASH_LATENCY_2` | Toàn hệ thống |
| **USART2** | 9600 baud, 8 bit, no parity, 1 stop, không flow control, oversampling 16, chế độ TX+RX theo ngắt | Bluetooth |
| **I2C1** | 400 kHz Fast-mode, DutyCycle 2, địa chỉ 7 bit, blocking (không ngắt, không DMA) | OLED |
| **TIM2** | Bộ đếm tự do 1 MHz (1 tick = 1 µs), Period 65535, prescaler tính từ clock thực tế | Đồng hồ µs + watchdog cho DHT11 |
| **EXTI** | Line 4 (DHT11), line 0/1 và 5–9 (nút bấm) | Ngắt ngoài |
| **AFIO** | `SWJ_NOJTAG` | Giải phóng PA15/PB3/PB4 |

Không dùng **ADC**, **SPI**, **DMA**, **RTC**, **IWDG**.

### Vì sao APB1 phải là /2

PCLK1 tối đa của STM32F103 là **36 MHz** — đặt /1 ở SYSCLK 72 MHz là vượt trần và chip
chạy sai. Hệ quả kéo theo: theo quy tắc của F1, khi APB1 prescaler khác /1 thì clock cấp
cho timer bằng **2 × PCLK1** = 72 MHz. `MX_TIM2_Init()` (`main.c:647`) tính prescaler từ
`HAL_RCC_GetPCLK1Freq()` chứ không ghi cứng, và gọi `Error_Handler()` nếu không chia ra
được đúng 1 MHz — nhờ vậy đổi cây clock sẽ báo lỗi ngay thay vì làm sai lặng lẽ timing của
DHT11.

## 4.3 Bảng ưu tiên ngắt (NVIC)

Số **nhỏ hơn** = ưu tiên **cao hơn**.

| Vector | Ưu tiên | Nguồn | Lý do đặt mức này |
|---|---|---|---|
| `TIM2_IRQn` | **2** | Watchdog + chuyển pha FSM của DHT11 | Cao nhất trong các ngắt ứng dụng: bỏ lỡ mốc 18 ms hoặc watchdog 500 µs là hỏng cả phiên đo |
| `EXTI4_IRQn` | **5** | Sườn xuống trên bus DHT11 | Giải mã bit bằng cách **đo thời gian ngay trong ISR**; các bit chỉ cách nhau 77–124 µs. Trễ vài chục µs là đọc sai bit |
| `USART2_IRQn` | **6** | Nhận/phát byte Bluetooth | Phải **thấp hơn** DHT11. Ở 9600 baud một byte mất ~1 ms — chậm vài chục µs không sao. Bị chèn nhiều thì sinh lỗi tràn ORE, và `HAL_UART_ErrorCallback()` đã xử lý |
| `EXTI9_5_IRQn` | **7** | Nút PA5, PA6, PA7 | Thấp nhất: ISR chỉ đặt một cờ vào hàng đợi, hoãn vài chục µs người dùng không cảm nhận được |
| `EXTI0_IRQn` | **7** | Nút PB0 | như trên |
| `EXTI1_IRQn` | **7** | Nút PB1 | như trên |
| `SysTick` | 15 | `HAL_GetTick()` | Mặc định của HAL, thấp nhất toàn hệ thống |

**Ràng buộc bất biến**: `TIM2 < EXTI4(DHT11) < USART2 < EXTI(nút)`. Đảo thứ tự này sẽ làm
DHT11 đọc sai ngẫu nhiên — kiểu lỗi rất khó truy. Ràng buộc được ghi thành comment ngay
tại `pin_config.h:34` và `pin_config.h:178`.

### Vì sao không nút nào nằm trên EXTI line 4

DHT11 dùng PA4 → `EXTI4_IRQn` là một vector **riêng**. Nếu đặt một nút lên PC4/PB4 thì nó
dùng chung vector với DHT11, và mỗi lần người dùng bấm nút giữa lúc đang đọc cảm biến,
ISR sẽ lấy nhầm timestamp → sai bit. Việc phân bổ chân nút sang line 0, 1, 5, 6, 7 là chủ ý.

## 4.4 Ánh xạ vector ngắt

Bảng handler nằm ở [`stm-firmware/src/stm32f1xx_it.c`](../stm-firmware/src/stm32f1xx_it.c).

| Handler | Gọi tới | Kết quả |
|---|---|---|
| `SysTick_Handler` | `HAL_IncTick()` | Cập nhật `HAL_GetTick()` |
| `EXTI4_IRQHandler` | `HAL_GPIO_EXTI_Callback(PA4)` → `DHT11_CallbackEXTI()` | Giải mã một bit |
| `EXTI0_IRQHandler` | `HAL_GPIO_EXTI_Callback(PB0)` → `UI_HandleButtonIrq()` | Sự kiện nút UP |
| `EXTI1_IRQHandler` | `HAL_GPIO_EXTI_Callback(PB1)` → `UI_HandleButtonIrq()` | Sự kiện nút PREV |
| `EXTI9_5_IRQHandler` | Gọi riêng cho PA5, PA6, PA7 | Sự kiện NEXT / DOWN / OK |
| `USART2_IRQHandler` | `HAL_UART_IRQHandler()` → `HAL_UART_RxCpltCallback()` | Đẩy 1 byte vào ring buffer |
| `TIM2_IRQHandler` | `HAL_TIM_IRQHandler()` → `DHT11_CallbackTIM2()` | Chuyển pha FSM / watchdog |

`HAL_GPIO_EXTI_Callback()` (`main.c:792`) là điểm đến chung của mọi ngắt EXTI. Nó thử nút
**trước** vì `UI_HandleButtonIrq()` trả về `false` ngay khi chân không phải của nó; chân
nào không ai nhận thì được bỏ qua.

## 4.5 Thứ tự khởi tạo bắt buộc

Thứ tự trong `main()` (`main.c:156-195`) không được đảo:

```
1.  HAL_Init()                    ─ SysTick, NVIC grouping
2.  SystemClock_Config()          ─ 72 MHz; phải xong trước mọi tính toán baud/prescaler
3.  MX_GPIO_Init()                ─ bật clock GPIOA/B/C, ghi mức TẮT lên 5 chân ngõ ra,
                                    cấu hình nút + LED, đặt ưu tiên EXTI
4.  MX_USART2_UART_Init()         ─ kéo theo HAL_UART_MspInit(): chân AF + NVIC USART2
5.  MX_I2C1_Init()                ─ kéo theo HAL_I2C_MspInit(): PB6/PB7 AF-OD
6.  MX_TIM2_Init()                ─ 1 MHz, bật NVIC TIM2
7.  Digital_Out_Init() × 5        ─ chuyển 5 chân sang output (mức đã an toàn từ bước 3)
    + Set_Output(i, false)
8.  DHT11_Init()                  ─ cần htim2 đã sẵn sàng từ bước 6
9.  UI_Init()                     ─ cần hi2c1 đã sẵn sàng từ bước 5
10. Ring_Buffer_Init()
11. Developer_UART_Handler_Init()
12. HAL_UART_Receive_IT()         ─ mở phiên nhận byte đầu tiên
```

**Điểm mấu chốt ở bước 3 → 7**: sau reset, chân GPIO là input floating và thanh ghi ODR có
thể còn giữ mức của lần chạy trước. Nếu chuyển chân sang output *rồi mới* ghi mức, thiết bị
sẽ bật trong vài chu kỳ clock. Vì vậy `MX_GPIO_Init()` ghi mức TẮT lên cả 5 chân **trước**,
rồi `Digital_Out_Init()` mới đổi chúng sang output. Đây là cách hiện thực yêu cầu FR-25.

PA4 (DHT11) và 5 chân ngõ ra **cố ý không** được cấu hình trong `MX_GPIO_Init()` — driver
tương ứng tự lo, vì chân DHT11 còn phải đổi mode qua lại lúc chạy.
