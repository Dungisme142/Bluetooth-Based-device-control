# 12 — Hạn chế và hướng phát triển

## 12.1 Hạn chế đã biết

Liệt kê trung thực những chỗ hệ thống chưa hoàn chỉnh. Mỗi mục ghi rõ **hệ quả thực tế** và
**hướng khắc phục**, để người đọc tự đánh giá được mức độ nghiêm trọng.

### L-01 · Chế độ `AUTO` mới là stub

Lệnh `AUTO` có trong bảng lệnh và trả lời `AUTO_MODE_READY`, nhưng **không điều khiển gì**
theo ngưỡng cảm biến. Handler chỉ có đúng một dòng `snprintf` (`main.c:485-489`).

*Hệ quả*: người dùng có thể tưởng đã bật chế độ tự động trong khi không có gì xảy ra.
*Khắc phục*: xem §12.3 mục 1.

### L-02 · Không lưu trạng thái qua reset

Không ghi flash, không EEPROM. Mất điện rồi có lại là cả 5 kênh về TẮT.

*Hệ quả*: sau mỗi lần cúp điện, người dùng phải bật lại thủ công.
*Đây vừa là hạn chế vừa là lựa chọn có chủ ý*: TẮT là trạng thái an toàn cho thiết bị công
suất. Nếu bổ sung tính năng khôi phục thì phải cho người dùng chọn bật/tắt nó.

### L-03 · Không có bảo mật trên kênh Bluetooth

Không xác thực, không mã hoá ở lớp ứng dụng. Ai ghép cặp được với module là điều khiển được
toàn bộ 5 kênh.

*Hệ quả*: chỉ chấp nhận được trong phạm vi một đồ án/thiết bị cá nhân. Không dùng cho thiết
bị đặt nơi công cộng.

### L-04 · `UART_Print()` bỏ bản tin khi đang bận gửi

```c
if (developer_uart_handler->hal_huart->gState != HAL_UART_STATE_READY) {
    return;      /* uart.c:204-206 */
}
```

Bộ đệm phát chỉ có một; ghi đè nó lúc `Transmit_IT` còn đang đọc sẽ làm câu đang gửi bị lẫn.
Giải pháp hiện tại là **bỏ hẳn bản tin mới**.

*Hệ quả*: gửi lệnh dồn dập, hoặc một bản tin trạng thái định kỳ rơi đúng lúc đang trả lời
lệnh, thì một trong hai bị mất — âm thầm, không có dấu hiệu gì.
*Khắc phục*: dùng một bộ đệm vòng cho đường phát thay vì một bộ đệm phẳng, đẩy byte tiếp
theo trong `HAL_UART_TxCpltCallback`.

### L-05 · Nhật ký bị `DHT OK` lấp đầy trong 24 giây

Nhật ký giữ 12 dòng, mà `DHT OK`/`DHT FAIL` được ghi mỗi 2 giây → sau ~24 giây mọi sự kiện
`OUTn ON/OFF` và `BT LINK UP` đều bị đẩy ra.

*Hệ quả*: trang LOG hầu như chỉ toàn `DHT OK`, giá trị chẩn đoán thấp.
*Khắc phục*: chỉ ghi log khi kết quả đọc cảm biến **đổi trạng thái** (OK→FAIL hoặc
FAIL→OK), thay vì mỗi lần đọc.

### L-06 · Vẽ OLED chặn vòng lặp ~25 ms

`SSD1306_UpdateScreen()` đẩy 1 KB qua I2C blocking. Xem
[09 — Timing và ngắt](09-timing-va-ngat.md) §9.5 để biết phân tích đánh đổi.

*Hệ quả*: chấp nhận được ở quy mô hiện tại; sẽ thành vấn đề nếu thêm tác vụ chặn khác.
*Khắc phục*: chuyển sang I2C + DMA.

### L-07 · Chưa có unit test tự động

`stm-firmware/test/` hiện chỉ có một `README.md` mô tả việc còn thiếu, không có framework,
và thư mục này **không nằm trong danh sách build của CMake**. Toàn bộ việc kiểm chứng là
thủ công theo
[11 — Kế hoạch kiểm thử](11-ke-hoach-kiem-thu.md).

*Hệ quả*: mỗi lần sửa code phải chạy lại 79 ca thủ công mới yên tâm — trên thực tế không ai
làm vậy, nên hồi quy dễ lọt.
*Khắc phục*: xem §12.2.

### L-08 · Nút OK có tác dụng ở cả trang không hiển thị con trỏ

`UI_HandleEvent()` chỉ coi trang LOG là ngoại lệ; ở trang SENSOR và HƯỚNG DẪN, bấm OK vẫn
bật/tắt kênh đang chọn dù màn hình không hề hiển thị kênh nào đang được chọn.

*Hệ quả*: bấm nhầm ở trang HƯỚNG DẪN có thể bật một thiết bị mà người dùng không biết.
*Khắc phục*: bỏ qua sự kiện OK ở SENSOR và HƯỚNG DẪN, hoặc hiển thị con trỏ trên hai trang
đó.

### ~~L-09 · Sai chữ hoa/thường tên linker script~~ ✅ ĐÃ SỬA

`cmake/gcc-arm-none-eabi.cmake:39` từng ghi `STM32F103XX_FLASH.ld` trong khi file thật là
`STM32F103xx_FLASH.ld` — Windows chạy được, Linux/macOS fail ở bước link. Đã sửa cho khớp.

### L-10 · Điểm cần xác nhận về mức áp chân DHT11

PA4 không phải chân 5 V tolerant, trong khi J6 cấp `+5V` cho cảm biến. Nếu module DHT11 có
điện trở kéo lên riêng tới 5 V thì PA4 bị quá áp. Xem
[03 — Thiết kế phần cứng](03-thiet-ke-phan-cung.md) §3.6.

*Hệ quả*: nếu đúng, chân PA4 có thể hỏng dần theo thời gian dù mạch vẫn chạy được lúc đầu.
*Khắc phục*: đo mức HIGH thực tế trên PA4 bằng đồng hồ. Nếu > 3,6 V thì đổi J6 sang `+3.3V`.

### ~~L-11 · Comment lạc hậu trong code~~ ✅ ĐÃ SỬA

Comment của `Set_Output()` từng ghi *"Kênh 0 (relay PB12) kéo theo LED chỉ báo PB13"* — mô
tả kiến trúc cũ trước đợt refactor bỏ relay, sai so với bảng chân hiện tại (OUT-1 = PA8,
PB13 = OUT-4). Đã viết lại cho đúng.

## 12.2 Hướng phát triển — chất lượng code

### 1. Khung unit test chạy trên máy tính

Ưu tiên cao nhất. Nhiều module trong `lib/` là **C thuần, không phụ thuộc phần cứng** nên
biên dịch và test được ngay trên PC bằng gcc thường:

| Module | Phụ thuộc HAL? | Test được ngay? |
|---|---|---|
| `Ring_Buffer.c` | Không | ✅ |
| `Command_Selector.c` | Không | ✅ |
| `uart.c` — phần `Text_Filting`/`Frame_Building` | Chỉ ở phần I/O | ✅ sau khi tách |
| `Digital_Out.c` | Có | Cần giả lập HAL |
| `DHT11.c` | Có (timer + GPIO) | Cần giả lập HAL |
| `SSD1306.c` | Có (I2C) | Cần giả lập HAL |

Đề xuất: dùng **Unity** (chỉ 3 file, không cần cài đặt), thêm một `CMakeLists.txt` riêng cho
build host với `enable_testing()` + CTest. Danh sách ca test cụ thể đã liệt kê ở
[11](11-ke-hoach-kiem-thu.md) §11.12.

### 2. Tách `main.c` (818 dòng) thành các module

Hiện `main.c` chứa cả khởi tạo ngoại vi, trạng thái hệ thống, bảng lệnh, đọc cảm biến và
vòng lặp. Tách hợp lý:

- `board.c` — `SystemClock_Config` + toàn bộ `MX_*_Init` + các hàm MSP
- `app_command.c` — `Command_Menu[]` và các handler
- `app_state.c` — trạng thái hệ thống + `Set_Output` + `Format_Status`
- `main.c` — chỉ còn thứ tự khởi tạo và vòng lặp

*Chỉ nên làm sau khi có unit test*, nếu không thì không có cách nào biết việc tách có làm
gãy gì không.

### 3. CI đơn giản

Một workflow GitHub Actions chạy `cmake + ninja` trên mỗi push đã đủ bắt được lỗi build và
lỗi hoa/thường tên file (L-09). Thêm bước chạy unit test khi có.

## 12.3 Hướng phát triển — tính năng

### 1. Hoàn thiện chế độ AUTO

Cho phép gắn ngưỡng nhiệt độ/độ ẩm vào một kênh:

```
AUTO 1 TEMP>30     → kênh 1 tự bật khi nhiệt độ vượt 30 °C
AUTO 2 HUM<40      → kênh 2 tự bật khi độ ẩm dưới 40 %
AUTO OFF           → tắt toàn bộ chế độ tự động
AUTO STATUS        → liệt kê các luật đang có hiệu lực
```

Cần thêm: một bảng luật (5 mục), kiểm tra luật sau mỗi phép đo cảm biến, và **độ trễ trễ
(hysteresis)** để kênh không dao động liên tục quanh ngưỡng. Trang UI nên thêm ký hiệu phân
biệt kênh đang ở chế độ tự động với kênh đang bị điều khiển tay.

### 2. Lưu trạng thái vào flash

Ghi `output_on[]` vào trang flash cuối (STM32F103C8 có trang 1 KB). Cần chú ý:

- Chỉ ghi khi trạng thái **thực sự đổi**, và nên gộp nhiều thay đổi liền nhau (flash chỉ
  chịu được ~10.000 chu kỳ xoá).
- Xoá trang là thao tác **chặn ~20 ms** — phải cân nhắc với NFR-03.
- Phải có tuỳ chọn tắt tính năng: khôi phục trạng thái sau mất điện không phải lúc nào cũng
  an toàn với thiết bị công suất.

### 3. Watchdog độc lập (IWDG)

Hiện `Error_Handler()` treo vĩnh viễn với ngắt bị tắt — chỉ có reset tay mới cứu được. Bật
IWDG với timeout ~2 giây, nạp lại nó ở cuối mỗi vòng lặp, sẽ khiến thiết bị tự khởi động lại
thay vì chết cứng. Lưu ý: timeout phải lớn hơn trường hợp xấu nhất của vòng lặp (~525 ms,
xem [09](09-timing-va-ngat.md) §9.2).

### 4. I2C bằng DMA

Giải phóng ~25 ms mỗi khung hình khỏi vòng lặp chính, mở đường cho việc vẽ mượt hơn hoặc
thêm tác vụ mới.

### 5. Đường phát UART có bộ đệm vòng

Khắc phục L-04, đồng thời cho phép gửi bản tin dài hơn 256 byte.

### 6. Mở rộng số kênh

Kiến trúc đã sẵn sàng: thêm một khối `OUTn_*` trong `pin_config.h`, tăng `OUT_COUNT`, thêm
một dòng vào `outputs[]` (`main.c`) và `ui_outputs[]` (`ui.c`). Giới hạn thực tế:

- Trang OUTPUTS hiển thị được **5 dòng** một lúc → quá 5 kênh phải thêm cuộn trang.
- Trang HOME có 5 ô với bước 22 px → quá 5 kênh phải đổi bố cục.
- Bộ tách tham số lệnh chỉ nhận **một chữ số** (`args[1] != '\0'`) → quá 9 kênh phải sửa
  `Command_SetOutputs()`.

### 7. Bảo mật cơ bản

Thêm lệnh `PIN <mã>` phải gọi thành công trước khi các lệnh điều khiển được chấp nhận, kèm
timeout tự khoá lại. Không phải bảo mật thật sự (kênh vẫn không mã hoá) nhưng chặn được
việc điều khiển vô tình từ thiết bị đã từng ghép cặp.

## 12.4 Thứ tự đề xuất

| Ưu tiên | Việc | Công sức | Lý do |
|---|---|---|---|
| ~~0~~ | ~~L-09 (tên linker script) và L-11 (comment lạc hậu)~~ | — | ✅ Đã xong |
| 1 | Xác nhận L-10 bằng đồng hồ đo | ~15 phút | Rủi ro phần cứng, cần biết chắc trước khi chạy lâu dài |
| 2 | Sửa L-05 (chỉ log khi cảm biến đổi trạng thái) | ~30 phút | Trang LOG lập tức hữu ích hơn |
| 3 | Sửa L-08 (bỏ qua OK ở trang không có con trỏ) | ~30 phút | Tránh bật nhầm thiết bị |
| 4 | Khung unit test (§12.2 mục 1) | 1–2 ngày | Điều kiện tiên quyết cho mọi refactor về sau |
| 5 | Hoàn thiện AUTO (§12.3 mục 1) | 2–3 ngày | Tính năng còn dang dở duy nhất |
| 6 | Tách `main.c` (§12.2 mục 2) | 1 ngày | Chỉ làm sau khi có test |
