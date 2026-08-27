# BLUETOOTH-BASED DEVICE CONTROL

Trung tâm điều khiển thiết bị công suất qua Bluetooth, xây dựng trên STM32F103C8T6 và
module MKE-M15. Điều khiển tối đa **5 thiết bị độc lập** cùng lúc, bằng lệnh từ điện thoại
hoặc bằng nút bấm ngay trên board, kèm màn hình OLED hiển thị trạng thái và cảm biến
nhiệt độ / độ ẩm.

📚 **Tài liệu đầy đủ: [`docs/`](docs/README.md)**

## 1. Project Overview

Các thiết bị công suất trong nhà thường chỉ bật/tắt được tại chỗ. Dự án làm một trung tâm
điều khiển duy nhất: một board mang 5 kênh tín hiệu, nhận lệnh ASCII qua Bluetooth SPP, và
vẫn điều khiển được tại chỗ khi không có điện thoại.

Điểm cốt lõi của thiết kế: **hai đường điều khiển — Bluetooth và nút bấm — đi vào cùng một
hàm đặt trạng thái**, nên trạng thái thiết bị luôn nhất quán dù người dùng dùng đường nào.

Board **không mang tầng công suất**: nó xuất tín hiệu logic 3.3 V ra hàng rào chân cắm,
relay/SSR và mọi việc cách ly điện lưới nằm ở module ngoài.

→ Chi tiết: [docs/01 — Tổng quan](docs/01-tong-quan.md) ·
[docs/02 — Đặc tả yêu cầu](docs/02-dac-ta-yeu-cau.md)

## 2. Learning Objectives

Dự án chạm vào các chủ đề chính của một khoá Embedded C:

| Chủ đề | Thể hiện ở |
|---|---|
| GPIO ngõ ra, đảo cực tính | `Digital_Out.c` |
| Ngắt ngoài EXTI và phân bổ vector | 5 nút + bus DHT11 |
| Chống dội phím bằng phần mềm | `UI_SampleButton()` |
| UART theo ngắt, không blocking | `Receive_IT` + `Transmit_IT` |
| Bộ đệm vòng một-ghi/một-đọc | `Ring_Buffer.c` |
| Máy trạng thái (FSM) | Driver DHT11 |
| Timer làm đồng hồ µs + watchdog | TIM2 @ 1 MHz |
| I2C và driver màn hình đồ hoạ | `SSD1306.c` + framebuffer |
| Bảng con trỏ hàm | `Command_Menu[]` |
| Thiết kế superloop không chặn | Vòng lặp chính |
| Ưu tiên ngắt và phân tích timing | [docs/09](docs/09-timing-va-ngat.md) |

## 3. Hardware Overview

| Thành phần | Vai trò |
|---|---|
| STM32F103C8T6 (Blue Pill) | MCU, 72 MHz, 64 KB flash / 20 KB RAM |
| MKE-M15 | Module Bluetooth, UART 9600 8N1 |
| SSD1306 OLED 0.96" | Màn hình 128×64, I2C |
| DHT11 | Cảm biến nhiệt độ / độ ẩm, 1-wire |
| 5 nút nhấn | NEXT · PREV · UP · DOWN · OK |
| 5 ngõ ra + LED + J1–J5 | Tín hiệu điều khiển ra module công suất ngoài |
| Khối nguồn có bảo vệ | Polyfuse, chống ngược cực AO3401A, TVS |

→ Chi tiết + BOM: [docs/03 — Thiết kế phần cứng](docs/03-thiet-ke-phan-cung.md)

## 4. Pinout Schematic

Sơ đồ nguyên lý: [`docs/images/schematic.pdf`](docs/images/schematic.pdf)

| Chân | Chức năng | Chân | Chức năng |
|---|---|---|---|
| PA2 / PA3 | USART2 ↔ MKE-M15 | PA8 | OUT-1 |
| PA4 | DHT11 DATA | PB15 | OUT-2 |
| PA5 | Nút UP | PB14 | OUT-3 |
| PA6 | Nút PREV | PB13 | OUT-4 |
| PA7 | Nút OK | PB12 | OUT-5 |
| PB0 | Nút DOWN | PB10 / PB11 | I2C2 → OLED |
| PB1 | Nút NEXT | PC13 | LED nhịp tim |

Nguồn sự thật là [`stm-firmware/lib/Inc/pin_config.h`](stm-firmware/lib/Inc/pin_config.h) —
mọi pin/port/IRQn khai báo ở đúng một chỗ đó.

→ Bảng đầy đủ, ngoại vi, thứ tự khởi tạo:
[docs/04 — Sơ đồ chân](docs/04-so-do-chan.md)

## 5. Software Architecture

Bare-metal superloop, **không RTOS**. Tầng ứng dụng ở `stm-firmware/src/`, driver ở
`stm-firmware/lib/`, chiều phụ thuộc một hướng.

```
ISR USART2 ─► Ring_Buffer ─► Text_Filting ─► Frame_Building
                                          ─► Command_Selecting ─► handler ─► UART_Print

ISR EXTI (nút) ─► hàng đợi sự kiện ─► UI_Task ─► UI_Request_t ─► Set_Output()
```

Bốn nhịp thời gian trong vòng lặp: đọc cảm biến 2 s · báo trạng thái 3 s · nhịp tim 1 s ·
vẽ màn hình 500 ms. Không có `HAL_Delay` nào.

Quy tắc bất di bất dịch: **UI không tự bật thiết bị** — nó chỉ điền một đề nghị vào
`UI_Request_t` để `main.c` thi hành, nhờ vậy mọi lối vào bật/tắt đều đi qua đúng một hàm.

→ Chi tiết: [docs/05 — Kiến trúc phần mềm](docs/05-kien-truc-phan-mem.md)

## 6. Timing and Interrupt Design

Thang ưu tiên (số nhỏ = ưu tiên cao) xếp đúng theo độ chặt của hạn chót:

| Vector | Ưu tiên | Ràng buộc |
|---|---|---|
| TIM2 (watchdog DHT11) | 2 | Watchdog không được bị chèn |
| EXTI4 (bit DHT11) | 5 | **77–124 µs** giữa hai sườn |
| USART2 | 6 | ~1 ms một byte @ 9600 |
| EXTI nút bấm | 7 | Hàng chục ms |
| SysTick | 15 | — |

Ràng buộc bất biến: `TIM2 < EXTI4 < USART2 < EXTI(nút)`. Đảo thứ tự này sẽ làm DHT11 đọc
sai ngẫu nhiên.

→ Phân tích đầy đủ, gồm cả lý do phải có `HAL_UART_ErrorCallback`:
[docs/09 — Timing và ngắt](docs/09-timing-va-ngat.md)

## 7. Demo Video

_(Chưa có — điền link vào đây)_

## 8. Build and Flash

```bash
cd stm-firmware
cmake -G Ninja -B build && ninja -C build     # build
./build_and_flash.sh                          # Linux/macOS: build + nạp qua st-flash
build_and_flash.bat                           # Windows: build + nạp qua STM32_Programmer_CLI
```

Nạp qua ST-Link/SWD (PA13 = SWDIO, PA14 = SWCLK). Không có cổng serial nào nối tới máy
tính — **mọi log đi qua đường Bluetooth**; mở app SPP terminal trên điện thoại, thấy
`MKE-M15 ready` là firmware đã boot.

→ Chi tiết, gồm cả luồng nạp từ xa qua Raspberry Pi và bảng lỗi thường gặp:
[docs/10 — Build và nạp](docs/10-build-va-nap.md)

### Tập lệnh nhanh

```
ON n     OFF n      n = 1..5        →  OUT3_ON
ON ALL   OFF ALL                    →  ALL_ON
STATUS                              →  TEMP=27C HUM=61% OUT1=ON BT=OK OUT=10100
TEMP     HUM                        →  TEMP=27C
```

→ Đặc tả đầy đủ (cú pháp, lỗi, phiên mẫu):
[docs/06 — Giao thức Bluetooth](docs/06-giao-thuc-bluetooth.md)

## 9. HAL

Dùng **STM32F1xx HAL Driver** của ST, không dùng LL (đã loại khỏi build) và không dùng
STM32CubeMX — **project không có file `.ioc`**, toàn bộ cấu hình ngoại vi viết tay trong
`MX_*_Init()` và các hàm MSP.

Lý do: cấu hình viết tay đọc được, comment giải thích được từng lựa chọn, và không bị
CubeMX ghi đè mỗi lần sinh lại code. Cái giá phải trả là không có giao diện đồ hoạ để đổi
cấu hình.

HAL được dùng ở mức vừa phải: các driver trong `lib/` chỉ nhận `{port, pin, IRQn, htim}`
qua tham số chứ không tham chiếu biến toàn cục của tầng ứng dụng, nên bê sang project khác
được.

## 10. Performance & Timing Trade-offs

| Chỉ số | Giá trị |
|---|---|
| Chu kỳ vòng lặp điển hình | < 200 µs (~5000 vòng/giây) |
| Trường hợp xấu nhất | ~525 ms (đọc cảm biến quá hạn + một khung OLED) |
| Vẽ một khung OLED | ~25 ms, blocking (1 KB qua I2C 400 kHz) |
| Một phép đo DHT11 | ~25 ms bình thường, tới 500 ms khi cảm biến không đáp |
| Flash sử dụng | ~27,3 KB / 64 KB (**43 %**) — bản Debug |
| RAM sử dụng | ~4,1 KB / 20 KB (**21 %**) |

Ba đánh đổi chính đã cân nhắc:

1. **I2C blocking thay vì DMA** — driver đơn giản, không cần đồng bộ framebuffer; đổi lại
   chặn vòng lặp 25 ms mỗi khung. Chấp nhận được vì blocking chỉ chặn vòng lặp chính, **không
   chặn ngắt**.
2. **DHT11 ưu tiên cao hơn UART** — bảo vệ phép đo 77 µs, đổi lại có nguy cơ tràn ORE; đã bù
   bằng `HAL_UART_ErrorCallback()` mở lại phiên nhận.
3. **Mỗi lượt vòng lặp chỉ xử lý một lệnh UART** — bảo đảm câu trả lời trước gửi xong mới có
   câu sau; đổi lại lệnh gửi dồn dập có thể bị bỏ câu trả lời.

→ [docs/09 — Timing và ngắt](docs/09-timing-va-ngat.md)

## 11. Limitations and Future Improvements

Hạn chế đáng chú ý nhất:

- Chế độ `AUTO` mới là stub — trả lời nhưng chưa điều khiển gì theo ngưỡng cảm biến.
- Không lưu trạng thái qua reset (vừa là hạn chế, vừa là lựa chọn an toàn có chủ ý).
- Không xác thực / mã hoá trên kênh Bluetooth.
- Nhật ký 12 dòng bị `DHT OK` lấp đầy trong ~24 giây.
- Chưa có unit test tự động — kiểm chứng hoàn toàn thủ công.

Hướng đi tiếp, theo thứ tự ưu tiên: khung unit test chạy trên host (Unity + CTest) cho
`Ring_Buffer` / `Command_Selector` / bộ dựng khung → hoàn thiện chế độ AUTO có hysteresis →
tách `main.c` thành module → lưu trạng thái vào flash → IWDG.

→ Danh sách đầy đủ kèm hệ quả và hướng khắc phục từng mục:
[docs/12 — Hạn chế và hướng phát triển](docs/12-han-che-va-huong-phat-trien.md)

Nghiệm thu bằng 79 ca kiểm thử thủ công:
[docs/11 — Kế hoạch kiểm thử](docs/11-ke-hoach-kiem-thu.md)

## 12. Author

Đồ án cuối khoá Embedded C.

### Nhóm thực hiện

| Họ và tên | MSSV | Phần đảm nhiệm |
|---|---|---|
| Hoàng Bùi Nghĩa Dũng | 2410585 | Thiết kế module DHT11, chạy thử mạch nguyên lý trên breadboard |
| Trần Đình Ý | 2414100 | Sơ đồ nguyên lý (schematic), layout PCB, màn hình OLED |
| Nguyễn Tuấn Minh | 2412108 | Giao tiếp UART, hướng dẫn sử dụng BLE |
| Nguyễn Trọng Nhân | 2452882 | Thiết kế luồng hệ thống và cơ chế ngắt |

### Giảng viên hướng dẫn

| Vai trò | Họ và tên |
|---|---|
| Mentor | Võ Phúc Thịnh |
| Th.S | Nguyễn Khánh Lợi |

### Thông tin dự án

| | |
|---|---|
| Repository | [Bluetooth-Based-device-control](https://github.com/Dungisme142/Bluetooth-Based-device-control) |
| MCU | STM32F103C8T6 |
| Toolchain | arm-none-eabi-gcc · CMake · Ninja |
