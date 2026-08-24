# 01 — Tổng quan dự án

## 1.1 Bài toán

Các thiết bị công suất trong nhà (quạt, đèn, bơm, ổ cắm) thường chỉ bật/tắt được tại chỗ
bằng công tắc cơ. Muốn điều khiển từ xa thì mỗi thiết bị phải có một bộ điều khiển riêng,
mỗi bộ một ứng dụng — càng nhiều thiết bị càng rối.

Dự án này làm một **trung tâm điều khiển** duy nhất: một board STM32 mang 5 kênh tín hiệu
điều khiển, nhận lệnh qua Bluetooth từ điện thoại, đồng thời vẫn bật/tắt được bằng nút
ngay trên board và hiển thị trạng thái trên màn hình OLED. Board còn đo nhiệt độ và độ ẩm
môi trường để làm cơ sở cho chế độ tự động về sau.

## 1.2 Ý tưởng hệ thống

```
   Điện thoại                board STM32F103C8T6              Thiết bị công suất
  ┌───────────┐            ┌──────────────────────┐          ┌──────────────────┐
  │ App SPP   │  Bluetooth │  MKE-M15  ──► USART2 │  OUT-1..5│ Module công suất │
  │ terminal  │◄──────────►│                      ├─────────►│ (relay / SSR /   │
  └───────────┘   9600 8N1 │  DHT11    ──► EXTI4  │  3.3V TTL│  MOSFET) ─► tải  │
                           │  OLED     ◄── I2C1   │          └──────────────────┘
   5 nút bấm ─────────────►│  5 nút    ──► EXTI   │
                           └──────────────────────┘
```

Hai đường điều khiển — lệnh Bluetooth và nút bấm tại chỗ — đi vào cùng một hàm đặt trạng
thái, nên trạng thái thiết bị luôn nhất quán dù người dùng dùng đường nào.

## 1.3 Phạm vi

### Nằm trong phạm vi

- Bật/tắt độc lập **5 kênh ngõ ra** (OUT-1..OUT-5), có lệnh bật/tắt tất cả cùng lúc.
- Nhận lệnh ASCII qua Bluetooth SPP, trả lời từng lệnh và tự báo trạng thái mỗi 3 giây.
- Điều khiển tại chỗ bằng 5 nút bấm, không cần điện thoại.
- Hiển thị trạng thái trên OLED 128×64 với 5 trang: dashboard, danh sách ngõ ra, cảm
  biến, nhật ký sự kiện, hướng dẫn.
- Đo nhiệt độ và độ ẩm bằng DHT11, chu kỳ 2 giây.
- Nhật ký 12 sự kiện gần nhất xem được ngay trên màn hình, không cần máy tính.

### Ngoài phạm vi

- **Không có tầng công suất trên board.** Board chỉ xuất tín hiệu logic 3.3 V ra hàng rào
  chân cắm; relay/SSR/MOSFET và mọi việc cách ly điện lưới nằm ở module ngoài. Xem
  [03 — Thiết kế phần cứng](03-thiet-ke-phan-cung.md) §3.7.
- **Không lưu trạng thái qua reset.** Mất điện rồi có lại thì cả 5 kênh về TẮT (chủ ý —
  đây là trạng thái an toàn).
- **Không kết nối Internet/cloud**, không app di động riêng — dùng app SPP terminal có sẵn.
- **Không xác thực, không mã hoá** trên kênh Bluetooth. Ai ghép cặp được thì điều khiển
  được.
- **Chế độ tự động (`AUTO`) chưa triển khai** — lệnh có tồn tại và trả lời, nhưng chưa
  thực sự điều khiển gì theo ngưỡng cảm biến.

## 1.4 Mục tiêu học tập

Dự án được thiết kế để chạm vào các chủ đề chính của môn Embedded C:

| Chủ đề | Thể hiện ở đâu |
|---|---|
| GPIO ngõ ra, đảo cực tính | `Digital_Out.c`, `output_on_state[]` trong `main.c` |
| Ngắt ngoài EXTI, phân bổ vector | 5 nút + DHT11, xem [04](04-so-do-chan.md) §4.3 |
| Chống dội phím bằng phần mềm | `UI_SampleButton()` trong `ui.c` |
| UART theo ngắt (không blocking) | `HAL_UART_Receive_IT` + `HAL_UART_Transmit_IT` |
| Cấu trúc dữ liệu bộ đệm vòng | `Ring_Buffer.c`, một người ghi / một người đọc |
| Máy trạng thái (FSM) | Driver DHT11, xem [08](08-dac-ta-cam-bien-dht11.md) |
| Timer làm đồng hồ micro-giây + watchdog | TIM2 ở 1 MHz |
| I2C và driver màn hình đồ hoạ | `SSD1306.c` + framebuffer |
| Bảng con trỏ hàm | `Command_Menu[]` + `Command_Selecting()` |
| Thiết kế superloop không chặn | Vòng lặp chính trong `main.c`, xem [09](09-timing-va-ngat.md) |
| Ưu tiên ngắt và phân tích timing | [09](09-timing-va-ngat.md) |
| Tách tầng, tránh phụ thuộc vòng | UI chỉ *đề nghị*, app thi hành |

## 1.5 Thành phần phần cứng chính

| Thành phần | Vai trò |
|---|---|
| STM32F103C8T6 (Blue Pill) | Bộ xử lý trung tâm, 72 MHz, 64 KB flash / 20 KB RAM |
| MKE-M15 | Module Bluetooth, giao tiếp UART 9600 8N1 |
| SSD1306 OLED 0.96" | Màn hình 128×64 đơn sắc, giao tiếp I2C |
| DHT11 | Cảm biến nhiệt độ / độ ẩm, giao thức 1-wire |
| 5 nút nhấn | Điều hướng và điều khiển tại chỗ |
| 5 ngõ ra + LED chỉ báo | Tín hiệu điều khiển ra module công suất ngoài |

## 1.6 Thuật ngữ

| Thuật ngữ | Nghĩa trong tài liệu này |
|---|---|
| **Kênh** (channel) | Một ngõ ra điều khiển, đánh số 1..5 với người dùng |
| **Superloop** | Vòng `while(1)` duy nhất, không dùng hệ điều hành thời gian thực |
| **SPP** | Serial Port Profile — Bluetooth giả lập cổng nối tiếp |
| **EXTI** | External Interrupt — ngắt ngoài của STM32 gắn với chân GPIO |
| **ISR** | Interrupt Service Routine — hàm xử lý ngắt |
| **Frame / khung lệnh** | Một dòng lệnh hoàn chỉnh nhận qua UART, kết thúc bằng CR/LF |
| **Debounce** | Chống dội — lọc nhiễu cơ khí của tiếp điểm nút bấm |
| **Framebuffer** | Vùng RAM chứa ảnh màn hình, đẩy sang OLED một lần mỗi khung hình |
| **Blocking** | Hàm giữ CPU cho tới khi xong, chặn phần còn lại của vòng lặp |
| **HAL** | Hardware Abstraction Layer — thư viện driver của ST |
