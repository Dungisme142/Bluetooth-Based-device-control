# DỰ ÁN: BỘ ĐIỀU KHIỂN THIẾT BỊ VÀ GIÁM SÁT MÔI TRƯỜNG QUA BLUETOOTH

---

## 1. VẤN ĐỀ / BỐI CẢNH (PROBLEM STATEMENT)
Trong các hệ thống gia dụng quy mô nhỏ, việc phải thao tác trực tiếp trên thiết bị gây ra nhiều bất tiện. Đồng thời, việc thiếu thông tin giám sát các thông số môi trường (như nhiệt độ, độ ẩm) thời gian thực khiến người dùng khó quản lý trạng thái không gian sống và hoạt động của thiết bị một cách tối ưu.

---

## 2. MỤC TIÊU (OBJECTIVES)
* **Điều khiển từ xa:** Xây dựng hệ thống điều khiển bật/tắt thiết bị điện thoại thông qua kết nối Bluetooth.
* **Xử lý truyền thông:** Nhận và giải mã chính xác các lệnh điều khiển qua giao tiếp UART.
* **Điều khiển chấp hành:** Điều khiển Relay mô phỏng việc đóng/ngắt tải điện gia dụng.
* **Thu thập dữ liệu:** Đọc chính xác nhiệt độ và độ ẩm từ cảm biến DHT11.
* **Phản hồi thời gian thực:** Gửi dữ liệu cảm biến và trạng thái thiết bị về điện thoại theo chu kỳ định trước.
* **Chỉ báo & Hiển thị:** Sử dụng LED chỉ báo trạng thái hoạt động/kết nối và màn hình OLED 0.96 inch để trực quan hóa dữ liệu tại chỗ.
* **Kiến trúc phần mềm:** Thiết kế chương trình dạng mô-đun hóa (Layered Architecture), dễ bảo trì, mở rộng và kiểm thử độc lập.
* **Độ ổn định:** Bảo đảm hệ thống vận hành liên tục, phản hồi tức thì với các lệnh điều khiển cơ bản.

---

## 3. PHẠM VI DỰ ÁN (SCOPE)

### 🟢 Thực hiện (In-Scope)
* **Bộ điều khiển trung tâm:** Vi điều khiển STM32F103C8T6 (Blue Pill).
* **Truyền thông không dây:** Giao tiếp Bluetooth với module HC-05 qua UART.
* **Cảm biến:** Đọc thông số nhiệt độ, độ ẩm bằng cảm biến DHT11.
* **Chấp hành & Chỉ báo:** Điều khiển 01 Relay, 01 LED trạng thái và 01 màn hình OLED 0.96".
* **Giao diện người dùng:** Gửi/nhận dữ liệu và trạng thái qua ứng dụng Bluetooth Terminal trên điện thoại.
* **Công cụ phát triển:** Lập trình thuần C trên VS Code (kết hợp CMake & GCC ARM).

### 🚫 Không thực hiện (Out-of-Scope)
* Không kết nối Internet, Wi-Fi hoặc tích hợp các nền tảng IoT Cloud.
* Không xây dựng ứng dụng mobile chuyên dụng (App riêng) phức tạp.
* Không lưu trữ cơ sở dữ liệu dài hạn (Database/Cloud Storage).
* Không ứng dụng DHT11 cho các môi trường yêu cầu độ chính xác công nghiệp cao.
* Không trực tiếp đóng ngắt tải điện áp cao ($220\text{V}$) trong giai đoạn thử nghiệm khi chưa trang bị đầy đủ mạch bảo vệ an toàn.

---

## 4. PHẦN CỨNG & THIẾT BỊ SỬ DỤNG

### 🔹 Vi điều khiển / Kit
* Kit phát triển **STM32F103C8T6** (Blue Pill).

### 🔹 Cảm biến & Module ngoại vi
* **01 Module Bluetooth HC-05:** Truyền thông không dây.
* **01 Cảm biến DHT11:** Đo nhiệt độ và độ ẩm.
* **01 Module Relay:** Đóng ngắt tải (phù hợp mức logic STM32).
* **01 LED đơn:** Báo trạng thái kết nối / hệ thống.
* **01 Màn hình OLED 0.96 inch (I2C/SPI):** Hiển thị trực tiếp thông số cảm biến và trạng thái hệ thống.

---

## 5. CÔNG CỤ PHẦN MỀM & THƯ VIỆN

* **VS Code:** Môi trường lập trình chính (IDE).
* **Ngôn ngữ:** C (ANSI C).
* **Trình biên dịch & Build System:** GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`), CMake, Ninja.
* **Thư viện nền tảng:** STM32 HAL Library, CMSIS.
* **Cấu hình & Nạp code:** STM32CubeMX / STM32CubeIDE (tham khảo/sinh code ngoại vi), STM32CubeProgrammer.
* **Quản lý mã nguồn:** Git & GitHub.
* **Giao diện di động:** Ứng dụng *Bluetooth Terminal* trên Smartphone.

---

## 6. KIẾN TRÚC & KỸ THUẬT PHẦN MỀM

### Ngoại vi & Kỹ thuật STM32 sử dụng
* **GPIO Output:** Điều khiển LED chỉ báo và chân kích Relay.
* **GPIO Input/Output (Software 1-Wire):** Giao tiếp một dây bằng phần mềm để đọc dữ liệu DHT11.
* **UART (Interrupt / RX Event):** Giao tiếp hai chiều không mất dữ liệu giữa STM32 và HC-05.
* **Timer Base:** Tạo khoảng thời gian trễ độ chính xác cao (microsecond) cho DHT11 và định thời gửi dữ liệu / chuyển Task.
* **SysTick / HAL Timing:** Quản lý các khoảng thời gian chờ thông thường.

### 📁 Cấu trúc thư mục mã nguồn (Project Structure)

```text
Bluetooth-based device control/
├── .git/                   # Thư mục Git nội bộ
├── cmake/                  # Cấu hình CMake cho toolchain ARM
├── docs/                   # Tài liệu thiết kế, sơ đồ khối, tổng quan project
│   ├── images/            # Hình ảnh, sơ đồ khối, tài liệu thiết kế
│   │   ├── design/        # Bản vẽ và tài liệu thiết kế
│   │   └── block_diagram/ # Sơ đồ khối hệ thống
│   └── project_docs/     # Tổng quan project và hướng dẫn phát triển
│       ├── Project_Overview.md
│       └── Developer_Guide.md

├── Drivers/                 # HAL/CMSIS của ST
│   ├── CMSIS/
│   ├── Inc/                # stm32f1xx_hal_conf.h
│   └── STM32F1xx_HAL_Driver/
├── lib/                    # Thư viện mô-đun phần cứng + hạ tầng lệnh
│   ├── Inc/
│   └── Src/
├── src/                    # Tầng ứng dụng
│   ├── board.c / board.h   # BSP: clock, GPIO, USART1, I2C1, TIM2
│   ├── main.c              # Trạng thái hệ thống, vòng lặp chính, bảng lệnh
│   ├── ui.c / ui.h         # 4 trang OLED + 2 nút điều hướng
│   ├── stm32f1xx_it.c      # Toàn bộ vector ngắt
│   ├── stm32f1xx_hal_msp.c # Cấu hình mức thấp từng ngoại vi
│   └── sysmem.c
├── test/                   # Thư mục kiểm thử (Test Bench)
│   ├── test1.c
│   ├── test2.c
│   ├── test3.c
│   └── test4.c
├── CMakeLists.txt
├── build_and_flash.bat
├── README.md
├── startup_stm32f103xb.s
└── STM32F103xx_FLASH.ld
```

### 🔧 Git và commit

Sau khi chỉnh sửa tài liệu hoặc code, nên thực hiện theo các bước sau:

```bash
git status
git add <file_or_folder>
git commit -m "<message>"
git push
```

Ví dụ:

```bash
git add docs/ docs/project_docs/Project_Overview.md
git commit -m "docs: update project structure"
```

---

## 7. MÔ TẢ CHI TIẾT CÁC MÔ-ĐUN

> **Cập nhật:** thiết kế ban đầu dự tính một tầng `logic/` gồm 5 task
> (`SENSOR_TASK`, `TIMER_TASK`, `ACTUATOR_TASK`, `PROTOCOL_TASK`, `DISPLAY_TASK`).
> Tầng đó **đã được gỡ bỏ**: nó chưa bao giờ nằm trong build (CMake chỉ gom
> `src/`, `lib/Src/`, `Drivers/`) và trùng chức năng với code đang chạy thật.
> Bảng dưới đây ghi lại chức năng đó **hiện nằm ở đâu**.

| Chức năng (tên task cũ) | Nơi thực thi hiện tại |
|---|---|
| `SENSOR_TASK` — đọc DHT11, kiểm tra timeout/checksum | `lib/Src/DHT11.c` (máy trạng thái + checksum) và `Dht11_ReadOnce()` trong `src/main.c` (một phép đo trọn vẹn, tối đa ~500 ms) |
| `TIMER_TASK` — nhịp định kỳ cho các tác vụ | Vòng lặp chính trong `src/main.c` so sánh `HAL_GetTick()` với các mốc `SENSOR_PERIOD_MS` / `STATUS_PERIOD_MS` / `HEARTBEAT_PERIOD_MS`. TIM2 **không** dùng làm nhịp hệ thống — nó là đồng hồ micro-giây riêng của DHT11, cấu hình trong `Board_TIM2_Init()` |
| `ACTUATOR_TASK` — điều khiển relay và LED | `Relay_SetState()` trong `src/main.c`, dựng trên `lib/Src/MKE_M05_RELAY.c` |
| `PROTOCOL_TASK` — nhận và phân tích lệnh Bluetooth | Chuỗi `lib/Src/Ring_Buffer.c` → `Text_Filter.c` → `Frame_Builder.c` → `Command_Selector.c`, điều phối bởi `UART_Task()` trong `lib/Src/uart.c`. Bảng lệnh `Command_Menu[]` khai báo trong `src/main.c` |
| `DISPLAY_TASK` — hiển thị OLED | `src/ui.c` — 4 trang (GPIO STATUS, DHT11 SENSOR, UART LOG, HƯỚNG DẪN), chuyển trang bằng 2 nút EXTI trên PA0/PA1 |

Chi tiết chân, mức ưu tiên ngắt và ràng buộc phần cứng: xem `local/PIN_MAP.md`.

---

## 8. QUẢN LÝ PHÂN CÔNG CÔNG VIỆC

| STT | Công việc | Người phụ trách | Kết quả bàn giao |
| :---: | :--- | :--- | :--- |
| **1** | **Cảm biến và Đọc dữ liệu**<br>*(DHT11 – Single-Wire)* | **Hoàng Bùi Nghĩa Dũng** | Hoàn thành phần đọc dữ liệu cảm biến và kiểm thử cơ bản. |
| **2** | **Ngắt và Định thời**<br>*(Timer Base & Periodical Task)* | **Nguyễn Trọng Nhân** | Hoàn thành phần timer và các cờ định kỳ cho hệ thống. |
| **3** | **Cơ cấu chấp hành và Chỉ báo**<br>*(Relay & LED Status – GPIO)* | **Trần Đình Ý** | Hoàn thành phần điều khiển Relay và LED. |
| **4** | **Giao tiếp và Giải mã lệnh**<br>*(UART – HC-05 & Protocol Parser)* | **Nguyễn Tuấn Minh** | Hoàn thành phần nhận/gửi dữ liệu Bluetooth và xử lý lệnh. |

---

## 9. KẾ HOẠCH KIỂM THỬ VÀ ĐÁNH GIÁ (TESTING & EVALUATION)

### 🎯 Dữ liệu đầu vào (Input Data)
* Thu thập chuỗi khung truyền lệnh từ ứng dụng Bluetooth Terminal.
* Dữ liệu thời gian thực xung đọc được từ chân Data của DHT11.
* Bộ dữ liệu giả lập (Mock data) truyền qua UART để test parser khi không có cảm biến thực.

### 🧪 Kịch bản kiểm thử (Test Cases)
* **TC1 - Kiểm thử ngoại vi độc lập (`test1.c`):** Test riêng biệt đọc DHT11, truyền nhận UART HC-05, đóng ngắt Relay, chớp tắt LED và hiển thị OLED.
* **TC2 - Kiểm thử tích hợp (`test2.c`):** Test phối hợp giữa UART + Relay/LED (nhận lệnh bật Relay) và DHT11 + OLED (đọc dữ liệu và đẩy lên màn hình).
* **TC3 - Kiểm thử toàn hệ thống (`test3.c`):** Vận hành hệ thống thực tế: Nhận lệnh từ điện thoại $\rightarrow$ Điều khiển Relay $\rightarrow$ Đọc DHT11 định kỳ $\rightarrow$ Cập nhật OLED $\rightarrow$ Gửi chuỗi trạng thái phản hồi về điện thoại.
* **TC4 - Kiểm thử biên & Khả năng chịu lỗi:** Test ngắt kết nối Bluetooth đột ngột, lỗi đọc DHT11 (như timeout, sai Checksum), và gửi lệnh sai cú pháp.

### 📐 Phương pháp đánh giá kết quả
* Đo sai số nhiệt độ/độ ẩm của DHT11 so với thiết bị đo chuẩn.
* Đo độ trễ phản hồi (Response Latency) từ lúc bấm nút trên điện thoại đến khi Relay kích hoạt.
* Log và phân tích khung dữ liệu UART qua Serial Monitor / Logic Analyzer.
