# 06 — Giao thức Bluetooth

Tài liệu này đủ để viết một app điều khiển mà không cần đọc firmware.

## 6.1 Lớp vật lý

| Thuộc tính | Giá trị |
|---|---|
| Module | MKE-M15, hoạt động như cổng nối tiếp qua Bluetooth (SPP) |
| Tốc độ | **9600 baud** |
| Khung UART | 8 bit dữ liệu, không parity, 1 stop bit (**8N1**) |
| Flow control | Không |
| Mã hoá ký tự | ASCII |
| Bảo mật | Không xác thực, không mã hoá ở lớp ứng dụng |

Phía điện thoại chỉ cần một app SPP terminal thông thường (ví dụ *Serial Bluetooth
Terminal* trên Android).

## 6.2 Cú pháp lệnh

```
<LỆNH>[ <THAM_SỐ>]<ký tự kết thúc>
```

| Quy tắc | Chi tiết |
|---|---|
| **Phân biệt hoa thường** | `ON` hợp lệ, `on` **không** hợp lệ |
| **Dấu phân cách** | Đúng **một dấu cách** giữa lệnh và tham số |
| **Ký tự kết thúc** | `CR` (`\r`), `LF` (`\n`), hoặc `CRLF` — chấp nhận cả ba |
| **Không có ký tự kết thúc** | Sau **250 ms** im lặng, khung đang dở được tự chốt và thực thi |
| **Độ dài tối đa** | 128 byte một khung; dài hơn → `Fail, try again!` |
| **Backspace** | `\b` xoá lùi một ký tự trong khung đang dựng |
| **Khung rỗng** | Bị bỏ qua lặng lẽ (xảy ra với CRLF: `\r` chốt lệnh, `\n` chốt khung rỗng) |
| **Khớp lệnh** | Khớp tiền tố **rồi** yêu cầu ngay sau đó là hết chuỗi hoặc một dấu cách — nên `ONLINE` bị từ chối, không bị hiểu thành `ON` |

## 6.3 Bảng lệnh

| Lệnh | Tham số | Quyền tối thiểu | Tác dụng | Trả lời |
|---|---|---|---|---|
| `LOGIN` | `<PIN>` | Bất kỳ | Đăng nhập phiên điều khiển BLE (PIN cố định `"1234"`) | `LOGIN_OK` (thành công)<br/>`LOGIN_FAIL` (sai PIN hoặc sai định dạng)<br/>`LOGIN_BUSY_LOCAL` (Local đang giữ phiên) |
| `LOGOUT` | (không) | Chủ BLE | Đăng xuất phiên điều khiển hiện tại của BLE | `LOGOUT_OK` (nếu là chủ BLE)<br/>`ERR_NOT_OWNER` (nếu không phải chủ) |
| `ON` | (không) | Chủ BLE | Bật kênh **1** (OUT1 trên PB15) | `OUT1_ON` (hoặc `ERR_LOCKED`) |
| `ON n` | `1`..`4` | Chủ BLE | Bật kênh `n` | `OUTn_ON` (hoặc `ERR_LOCKED`) |
| `ON ALL` | `ALL` | Chủ BLE | Bật cả 4 kênh | `ALL_ON` (hoặc `ERR_LOCKED`) |
| `OFF` | (không) | Chủ BLE | Tắt kênh **1** (OUT1 trên PB15) | `OUT1_OFF` (hoặc `ERR_LOCKED`) |
| `OFF n` | `1`..`4` | Chủ BLE | Tắt kênh `n` | `OUTn_OFF` (hoặc `ERR_LOCKED`) |
| `OFF ALL` | `ALL` | Chủ BLE | Tắt cả 4 kênh | `ALL_OFF` (hoặc `ERR_LOCKED`) |
| `STATUS` | — | Bất kỳ | Trạng thái đầy đủ của hệ thống | xem §6.6 |
| `TEMP` | — | Bất kỳ | Nhiệt độ hiện tại | `TEMP=27C` |
| `HUM` | — | Bất kỳ | Độ ẩm hiện tại | `HUM=61%` |
| `AUTO` | — | Bất kỳ | ⚠️ **Chưa triển khai** — chỉ trả lời, không đổi hành vi | `AUTO_MODE_READY` |

Mọi câu trả lời kết thúc bằng `\r\n`.

Bất kỳ lệnh hợp lệ nào gửi từ BLE khi đang nắm quyền phiên (`AUTH_BLE`) đều tự động gia hạn bộ đếm thời gian phiên (60 giây).

## 6.4 Ma trận phân quyền lệnh

Hệ thống hoạt động theo mô hình phiên độc quyền với ba trạng thái quyền:

| Lệnh | `AUTH_NONE` (Chưa đăng nhập) | `AUTH_LOCAL` (Local đang đăng nhập) | `AUTH_BLE` (BLE đang giữ phiên) |
|---|---|---|---|
| `STATUS`, `TEMP`, `HUM` | Cho phép | Cho phép | Cho phép |
| `LOGIN 1234` | Thành công → chuyển `AUTH_BLE` | Từ chối (`LOGIN_BUSY_LOCAL`) | Đã đăng nhập (`LOGIN_OK`) |
| `LOGIN <sai>` | `LOGIN_FAIL` | `LOGIN_FAIL` (hoặc `LOGIN_BUSY_LOCAL`) | `LOGIN_FAIL` |
| `LOGOUT` | Từ chối (`ERR_NOT_OWNER`) | Từ chối (`ERR_NOT_OWNER`) | Thành công → chuyển `AUTH_NONE` |
| `ON`, `OFF` (kênh 1..4, ALL) | Từ chối (`ERR_LOCKED`) | Từ chối (`ERR_LOCKED`) | Cho phép thực thi |

## 6.5 Thông báo lỗi

| Trả lời | Nguyên nhân |
|---|---|
| `ERR_LOCKED` | Gửi lệnh điều khiển GPIO (`ON`, `OFF`) khi BLE chưa đăng nhập hoặc khi Local đang giữ phiên độc quyền. |
| `LOGIN_FAIL` | Mã PIN gửi kèm lệnh `LOGIN` không đúng `"1234"` hoặc không đủ 4 chữ số. |
| `LOGIN_BUSY_LOCAL` | BLE cố gắng đăng nhập trong lúc người dùng tại chỗ (Local) đang đăng nhập và giữ phiên. |
| `ERR_NOT_OWNER` | Gửi lệnh `LOGOUT` khi BLE không phải là chủ sở hữu phiên hiện tại. |
| `BAD_CHANNEL` | Tham số của `ON`/`OFF` không phải `ALL` và không phải đúng một chữ số trong `1`..`4`. Ví dụ: `ON 5`, `ON 9`, `ON 0`, `ON 12`, `ON 1X`, `ON abc`. |
| `Invalid Command` | Không lệnh nào trong bảng khớp. Ví dụ: `ONLINE`, `on 1`, `RESET`. |
| `Fail, try again!` | Khung lệnh vượt quá 128 byte. |

## 6.6 Bản tin trạng thái

Cùng một định dạng duy nhất cho cả lệnh `STATUS` và bản tin tự phát mỗi 3 giây — sinh ra từ hàm `Format_Status()` (`main.c`):

```
TEMP=27C HUM=61% DHT=OK BT=OK AUTH=NONE ALARM=NORMAL OUT=0000
```

| Trường | Giá trị khả dĩ | Ý nghĩa |
|---|---|---|
| `TEMP=27C` | `0`..`50C` | Nhiệt độ đo được (°C). Bằng `0` nếu chưa đọc được lần nào |
| `HUM=61%` | `0`..`100%` | Độ ẩm đo được (%) |
| `DHT=OK` | `OK` / `FAIL` | Sức khỏe cảm biến DHT11 (`OK` = đọc thành công; `FAIL` = lỗi đọc/checksum) |
| `BT=OK` | `OK` / `NO` | Tình trạng liên lạc UART với module Bluetooth (`OK` = đã nhận byte; `NO` = chưa) |
| `AUTH=NONE` | `NONE` / `LOCAL` / `BLE` | Chủ sở hữu phiên độc quyền hiện tại |
| `ALARM=NORMAL` | `NORMAL` / `HIGH` / `DHT_FAULT` | Trạng thái cảnh báo chân PA8 |
| `OUT=0000` | 4 ký tự `0` hoặc `1` | Bản đồ trạng thái của 4 kênh OUT1–OUT4 (kênh 1 ở đầu, 1 = BẬT, 0 = TẮT) |

## 6.7 Bản tin tự phát

Ngoài câu trả lời cho từng lệnh, thiết bị còn tự gửi:

| Bản tin | Khi nào | Ý nghĩa |
|---|---|---|
| `MKE-M15 ready` | Ngay sau khi khởi động xong | Dấu hiệu firmware đã boot thành công |
| Chuỗi trạng thái (§6.6) | Mỗi **3 giây** | Cập nhật telemetry định kỳ |
| `Disconnected` | Sau **10 giây** im lặng | Cảnh báo mất kết nối từ xa do đường truyền không có tín hiệu |
| `LOCAL LOGIN - KICKED` | Ngay khi người dùng Local đăng nhập thành công trên OLED | Quyền điều khiển của BLE bị thu hồi ở tầng ứng dụng do Local được ưu tiên |

> **Lưu ý về Kick BLE**: “Kick BLE” chỉ thu hồi quyền điều khiển ở tầng logic ứng dụng và gửi thông báo `LOCAL LOGIN - KICKED\r\n`. Kết nối Bluetooth vật lý (SPP) vẫn duy trì bình thường, BLE vẫn đọc được telemetry nhưng không thể gửi lệnh `ON`/`OFF` cho đến khi Local hết hạn hoặc đăng xuất.

## 6.8 Phiên hội thoại mẫu

```
                                    ← MKE-M15 ready
                                    ← TEMP=0C HUM=0% DHT=FAIL BT=NO AUTH=NONE ALARM=DHT_FAULT OUT=0000
STATUS →
                                    ← TEMP=28C HUM=65% DHT=OK BT=OK AUTH=NONE ALARM=NORMAL OUT=0000
ON 1 →
                                    ← ERR_LOCKED
LOGIN 9999 →
                                    ← LOGIN_FAIL
LOGIN 1234 →
                                    ← LOGIN_OK
ON 1 →
                                    ← OUT1_ON
ON 4 →
                                    ← OUT4_ON
STATUS →
                                    ← TEMP=28C HUM=65% DHT=OK BT=OK AUTH=BLE ALARM=NORMAL OUT=1001
    (Người dùng Local bấm phím OLED và nhập đúng PIN 1234)
                                    ← LOCAL LOGIN - KICKED
ON 2 →
                                    ← ERR_LOCKED
LOGIN 1234 →
                                    ← LOGIN_BUSY_LOCAL
    (Sau 60 giây Local không thao tác, phiên Local hết hạn)
LOGIN 1234 →
                                    ← LOGIN_OK
LOGOUT →
                                    ← LOGOUT_OK
```

Lưu ý dòng thứ hai: bản tin trạng thái đầu tiên có `BT=NO` vì lúc đó thiết bị chưa nhận
được byte nào từ điện thoại — cờ `BT` chỉ lên sau lệnh đầu tiên người dùng gửi.

## 6.8 Ghi chú cho người viết app

1. **Nên đặt app gửi CR hoặc LF.** Cơ chế tự chốt sau 250 ms là để cứu các app đặt "no line
   ending", nhưng nó gộp mọi thứ gõ trong 250 ms thành một khung — app gửi từng phím vừa gõ
   sẽ tạo ra lệnh cắt vụn.
2. **Đọc bản tin theo dòng.** Mọi bản tin kết thúc bằng `\r\n`; đừng giả định một lần đọc
   socket là trọn một bản tin.
3. **Bản tin tự phát xen kẽ với câu trả lời.** Sau khi gửi `ON 3`, dòng tiếp theo nhận được
   có thể là chuỗi trạng thái định kỳ chứ chưa phải `OUT3_ON`. App nên phân loại bản tin
   theo nội dung, không theo thứ tự.
4. **Đừng gửi hai lệnh liên tiếp quá nhanh.** Thiết bị xử lý mỗi lượt vòng lặp một lệnh, và
   `UART_Print()` bỏ bản tin nếu lần phát trước chưa xong. Chờ nhận được câu trả lời rồi
   hãy gửi lệnh kế tiếp.
5. **`OUT=` là nguồn sự thật về trạng thái 5 kênh**, không phải `OUT1=`. Trường `OUT1=` chỉ
   nói về kênh 1 và tồn tại vì lý do tương thích.
6. Thiết bị **không nhớ trạng thái qua reset** — sau khi mất điện, app nên gửi `STATUS` để
   đồng bộ lại thay vì tin vào trạng thái đang giữ.

## 6.9 Thêm một lệnh mới

Ba việc phải làm cùng lúc, thiếu bước nào cũng gây lệch:

1. Viết handler trong `main.c` theo chữ ký `void Cmd(char *return_msg, const char *args)`,
   dùng `COMMAND_RETURN_MSG_SIZE` (256) làm giới hạn của `snprintf`.
2. Thêm một dòng vào `Command_Menu[]` (`main.c:132`).
3. Cập nhật trang **HƯỚNG DẪN** trong `ui.c:751-757` và bảng §6.3 của tài liệu này.
