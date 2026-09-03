# 02 — Đặc tả yêu cầu (SRS)

Mỗi yêu cầu có một mã để [Kế hoạch kiểm thử](11-ke-hoach-kiem-thu.md) tham chiếu ngược.
Cột "Hiện thực" chỉ ra nơi trong code chịu trách nhiệm cho yêu cầu đó.

## 2.1 Yêu cầu chức năng (FR)

### Điều khiển thiết bị

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-01** | Hệ thống điều khiển được **4 kênh ngõ ra độc lập** (OUT1–OUT4 trên PB15..PB12). Mỗi kênh bật/tắt riêng, không ảnh hưởng kênh khác. Chân PA8 cũ được giải phóng thành chân cảnh báo tự động ALARM. | `OUT_COUNT` = 4 (`pin_config.h`), `outputs[]` (`main.c`) |
| **FR-02** | Người dùng bật/tắt một kênh bất kỳ bằng lệnh Bluetooth có tham số số kênh (`ON 3`, `OFF 4`). Số kênh hợp lệ là `1`..`4`. | `Command_SetOutputs()` (`main.c`) |
| **FR-03** | Người dùng bật/tắt **cả 4 kênh cùng lúc** bằng một lệnh (`ON ALL`, `OFF ALL`). | `main.c` |
| **FR-04** | Lệnh `ON`/`OFF` không tham số tác động lên kênh 1 (OUT1 trên PB15). | `main.c` |
| **FR-05** | Người dùng bật/tắt được kênh **tại chỗ bằng nút bấm** sau khi đã đăng nhập Local, không cần điện thoại. | Nút OK → `UI_Request_t` → `Set_Output()` |
| **FR-06** | Hai đường điều khiển (Bluetooth và nút bấm) phải hội tụ về **cùng một trạng thái**: bật bằng nút thì `STATUS` phải báo BẬT, và ngược lại. | Mọi lối vào đều gọi `Set_Output()` (`main.c`) |
| **FR-07** | Số kênh sai (`ON 5`, `ON 9`, `ON 1X`, `ON abc`) phải bị từ chối rõ ràng, không được âm thầm tác động nhầm kênh. | `Command_SetOutputs()` → `BAD_CHANNEL` |

### Giao tiếp Bluetooth

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-08** | Hệ thống nhận lệnh dạng văn bản ASCII qua Bluetooth SPP và trả lời **từng lệnh một**. | `UART_Task()` (`uart.c`) |
| **FR-09** | Hệ thống tự **phát trạng thái đầy đủ mỗi 3 giây** mà không cần được hỏi. | `STATUS_PERIOD_MS` = 3000 (`main.c`) |
| **FR-10** | Lệnh `STATUS` trả về **đúng cùng một chuỗi** với bản tin tự phát theo định dạng: `TEMP=… HUM=… DHT=OK/FAIL BT=OK/NO AUTH=NONE/LOCAL/BLE ALARM=NORMAL/HIGH/DHT_FAULT OUT=0000`. | Cả hai gọi `Format_Status()` (`main.c`) |
| **FR-11** | Hệ thống đọc riêng được nhiệt độ (`TEMP`) và độ ẩm (`HUM`) ở bất kỳ trạng thái khóa nào. | `main.c` |
| **FR-12** | Lệnh không nằm trong bảng lệnh phải nhận về thông báo lỗi, không được im lặng. | `Command_Selecting()` → `Invalid Command` |
| **FR-13** | Hệ thống chấp nhận cả CR, LF và CRLF làm ký tự kết thúc lệnh, **và** tự chốt lệnh sau 250 ms im lặng nếu app không gửi ký tự kết thúc nào. | `Text_Filting()` (`uart.c`), `UART_FRAME_IDLE_TIMEOUT_MS` (`uart.h`) |
| **FR-14** | Hệ thống phát hiện được liên kết Bluetooth vừa lên (byte đầu tiên nhận được) và vừa mất (im lặng 10 giây). Đồng bộ trạng thái từ UART driver và ghi log `BT LINK UP` / `BT LINK DOWN`. | `Developer_UART_HandleTypeDef`, `main.c` |
| **FR-15** | Hệ thống gửi một dòng chào ngay sau khi khởi động để người dùng biết firmware đã boot. | `"MKE-M15 ready\r\n"` (`main.c`) |

### Cảm biến & Sức khỏe hệ thống

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-16** | Hệ thống đo nhiệt độ và độ ẩm bằng DHT11, **chu kỳ 2 giây**. | `SENSOR_PERIOD_MS` = 2000 (`main.c`) |
| **FR-17** | Phép đo sai checksum hoặc quá hạn phải bị loại bỏ, **không được cập nhật vào số liệu hiển thị**. | `DHT11_ReadOnce()` (`main.c`) |
| **FR-18** | Người dùng phải nhận biết được trạng thái sức khỏe cảm biến (DHT Health). Khi cảm biến lỗi/mất tín hiệu, hệ thống ghi log `DHT BAD`, thanh tiêu đề mọi trang OLED hiện huy hiệu `!DHT`, trang HOME hiện `DHT BAD` (thay vì giữ `DHT OK`), và trang SENSOR hiện `DHT BAD` kèm thời gian `LAST OK`. | `health_monitor.c`, `ui.c` |

### Giao diện tại chỗ

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-19** | Khi chưa đăng nhập Local, OLED chỉ hiển thị **Màn hình khóa (Locked Dashboard)** tổng hợp telemetry và không cho điều khiển. Sau khi đăng nhập thành công, OLED mở **5 trang** tiêu chuẩn: HOME, OUTPUTS, SENSOR, LOG, HƯỚNG DẪN (cho 4 ngõ ra). | `ui.c` |
| **FR-20** | Khi ở giao diện mở khóa: Hai nút chuyển trang tới/lui theo vòng tròn; hai nút di chuyển con trỏ; một nút xác nhận. | `UI_HandleEvent()` (`ui.c`) |
| **FR-21** | Hệ thống lưu và hiển thị **12 sự kiện gần nhất** kèm mốc thời gian, cuộn xem được bằng nút. | `UI_LOG_LINES` = 12 (`ui.c`) |
| **FR-22** | Nút bấm phải được chống dội: một cú bấm sinh đúng một sự kiện. | `UI_SampleButton()` (`ui.c`) |
| **FR-23** | Thanh tiêu đề của mọi trang cho biết trạng thái kết nối Bluetooth, vị trí trang và **huy hiệu cảnh báo lỗi sức khỏe `!DHT`** khi cảm biến gặp sự cố. | `UI_DrawHeader()` (`ui.c`) |

### Vận hành & An toàn

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-24** | LED nhịp tim nhấp nháy chu kỳ 1 giây, làm dấu hiệu nhìn-là-biết vòng lặp chính còn sống. | `HEARTBEAT_PERIOD_MS` = 1000 (`main.c`) |
| **FR-25** | Sau khi cấp nguồn hoặc reset, **cả 4 kênh ngõ ra và chân ALARM phải ở trạng thái TẮT** trước khi bất kỳ tín hiệu nào ra tới chân cắm. | `MX_GPIO_Init()` (`main.c`) |

### Xác thực, Phiên độc quyền & Cảnh báo tự động

| Mã | Yêu cầu | Hiện thực |
|---|---|---|
| **FR-26** | Mã PIN xác thực cố định 4 chữ số `"1234"`. Không lưu Flash, không hỗ trợ đổi PIN và không khóa vĩnh viễn (lockout) sau nhiều lần nhập sai. | `auth.h` / `auth.c` |
| **FR-27** | Quản lý phiên đăng nhập độc quyền (Exclusive Session) gồm 3 trạng thái: `AUTH_NONE`, `AUTH_LOCAL`, `AUTH_BLE`. Tại một thời điểm chỉ có tối đa một bên nắm quyền điều khiển thiết bị. | `auth.c` |
| **FR-28** | Dữ liệu telemetry (nhiệt độ, độ ẩm, trạng thái DHT, Bluetooth, trạng thái 4 kênh ngõ ra và chuỗi `STATUS`) luôn xem được mà không cần đăng nhập. Lệnh điều khiển GPIO (`ON`, `OFF`) chỉ được thực thi bởi chủ phiên hiện tại; kẻ khác nhận thông báo lỗi `ERR_LOCKED`. | `main.c`, `Command_SetOutputs()` |
| **FR-29** | Thời gian chờ (Timeout): Bàn phím số ảo tự thoát về Dashboard khóa sau **30 giây** không thao tác. Phiên đăng nhập Local và BLE tự động kết thúc sau **60 giây** không có hoạt động (hoạt động Local là bất kỳ lần nhấn phím nào; hoạt động BLE là bất kỳ lệnh hợp lệ nào). | `ui.c`, `auth.c` |
| **FR-30** | Quyền ưu tiên Local và Kick BLE: Khi người dùng đăng nhập Local thành công, nếu BLE đang giữ phiên thì quyền của BLE lập tức bị thu hồi ở tầng ứng dụng (không ngắt kết nối Bluetooth vật lý). Hệ thống xếp hàng và phát bản tin bắt buộc `LOCAL LOGIN - KICKED\r\n` qua UART ngay khi bus sẵn sàng. BLE không thể đăng nhập lại (`LOGIN_BUSY_LOCAL`) cho tới khi Local hết hạn. | `auth.c`, `main.c` |
| **FR-31** | Chân cảnh báo độ ẩm tự động **PA8** (active-high) hoạt động độc lập với quyền phiên theo 3 trạng thái: (1) Bình thường (độ ẩm <= 90%): Tắt; (2) Độ ẩm cao (> 90%): Bật liên tục; (3) Lỗi cảm biến hoặc chưa có mẫu hợp lệ: Đảo mức nhấp nháy chu kỳ 250 ms. PA8 không nằm trong danh sách điều khiển GPIO thủ công. | `main.c`, `Alarm_Task()` |
| **FR-32** | Giám sát sức khỏe hệ thống (System Health Monitor): Quản lý mặt nạ lỗi `FAULT_DHT11_DEAD`, kích hoạt còi/LED cảnh báo PA8 nhấp nháy 250 ms, hiển thị huy hiệu `!DHT` trên thanh tiêu đề mọi trang OLED, hiển thị hộp nổi bật `DHT BAD` trên trang HOME, và ghi nhận `DHT=BAD`/`FAIL` trong telemetry. Khi cảm biến hồi phục ở mẫu đọc kế tiếp, hệ thống lập tức thoát trạng thái lỗi. | `health_monitor.c`, `ui.c`, `main.c` |

## 2.2 Yêu cầu phi chức năng (NFR)

| Mã | Yêu cầu | Giá trị / căn cứ |
|---|---|---|
| **NFR-01** | Tốc độ đường truyền Bluetooth cố định 9600 baud, 8 bit dữ liệu, không parity, 1 stop bit. | Mặc định xuất xưởng của MKE-M15 (`pin_config.h:176`) |
| **NFR-02** | Firmware là bare-metal superloop, **không dùng RTOS**. | Yêu cầu của khoá học |
| **NFR-03** | Chỉ được có **một tác vụ chặn dài** trong vòng lặp (đọc DHT11, tối đa 500 ms). Thêm tác vụ chặn thứ hai là vi phạm. | `DHT11_POLL_TIMEOUT_MS` (`main.c:36`) |
| **NFR-04** | Một khung hình OLED chặn khoảng 25 ms; màn hình vẽ lại tối đa 2 lần/giây khi không có thao tác. | `UI_REFRESH_PERIOD_MS` = 500 (`ui.c:51`) |
| **NFR-05** | Bộ đệm nhận UART phải đủ chứa lượng byte đổ về trong lúc vòng lặp bị chặn lâu nhất: 500 ms × 9600 baud ≈ 500 byte lý thuyết, thực tế lệnh chỉ dài vài chục byte → **128 byte**. | `UART_RX_BUFFER_SIZE` (`main.c:38`) |
| **NFR-06** | Thời gian chống dội phím 25 ms — đủ dập nảy tiếp điểm (5–10 ms) mà vẫn nhận được cú chạm nhanh. | `BTN_DEBOUNCE_MS` (`pin_config.h:155`) |
| **NFR-07** | Firmware phải vừa **64 KB flash / 20 KB RAM** của STM32F103C8T6. Hiện dùng ~28 KB flash (43%) và ~4,2 KB RAM (21%) ở bản Debug. | `arm-none-eabi-size` trên `firmware.elf` |
| **NFR-08** | **Không cấp phát động** sau khi khởi tạo: mọi bộ đệm là biến tĩnh. | Không có `malloc` trong `src/` và `lib/` |
| **NFR-09** | Mọi so sánh thời gian dùng hiệu `now - last`, an toàn khi `HAL_GetTick()` tràn sau ~49,7 ngày. | `main.c:204-206` |
| **NFR-10** | Toàn bộ bảng chân tập trung ở **một file duy nhất** `lib/Inc/pin_config.h`; driver không được hardcode port/pin/IRQn. | Quy tắc ghi ở đầu `pin_config.h` |
| **NFR-11** | Ngắt của DHT11 phải có mức ưu tiên **cao hơn** UART; ngắt nút bấm thấp nhất. | Xem [09 — Timing và ngắt](09-timing-va-ngat.md) |
| **NFR-12** | ISR không được gọi I2C, không được ghi nhật ký, không được đổi trạng thái thiết bị. | Quy ước ghi trong `ui.h:10-16` |
| **NFR-13** | Code biên dịch sạch với `-Wall -Wextra -Wpedantic`. | `cmake/gcc-arm-none-eabi.cmake:29` |

## 2.3 Ràng buộc

| Mã | Ràng buộc |
|---|---|
| **C-01** | Vi điều khiển cố định là STM32F103C8T6 (Blue Pill), clock 72 MHz từ thạch anh HSE 8 MHz. |
| **C-02** | Không có cổng serial nào nối trực tiếp tới máy tính. Mọi log đi qua đường Bluetooth; gỡ lỗi sâu phải dùng SWD + GDB. |
| **C-03** | Project viết tay, **không có file `.ioc`** — không được sinh lại code bằng STM32CubeMX vì sẽ ghi đè cấu trúc hiện tại. |
| **C-04** | PA13/PA14 dành riêng cho SWD, không được dùng vào việc khác (mất luôn khả năng nạp/debug). |
| **C-05** | Bus DHT11 là 1-wire hai chiều: chân PA4 phải đổi qua lại giữa output open-drain và input EXTI lúc chạy. |

## 2.4 Giả định

| Mã | Giả định |
|---|---|
| **A-01** | Mỗi kênh ngõ ra nối tới một **module công suất bên ngoài** qua connector 3 chân (SIG/GND/VCC). Board không tự đóng cắt tải. |
| **A-02** | Module ngoài tác động **mức CAO** (`OUTn_ON_STATE = GPIO_PIN_SET`). Nếu dùng module tác động mức thấp, chỉ cần đảo macro tương ứng trong `pin_config.h`, không sửa logic. |
| **A-03** | Module MKE-M15 đã được cấu hình ở baud mặc định 9600 và đã ghép cặp với điện thoại trước khi dùng. |
| **A-04** | Điện thoại chạy một app SPP terminal thông thường (ví dụ *Serial Bluetooth Terminal* trên Android). Không cần app riêng. |
| **A-05** | Nguồn 5 V cấp cho board đủ dòng cho cả MCU, module Bluetooth, OLED và 5 LED chỉ báo. |
