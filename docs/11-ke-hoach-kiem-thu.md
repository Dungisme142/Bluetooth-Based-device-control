# 11 — Kế hoạch kiểm thử

Kiểm thử **thủ công trên board thật**. Project không có framework unit test tự động
(xem [12](12-han-che-va-huong-phat-trien.md) §12.2 về hướng bổ sung).

## 11.1 Chuẩn bị

### Thiết bị cần có

| Món | Ghi chú |
|---|---|
| Board đã lắp đầy đủ | MCU, MKE-M15, OLED, DHT11, 5 nút, 5 LED chỉ báo |
| Nguồn 5 V | Qua J7 hoặc J9 |
| ST-Link V2 | Để nạp firmware |
| Điện thoại Android | Có app *Serial Bluetooth Terminal* hoặc tương đương |
| Đồng hồ vạn năng | Cho các ca đo mức điện áp |

### Trước khi chạy

1. Nạp firmware theo [10 — Build và nạp](10-build-va-nap.md).
2. Ghép cặp điện thoại với module MKE-M15.
3. Trong app terminal: đặt **baud 9600**, **line ending = CR** hoặc **LF** (trừ các ca
   TC-14, TC-15 cố tình đổi).
4. **Chưa cắm module công suất vào J1–J5** — chỉ dùng LED chỉ báo D1–D5 để quan sát. Chỉ
   cắm tải sau khi TC-01..TC-05 đều đạt.

### Cách ghi kết quả

Điền cột **KQ** bằng `P` (Pass) / `F` (Fail) / `–` (không chạy). Ca nào Fail thì ghi hiện
tượng thật vào cột **Ghi chú**.

Ký hiệu: `→` = gửi lệnh từ điện thoại, `←` = nhận từ thiết bị.

---

## 11.2 Nhóm A — Khởi động và trạng thái ban đầu

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-01** | FR-25 | Cấp nguồn cho board. Quan sát 4 LED ngõ ra D1–D4 và LED PA8 **ngay từ khoảnh khắc đầu tiên** | Cả 4 LED ngõ ra **tắt hoàn toàn**. LED PA8 nhấp nháy chu kỳ 250 ms (do chưa có mẫu DHT hợp lệ) | | |
| **TC-02** | FR-19 | Sau khi cấp nguồn, quan sát OLED | Hiện màn hình `BOOTING` với `STM32 BT NODE` / `4 OUTPUTS 5 BUTTONS`, rồi **chuyển sang Màn hình khóa (Locked Dashboard) trong vòng 1 giây** | | |
| **TC-03** | FR-24 | Quan sát LED PC13 trên board Blue Pill trong 10 giây | Nháy đều, chu kỳ **1 giây** (sáng 1 s, tắt 1 s) | | |
| **TC-04** | FR-15 | Kết nối app terminal **trước** khi cấp nguồn, rồi bật nguồn | Nhận được `MKE-M15 ready` | | |
| **TC-05** | FR-21 | Đăng nhập Local bằng PIN 1234, chuyển tới trang LOG (bấm NEXT 3 lần từ HOME) | Có dòng `BOOT OK` với mốc thời gian `00:00` | | |
| **TC-06** | FR-25 | Ngay sau khởi động (khi chưa có lệnh nào), gửi `STATUS →` | `← TEMP=.. HUM=.. DHT=.. BT=OK AUTH=NONE ALARM=.. OUT=0000` — cả 4 kênh TẮT, chủ quyền là `NONE` | | |

---

## 11.3 Nhóm B — Giao thức Bluetooth: lệnh hợp lệ & xác thực

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-07** | FR-28 | Khi chưa gửi `LOGIN`: gửi `ON 1 →` | `← ERR_LOCKED`, LED D1 không đổi (bảo vệ khi chưa xác thực) | | |
| **TC-08** | FR-26 | Gửi `LOGIN 1234 →` | `← LOGIN_OK`, BLE trở thành chủ phiên (`AUTH_BLE`) | | |
| **TC-09** | FR-02 | Sau TC-08, gửi `ON 1 →` | `← OUT1_ON`, **LED D1 sáng** | | |
| **TC-10** | FR-02 | Gửi `ON 4 →` | `← OUT4_ON`, **LED D4 sáng**, D1 vẫn sáng, D2–D3 vẫn tắt | | |
| **TC-11** | FR-01 | Lần lượt gửi `ON 2`, `ON 3` | Mỗi lệnh trả `OUTn_ON` đúng số kênh, LED tương ứng sáng | | |
| **TC-12** | FR-02 | Gửi `OFF 3 →` | `← OUT3_OFF`, **chỉ D3 tắt**, ba LED còn lại giữ nguyên | | |
| **TC-13** | FR-03 | Gửi `OFF ALL →` | `← ALL_OFF`, **cả 4 LED tắt** | | |
| **TC-14** | FR-03 | Gửi `ON ALL →` | `← ALL_ON`, **cả 4 LED sáng** | | |
| **TC-15** | FR-04 | Gửi `OFF ALL →` rồi `ON →` (không tham số) | `← OUT1_ON`, chỉ D1 sáng (OUT1 trên PB15) | | |
| **TC-16** | FR-11 | Gửi `TEMP →` | `← TEMP=<n>C` với `<n>` hợp lý (20–35) | | |
| **TC-17** | FR-11 | Gửi `HUM →` | `← HUM=<n>%` với `<n>` trong 20–90 | | |
| **TC-18** | FR-10 | Sau khi bật kênh 1 và 4, gửi `STATUS →` | `← TEMP=..C HUM=..% DHT=OK BT=OK AUTH=BLE ALARM=NORMAL OUT=1001` | | |
| **TC-19** | FR-27 | Gửi `LOGOUT →` | `← LOGOUT_OK`, quyền phiên trở về `AUTH_NONE`. Gửi `ON 1` sau đó nhận `ERR_LOCKED` | | |

---

## 11.4 Nhóm C — Giao thức Bluetooth: lỗi, biên & phân quyền

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-20** | FR-26 | Gửi `LOGIN 9999 →` hoặc `LOGIN 12 →` | `← LOGIN_FAIL`, quyền vẫn là `AUTH_NONE` | | |
| **TC-21** | FR-27 | Gửi `LOGOUT →` khi chưa đăng nhập | `← ERR_NOT_OWNER` | | |
| **TC-22** | FR-07 | Đăng nhập BLE thành công, rồi gửi `ON 5 →` | `← BAD_CHANNEL` (chỉ có 4 kênh 1..4) | | |
| **TC-23** | FR-07 | Gửi `ON 0 →` hoặc `ON 9 →` | `← BAD_CHANNEL` | | |
| **TC-24** | FR-07 | Gửi `ON 12 →` | `← BAD_CHANNEL` — không được hiểu thành kênh 1 | | |
| **TC-25** | FR-07 | Gửi `ON abc →` | `← BAD_CHANNEL` | | |
| **TC-26** | FR-12 | Gửi `ONLINE →` | `← Invalid Command` — không bị hiểu nhầm thành `ON` | | |
| **TC-27** | FR-12 | Gửi `on 1 →` (chữ thường) | `← Invalid Command` (phân biệt hoa thường) | | |
| **TC-28** | FR-13 | Đặt app về **line ending = CR**, gửi `LOGIN 1234` rồi `ON 2` | `← LOGIN_OK`, `← OUT2_ON` | | |
| **TC-29** | FR-13 | Đặt app về **line ending = LF**, gửi `OFF 2` | `← OUT2_OFF` | | |
| **TC-30** | FR-13 | Đặt app về **line ending = CR+LF**, gửi `ON 2` | `← OUT2_ON`, không có `Invalid Command` thừa | | |
| **TC-31** | FR-13 | Đặt app về **no line ending**, gõ `ON 4` rồi gửi, chờ | Sau ~250 ms: `← OUT4_ON` | | |
| **TC-32** | — | Gửi chuỗi dài hơn 128 ký tự | `← Fail, try again!`, thiết bị hoạt động bình thường tiếp | | |

---

## 11.5 Nhóm D — Nút bấm và giao diện OLED (Khóa & Bàn phím ảo)

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-33** | FR-19 | Quan sát OLED sau khi boot xong | Hiển thị **Dashboard khóa**: `LOCKED`, `TEMP`, `HUMI`, `DHT: OK`, `BLE: LINK/NO`, `AUTH: NONE`, `ALARM: NORMAL`, `OUT: 0000`, `>> PRESS ANY KEY <<` | | |
| **TC-34** | FR-19 | Khi đang ở Dashboard khóa, bấm thử nút UP, DOWN, OK | Không kênh nào bị bật/tắt (không sinh `toggle_output`). Màn hình lập tức chuyển sang **Bàn phím số ảo 3×4** với đệm `PIN: ` rỗng | | |
| **TC-35** | FR-19 | Ở Bàn phím ảo, không thao tác trong **30 giây** | Tự động quay về Dashboard khóa | | |
| **TC-36** | FR-19 | Bấm phím mở lại Bàn phím ảo. Bấm NEXT/PREV để đổi cột; UP/DOWN để đổi hàng | Ô được chọn tô nền trắng chữ đen di chuyển đúng theo ma trận 3×4 | | |
| **TC-37** | FR-26 | Di chuyển và bấm OK chọn mã sai `1111` -> chọn ô ảo `OK` | Thanh tiêu đề hiển thị `WRONG PIN` trong ~1.5s, đệm xóa về rỗng | | |
| **TC-38** | FR-26 | Chọn `<` khi chưa nhập số nào; chọn `OK` ảo khi mới nhập 2 số | Không có phản ứng sai, không crash | | |
| **TC-39** | FR-26 | Nhập đúng `"1234"` -> chọn ô ảo `OK` | Đăng nhập thành công! Giao diện mở khóa chuyển thẳng vào **Trang 1 — HOME** của 5 trang tiêu chuẩn | | |
| **TC-40** | FR-29 | Sau khi đăng nhập, không bấm bất kỳ nút nào trong **60 giây** | Phiên Local tự động hết hạn, màn hình tự động quay về Dashboard khóa | | |
| **TC-41** | FR-19 | Đăng nhập lại. Bấm **NEXT** lần lượt qua 5 trang | `HOME` → `OUTPUTS` → `DHT11 SENSOR` → `LOG` → `HUONG DAN` → quay lại `HOME` (`1/5`..`5/5`) | | |
| **TC-42** | FR-19 | Xem trang OUTPUTS | Hiển thị 4 kênh: `1 OUT-1 PB15`, `2 OUT-2 PB14`, `3 OUT-3 PB13`, `4 OUT-4 PB12` (không có PA8) | | |
| **TC-43** | FR-05 | Ở trang OUTPUTS, chọn kênh 3, bấm **OK** vật lý | Kênh 3 chuyển `ON`, **LED D3 sáng** | | |
| **TC-44** | FR-05 | Bấm **OK** lần nữa | Kênh 3 chuyển `OFF`, LED D3 tắt | | |
| **TC-45** | FR-19 | Ở trang HOME, quan sát hàng ô vuông | Có đúng **4 ô vuông** tương ứng OUT1..OUT4. Ô sáng tô đặc, ô tắt để rỗng | | |

---

## 11.6 Nhóm E — Cảm biến DHT11 & Sức khỏe hệ thống

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-46** | FR-16 | Xem trang LOG trong 20 giây khi cảm biến bình thường | Xuất hiện `DHT OK` đều đặn mỗi ~2 giây | | |
| **TC-47** | FR-18 | **Rút dây cảm biến DHT11** khỏi J6 khi board đang chạy. Quan sát trong 5–10 giây | **(1)** Log xuất hiện dòng `DHT BAD`<br/>**(2)** Thanh tiêu đề trên **mọi trang** xuất hiện huy hiệu `!DHT`<br/>**(3)** Trang HOME góc phải dưới lập tức chuyển thành hộp đảo màu `DHT BAD` (thay vì giữ `DHT OK`)<br/>**(4)** Trang SENSOR hiển thị `DHT: BAD`<br/>**(5)** LED PA8 nhấp nháy liên tục chu kỳ 250 ms<br/>**(6)** `STATUS` báo `DHT=FAIL` và `ALARM=DHT_FAULT` | | |
| **TC-48** | FR-17 | Ngay sau TC-47, gửi `TEMP →` và `HUM →` từ Bluetooth | Trả về số đo cũ trước khi rút (không gửi rác) | | |
| **TC-49** | FR-18 | **Cắm lại dây cảm biến**. Chờ 3–4 giây | Ngay khi đọc thành công mẫu mới:<br/>**(1)** Huy hiệu `!DHT` trên thanh tiêu đề biến mất<br/>**(2)** Trang HOME khôi phục hiển thị `DHT OK`<br/>**(3)** LED PA8 ngừng nhấp nháy, trở về TẮT (nếu ẩm ≤ 90%)<br/>**(4)** LOG ghi `DHT OK` | | |
| **TC-50** | FR-31 | Thổi hơi ẩm vào cảm biến để độ ẩm vượt > 90% | LED PA8 bật sáng liên tục; `STATUS` báo `ALARM=HIGH`. Khi độ ẩm giảm ≤ 90%: PA8 tự tắt, `STATUS` báo `ALARM=NORMAL` | | |
| **TC-51** | FR-31 | Gửi các lệnh `ON ALL`, `OFF ALL`, `ON 1..4` | Không lệnh nào làm thay đổi trạng thái của chân PA8 | | |

---

## 11.7 Nhóm F — Hội tụ hai đường điều khiển & Quyền ưu tiên Local

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-52** | FR-30 | BLE gửi `LOGIN 1234` thành công (`AUTH_BLE`). Sau đó trên OLED, người dùng mở keypad và nhập `"1234"` | **(1)** Local đăng nhập thành công, OLED mở trang HOME<br/>**(2)** Trên Terminal BLE lập tức nhận được dòng `LOCAL LOGIN - KICKED\r\n`<br/>**(3)** Quyền phiên chuyển sang `AUTH_LOCAL` | | |
| **TC-53** | FR-30 | Ngay sau TC-52, từ BLE gửi `ON 1 →` | `← ERR_LOCKED` (quyền BLE đã bị thu hồi) | | |
| **TC-54** | FR-30 | Từ BLE gửi `LOGIN 1234 →` trong lúc Local đang giữ phiên | `← LOGIN_BUSY_LOCAL` (Local được bảo vệ độc quyền) | | |
| **TC-55** | FR-06 | Sau khi Local đăng nhập, bấm nút bật kênh 3. Rồi từ BLE gửi `STATUS →` | `← ... AUTH=LOCAL ... OUT=0010` — trạng thái do nút bật được phản ánh đúng qua Bluetooth | | |
| **TC-56** | FR-29 | Để yên không bấm nút trong 60 giây. Sau đó từ BLE gửi `LOGIN 1234 →` | `← LOGIN_OK` — phiên Local đã hết hạn, BLE đăng nhập lại thành công | | |
| **TC-57** | FR-06 | BLE gửi `OFF ALL →`, sau đó `ON 4 →`. Quan sát trang OUTPUTS trên OLED | Dòng 4 hiện `ON`, 3 dòng còn lại hiện `OFF` | | |

---

## 11.8 Nhóm G — Kết nối và độ bền

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-58** | FR-14 | Kết nối app rồi gửi lệnh đầu tiên. Xem trang LOG | Có dòng `BT LINK UP` (chỉ xuất hiện **một lần** duy nhất) | | |
| **TC-59** | FR-14 | Sau khi đã kết nối, **không gửi gì** trong 15 giây | Nhận được `← Disconnected` sau khoảng 10 giây | | |
| **TC-60** | FR-14 | Ngay sau TC-59, gửi `STATUS →` | Trả lời bình thường với `BT=OK` — kết nối tự phục hồi | | |
| **TC-61** | FR-09 | Kết nối app, **không thao tác gì**, đếm bản tin trong 30 giây | Nhận đúng khoảng **10 bản tin** trạng thái (3 s/bản) | | |
| **TC-62** | — | Tắt nguồn module Bluetooth (hoặc tắt Bluetooth điện thoại) trong 30 giây rồi bật lại | Board vẫn chạy bình thường: LED PC13 vẫn nháy, OLED vẫn cập nhật, nút vẫn dùng được. Kết nối lại được | | |
| **TC-63** | — | Gửi 20 lệnh `ON 1` / `OFF 1` xen kẽ (sau khi login), **mỗi lệnh chờ nhận trả lời rồi mới gửi tiếp** | Đủ 20 câu trả lời, LED D1 đổi trạng thái đúng 20 lần | | |
| **TC-64** | — | Bấm nút **liên tục** trong lúc đang gửi lệnh Bluetooth (30 giây) | Không treo, không mất lệnh, không có ký tự rác trên terminal | | |
| **TC-65** | NFR-09 | Để board chạy **liên tục 30 phút**, sau đó kiểm tra | LED PC13 vẫn nháy đều, uptime trang HOME đúng ~`00:30:00`, `STATUS` vẫn trả lời | | |
| **TC-66** | FR-25 | Bật vài kênh, **cắt nguồn rồi cấp lại** | Cả 4 kênh về **TẮT** (hệ thống không lưu trạng thái — đây là hành vi đúng) | | |
| **TC-67** | — | Nhấn nút RESET trên Blue Pill | Board khởi động lại đầy đủ: `MKE-M15 ready`, OLED về `BOOTING` rồi Locked Dashboard, LOG chỉ còn `BOOT OK` | | |

---

## 11.9 Nhóm H — Kiểm tra tải thật (sau khi mọi nhóm trên đã đạt)

> ⚠️ Chỉ chạy nhóm này khi **toàn bộ TC-01..TC-67 đều Pass**. Làm việc với module công suất
> và tải điện lưới đòi hỏi cẩn trọng — xem [03](03-thiet-ke-phan-cung.md) §3.7.

| ID | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|
| **TC-68** | Đo điện áp chân SIG của J1 khi kênh 1 TẮT | Gần 0 V | | |
| **TC-69** | Đo điện áp chân SIG của J1 khi kênh 1 BẬT | Mức logic cao (≈3,3 V trừ sụt áp trên điện trở 330 Ω) | | |
| **TC-70** | Cắm **một** module relay vào J1. Gửi `ON 1` / `OFF 1` | Nghe rõ tiếng relay đóng/mở đúng theo lệnh | | |
| **TC-71** | Cắm module relay vào cả 4 connector (J1..J4). Gửi `ON ALL` | Cả 4 relay đóng; kiểm tra nguồn 5 V không sụt gây reset MCU (LED PC13 vẫn nháy đều) | | |
| **TC-72** | Với đủ 4 module, chạy `ON ALL` / `OFF ALL` xen kẽ 10 lần | Hoạt động ổn định, không reset, không mất kết nối Bluetooth | | |

---

## 11.10 Bảng tổng hợp

| Nhóm | Số ca | Pass | Fail | Không chạy |
|---|---|---|---|---|
| A — Khởi động | 6 | | | |
| B — Lệnh hợp lệ & Xác thực | 13 | | | |
| C — Lệnh lỗi, biên & Phân quyền | 13 | | | |
| D — Nút bấm và OLED | 13 | | | |
| E — Cảm biến & Sức khỏe | 6 | | | |
| F — Hội tụ & Ưu tiên Local | 6 | | | |
| G — Kết nối và độ bền | 10 | | | |
| H — Tải thật | 5 | | | |
| **Tổng** | **72** | | | |

**Người kiểm thử**: ______________  **Ngày**: ______________
**Phiên bản firmware** (git commit): ______________

## 11.11 Tiêu chí nghiệm thu

| Mức | Điều kiện |
|---|---|
| **Bắt buộc đạt** | Toàn bộ nhóm A (an toàn khởi động) và nhóm F (hội tụ hai đường điều khiển) |
| **Đạt để bàn giao** | ≥ 95 % các ca ở nhóm A–G, và **không** có ca Fail nào ở nhóm A hoặc F |
| **Đạt để chạy tải thật** | Toàn bộ nhóm A–G Pass, sau đó mới chạy nhóm H |

Ca Fail ở nhóm A là **lỗi chặn**: hệ thống có thể bật thiết bị ngoài ý muốn lúc khởi động.

## 11.12 Những chỗ chưa được kiểm thử tự động

Các hàm sau là ứng viên tốt nhất cho unit test chạy trên máy tính (không cần phần cứng),
nếu sau này bổ sung khung Unity:

| Hàm | Vì sao đáng test | Ca test cần có |
|---|---|---|
| `Ring_Buffer_*` | Logic vòng, phân biệt đầy/rỗng | Ghi/đọc bình thường, ghi khi đầy, đọc khi rỗng, quay vòng |
| `Command_Selecting()` | Khớp tiền tố có bẫy | `ON`, `ON 3`, `ONLINE`, `on`, chuỗi rỗng, lệnh không có trong bảng |
| `Text_Filting()` / `Frame_Building()` | Xử lý biên phức tạp | CR, LF, CRLF, backspace, khung tràn, khung rỗng |
| `Command_SetOutputs()` | Kiểm tra tham số | `NULL`, `ALL`, `1`..`5`, `0`, `6`, `12`, `1X` |
| `Format_Status()` | Định dạng bản đồ bit | Mọi kênh tắt, mọi kênh bật, hỗn hợp |

Xem [12](12-han-che-va-huong-phat-trien.md) §12.2.
