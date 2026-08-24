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

| Lệnh | Tham số | Tác dụng | Trả lời |
|---|---|---|---|
| `ON` | (không) | Bật kênh **1** | `OUT1_ON` |
| `ON n` | `1`..`5` | Bật kênh `n` | `OUTn_ON` |
| `ON ALL` | `ALL` | Bật cả 5 kênh | `ALL_ON` |
| `OFF` | (không) | Tắt kênh **1** | `OUT1_OFF` |
| `OFF n` | `1`..`5` | Tắt kênh `n` | `OUTn_OFF` |
| `OFF ALL` | `ALL` | Tắt cả 5 kênh | `ALL_OFF` |
| `STATUS` | — | Trạng thái đầy đủ | xem §6.5 |
| `TEMP` | — | Nhiệt độ hiện tại | `TEMP=27C` |
| `HUM` | — | Độ ẩm hiện tại | `HUM=61%` |
| `AUTO` | — | ⚠️ **Chưa triển khai** — chỉ trả lời, không đổi hành vi | `AUTO_MODE_READY` |

Mọi câu trả lời kết thúc bằng `\r\n`.

Lệnh `ON`/`OFF` không tham số tác động lên kênh 1 — giữ lại có chủ ý để các app điện thoại
đã cấu hình sẵn từ thời hệ thống chỉ có một ngõ ra vẫn dùng được.

## 6.4 Thông báo lỗi

| Trả lời | Nguyên nhân |
|---|---|
| `BAD_CHANNEL` | Tham số của `ON`/`OFF` không phải `ALL` và không phải đúng một chữ số trong `1`..`5`. Ví dụ: `ON 9`, `ON 0`, `ON 12`, `ON 1X`, `ON abc` |
| `Invalid Command` | Không lệnh nào trong bảng khớp. Ví dụ: `ONLINE`, `on 1`, `RESET` |
| `Fail, try again!` | Khung lệnh vượt quá 128 byte |

## 6.5 Bản tin trạng thái

Cùng một định dạng cho cả lệnh `STATUS` và bản tin tự phát mỗi 3 giây — chỉ có một hàm
sinh ra nó (`Format_Status()`, `main.c:291`).

```
TEMP=27C HUM=61% OUT1=ON BT=OK OUT=10100
```

| Trường | Ý nghĩa |
|---|---|
| `TEMP=27C` | Nhiệt độ, số nguyên độ C. Bằng `0` nếu chưa đọc được lần nào |
| `HUM=61%` | Độ ẩm, số nguyên phần trăm |
| `OUT1=ON` | Trạng thái kênh 1 dưới dạng chữ (giữ lại cho tương thích ngược) |
| `BT=OK` | `OK` = đã nhận được byte từ module Bluetooth; `NO` = chưa bao giờ |
| `OUT=10100` | **Bản đồ bit của cả 5 kênh**, kênh 1 đứng trước |

Ví dụ `OUT=10100` nghĩa là: kênh 1 BẬT, kênh 2 TẮT, kênh 3 BẬT, kênh 4 TẮT, kênh 5 TẮT.

Dạng chuỗi 0/1 được chọn thay vì số hex để đọc bằng mắt được ngay trên màn hình terminal
mà không phải đổi cơ số.

## 6.6 Bản tin tự phát

Ngoài câu trả lời cho từng lệnh, thiết bị còn tự gửi:

| Bản tin | Khi nào |
|---|---|
| `MKE-M15 ready` | Ngay sau khi khởi động xong — dấu hiệu firmware đã boot |
| Chuỗi trạng thái (§6.5) | Mỗi **3 giây**, không cần được hỏi |
| `Disconnected` | Sau **10 giây** không nhận được byte nào kể từ lần cuối |

`Disconnected` là suy đoán từ sự im lặng của đường truyền, không phải sự kiện thật từ ngăn
xếp Bluetooth. Nếu app chỉ nhận mà không gửi gì trong 10 giây, thiết bị vẫn báo
`Disconnected` — sau đó chỉ cần gửi một lệnh bất kỳ là cờ kết nối lên lại.

## 6.7 Phiên hội thoại mẫu

```
                                    ← MKE-M15 ready
                                    ← TEMP=0C HUM=0% OUT1=OFF BT=NO OUT=00000
STATUS →
                                    ← TEMP=0C HUM=0% OUT1=OFF BT=OK OUT=00000
                                    ← TEMP=28C HUM=65% OUT1=OFF BT=OK OUT=00000
ON 1 →
                                    ← OUT1_ON
ON 3 →
                                    ← OUT3_ON
STATUS →
                                    ← TEMP=28C HUM=65% OUT1=ON BT=OK OUT=10100
TEMP →
                                    ← TEMP=28C
HUM →
                                    ← HUM=65%
ON 9 →
                                    ← BAD_CHANNEL
ONLINE →
                                    ← Invalid Command
OFF ALL →
                                    ← ALL_OFF
                                    ← TEMP=28C HUM=65% OUT1=OFF BT=OK OUT=00000
   (im lặng 10 giây)
                                    ← Disconnected
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
