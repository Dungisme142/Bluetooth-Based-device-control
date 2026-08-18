# HƯỚNG DẪN LÀM PROJECT BLUETOOTH-BASED DEVICE CONTROL

> ### ⚠️ Trạng thái tài liệu
>
> Phần phân công dưới đây là **kế hoạch ban đầu**, giữ lại để tra cứu ai phụ trách
> mảng nào. Bố cục file trong đó **không còn khớp với repo**: tầng `logic/` và
> `Drivers/Src/tim.c` đã bị gỡ (không nằm trong build, trùng chức năng với code
> thật), còn `onewire.c`, `gpio.c`, `HC05.c`, `MKE_M01_LED.c` thì chưa bao giờ tồn tại.
>
> Ánh xạ từ vai trò sang file đang chạy thật:
>
> | Vai trò | File thật |
> |---|---|
> | Cảm biến | `lib/{Inc,Src}/DHT11.c`, `Dht11_ReadOnce()` trong `src/main.c` |
> | Timer / ngắt | `Board_TIM2_Init()` trong `src/board.c`, toàn bộ vector trong `src/stm32f1xx_it.c`. Nhịp tác vụ chạy bằng `HAL_GetTick()` trong vòng lặp chính, không phải TIM2 |
> | Chấp hành | `lib/{Inc,Src}/MKE_M05_RELAY.c`, `Relay_SetState()` trong `src/main.c` |
> | UART / Bluetooth | `lib/Src/uart.c`, `Ring_Buffer.c`, `Text_Filter.c`, `Frame_Builder.c`, `Command_Selector.c`; bảng lệnh `Command_Menu[]` trong `src/main.c` |
> | OLED | `lib/Src/SSD1306.c`, `font5x7.c`, và `src/ui.c` (4 trang + 2 nút) |
>
> Kiến trúc hiện tại: `docs/project_docs/Project_Overview.md` §7. Bảng chân: `local/PIN_MAP.md`.

---

## 1. Phân công nhiệm vụ cho từng thành viên

Dự án này được chia theo tầng: driver, library, logic, test. Mỗi thành viên nên tập trung vào một nhóm file để tránh xung đột và dễ kiểm soát.

### 1.1. Hoàng Bùi Nghĩa Dũng
Vai trò: phụ trách cảm biến và đọc dữ liệu môi trường.

File chính cần làm:
- drivers/Inc/onewire.h
- drivers/Src/onewire.c
- lib/Inc/DHT11.h
- lib/Src/DHT11.c
- logic/Inc/SENSOR_TASK.h
- logic/Src/SENSOR_TASK.c

Kết quả mong đợi:
- Đọc dữ liệu nhiệt độ, độ ẩm từ DHT11 đúng.
- Xử lý lỗi timeout/checksum.
- Cung cấp dữ liệu cho các task khác.

### 1.2. Nguyễn Trọng Nhân
Vai trò: phụ trách timer, ngắt và định thời hệ thống.

File chính cần làm:
- drivers/Inc/tim.h
- drivers/Src/tim.c
- logic/Inc/TIMER_TASK.h
- logic/Src/TIMER_TASK.c

Kết quả mong đợi:
- Tạo các tick định kỳ cho task.
- Xử lý ngắt timer đúng.
- Cung cấp các cờ báo thời gian cho các task khác.

### 1.3. Trần Đình Ý
Vai trò: phụ trách cơ cấu chấp hành và chỉ báo trạng thái.

File chính cần làm:
- drivers/Inc/gpio.h
- drivers/Src/gpio.c
- lib/Inc/MKE_M01_LED.h
- lib/Src/MKE_M01_LED.c
- lib/Inc/MKE_M05_RELAY.h
- lib/Src/MKE_M05_RELAY.c
- logic/Inc/ACTUATOR_TASK.h
- logic/Src/ACTUATOR_TASK.c

Kết quả mong đợi:
- Điều khiển Relay bật/tắt.
- Chớp/tắt LED đúng trạng thái.
- Cung cấp API mức cao cho protocol task.

### 1.4. Nguyễn Tuấn Minh
Vai trò: phụ trách giao tiếp UART, Bluetooth HC-05 và giải mã lệnh.

File chính cần làm:
- drivers/Inc/uart.h
- drivers/Src/uart.c
- lib/Inc/HC05.h
- lib/Src/HC05.c
- logic/Inc/PROTOCOL_TASK.h
- logic/Src/PROTOCOL_TASK.c

File hỗ trợ nếu nhóm chia thêm màn hình OLED:
- lib/Inc/SSD1306.h
- lib/Src/SSD1306.c
- logic/Inc/DISPLAY_TASK.h
- logic/Src/DISPLAY_TASK.c

Kết quả mong đợi:
- Nhận và gửi dữ liệu qua UART/Bluetooth.
- Parse các lệnh điều khiển từ điện thoại.
- Trả lại phản hồi trạng thái cho người dùng.

### 1.5. File chung cần phối hợp cuối cùng
- src/main.c
- CMakeLists.txt
- build_and_flash.bat

Lưu ý: file main.c và file build system nên được chỉnh sau khi các module riêng đã hoàn thành để tích hợp lại.

---

## 2. Cách clone project về máy và bắt đầu làm việc

### 2.1. Clone repository
Mở terminal và chạy:

```bash
git clone <URL_REPOSITORY>
cd "Bluetooth-Based Device Control"
```

### 2.2. Tạo nhánh làm việc riêng
Mỗi thành viên nên tạo nhánh riêng trước khi code:

```bash
git checkout -b <ten-cua-ban>
```

Ví dụ:

```bash
git checkout -b dung-dht11
```

### 2.3. Mở project trong VS Code
- Mở thư mục project bằng VS Code.
- Xác nhận đang mở đúng folder: Bluetooth-Based Device Control.
- Nếu cần, cài đặt extension hỗ trợ C/C++, CMake Tools và ARM toolchain.

### 2.4. Cài đặt công cụ cần thiết trên Windows
Đảm bảo các phần mềm sau đã có trên máy:
- ARM GNU Toolchain: arm-none-eabi-gcc
- CMake
- Ninja
- STM32CubeProgrammer
- ST-Link driver

Kiểm tra bằng các lệnh:

```bash
arm-none-eabi-gcc --version
cmake --version
ninja --version
STM32_Programmer_CLI -h
```

Nếu thiếu tool, cài trước rồi thêm vào PATH.

---

## 3. Hướng dẫn build và flash bằng build_and_flash.bat

File batch này đã được viết sẵn để thực hiện cả 4 bước chính:
1. Xóa thư mục build cũ.
2. Cấu hình project bằng CMake.
3. Biên dịch firmware bằng Ninja.
4. Flash firmware vào STM32 qua ST-Link.

### 3.1. Chuẩn bị phần cứng
Trước khi flash, hãy đảm bảo:
- Board STM32F103C8T6 đã được kết nối đúng.
- ST-Link đã nối tới chân SWD.
- Board đã cấp nguồn.
- STM32_Programmer_CLI có thể tìm thấy thiết bị.

### 3.2. Chạy file batch
Trong terminal ở thư mục project, chạy:

```bash
build_and_flash.bat
```

### 3.3. Ý nghĩa từng bước trong file .bat
#### Bước 1: Xóa build cũ
- Xóa thư mục build cũ để tránh lỗi trùng file.

#### Bước 2: Cấu hình với CMake
- Chạy câu lệnh:

```bash
cmake -G "Ninja" -B build
```

#### Bước 3: Biên dịch firmware
- Chạy câu lệnh:

```bash
ninja -C build
```

- Nếu build thành công, file .bin sẽ được sinh ra trong thư mục build.
- Script sẽ tự copy file này thành:

```bash
build/app_firmware.bin
```

#### Bước 4: Flash vào chip
- Chạy lệnh:

```bash
STM32_Programmer_CLI -c port=SWD -w build/app_firmware.bin 0x08000000 -v -rst
```

- Nếu flash thành công, chip sẽ tự reset và chạy firmware mới.

### 3.4. Nếu build hoặc flash lỗi
Một số lỗi thường gặp:
- arm-none-eabi-gcc không được tìm thấy → kiểm tra PATH.
- CMake hoặc Ninja không tồn tại → cài lại.
- Không thấy board ST-Link → kiểm tra dây nối, driver, cổng USB.
- Lỗi flash → kiểm tra kết nối SWD và nguồn điện.

---

## 4. Hướng dẫn chạy testcase kiểm thử

Hiện tại thư mục test có 4 file mẫu:
- test/test1.c
- test/test2.c
- test/test3.c
- test/test4.c

### 4.1. Mục đích từng testcase
- test1.c: kiểm thử module ngoại vi độc lập.
- test2.c: kiểm thử tích hợp giữa 2 module.
- test3.c: kiểm thử toàn hệ thống.
- test4.c: kiểm thử biên/regression.

### 4.2. Bước 1: Viết nội dung testcase
Mỗi file test cần được điền logic kiểm thử phù hợp với module tương ứng.

Ví dụ:
- test1: kiểm tra DHT11, Relay, LED, UART riêng lẻ.
- test2: kiểm tra giao tiếp giữa UART và Relay/LED.
- test3: kiểm tra luồng đầy đủ: nhận lệnh → điều khiển relay → đọc cảm biến → hiển thị → phản hồi.
- test4: kiểm tra trường hợp lỗi, như mất kết nối Bluetooth, timeout DHT11, lệnh sai cú pháp.

### 4.3. Bước 2: Thêm target test vào CMake
Hiện tại file CMakeLists.txt đang trống, nên cần bổ sung các target test nếu muốn build trực tiếp bằng CMake.

Ví dụ ý tưởng:
- Tạo target riêng cho mỗi test.
- Nếu dùng test trên máy tính (host), có thể build bằng GCC.
- Nếu dùng trên board, nên flash từng bản firmware test riêng.

### 4.4. Bước 3: Build project
Sau khi đã viết testcase, chạy lại:

```bash
cmake -G "Ninja" -B build
ninja -C build
```

### 4.5. Bước 4: Chạy test
Nếu testcase được build thành file chạy được trên máy tính, chạy như sau:

```bash
./build/test1
./build/test2
./build/test3
./build/test4
```

Nếu testcase được dùng cho board thật, thì:
- flash firmware test vào STM32 bằng build_and_flash.bat,
- quan sát kết quả trên Serial Monitor hoặc qua Bluetooth Terminal.

### 4.6. Ghi nhận kết quả
Sau mỗi test, cần lưu lại:
- kết quả pass/fail,
- lỗi gặp phải,
- thời gian phản hồi,
- dữ liệu ghi nhận.

---

## 5. Quy ước làm việc nhóm
- Mỗi người chỉ làm file thuộc phần mình.
- Trước khi push code, chạy lại build để tránh lỗi tích hợp.
- Khi sửa file dùng chung như main.c hoặc CMakeLists.txt, cần thông báo trước cho cả nhóm.
- Mỗi commit nên có nội dung rõ ràng, ví dụ:
  - "Add DHT11 read function"
  - "Implement timer task"
  - "Add UART protocol parser"

---

## 6. Checklist trước khi nộp bài
- [ ] Code build thành công.
- [ ] Firmware flash thành công lên board.
- [ ] Testcase chạy được và có kết quả rõ ràng.
- [ ] Các file .h/.c đã được đặt đúng thư mục.
- [ ] README hoặc project note đã được cập nhật đủ.
