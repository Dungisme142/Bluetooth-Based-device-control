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
| **TC-01** | FR-25 | Cấp nguồn cho board. Quan sát 5 LED D1–D5 **ngay từ khoảnh khắc đầu tiên** | Cả 5 LED **tắt hoàn toàn**, không có cú nháy nào lúc khởi động | | |
| **TC-02** | — | Sau khi cấp nguồn, quan sát OLED | Hiện màn hình `BOOTING` với `STM32 BT NODE` / `5 OUTPUTS 5 BUTTONS`, rồi **chuyển sang trang HOME trong vòng 1 giây** | | |
| **TC-03** | FR-24 | Quan sát LED PC13 trên board Blue Pill trong 10 giây | Nháy đều, chu kỳ **1 giây** (sáng 1 s, tắt 1 s) | | |
| **TC-04** | FR-15 | Kết nối app terminal **trước** khi cấp nguồn, rồi bật nguồn | Nhận được `MKE-M15 ready` | | |
| **TC-05** | FR-21 | Chuyển tới trang LOG (bấm NEXT 3 lần từ HOME) | Có dòng `BOOT OK` với mốc thời gian `00:00` | | |
| **TC-06** | FR-25 | Ngay sau khởi động, gửi `STATUS →` | `← ... OUT=00000` — cả 5 kênh TẮT | | |

---

## 11.3 Nhóm B — Giao thức Bluetooth: lệnh hợp lệ

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-07** | FR-02 | `ON 1 →` | `← OUT1_ON`, **LED D1 sáng** | | |
| **TC-08** | FR-02 | `ON 5 →` | `← OUT5_ON`, **LED D5 sáng**, D1 vẫn sáng, D2–D4 vẫn tắt | | |
| **TC-09** | FR-01 | Lần lượt `ON 2`, `ON 3`, `ON 4` | Mỗi lệnh trả `OUTn_ON` đúng số kênh, LED tương ứng sáng, **không kênh nào khác đổi trạng thái** | | |
| **TC-10** | FR-02 | `OFF 3 →` | `← OUT3_OFF`, **chỉ D3 tắt**, bốn LED còn lại giữ nguyên | | |
| **TC-11** | FR-03 | `OFF ALL →` | `← ALL_OFF`, **cả 5 LED tắt** | | |
| **TC-12** | FR-03 | `ON ALL →` | `← ALL_ON`, **cả 5 LED sáng** | | |
| **TC-13** | FR-04 | `OFF ALL →` rồi `ON →` (không tham số) | `← OUT1_ON`, chỉ D1 sáng | | |
| **TC-14** | FR-11 | `TEMP →` | `← TEMP=<n>C` với `<n>` là số hợp lý của nhiệt độ phòng (20–35) | | |
| **TC-15** | FR-11 | `HUM →` | `← HUM=<n>%` với `<n>` trong 20–90 | | |
| **TC-16** | FR-10 | `ON 1`, `ON 3` rồi `STATUS →` | `← TEMP=..C HUM=..% OUT1=ON BT=OK OUT=10100` — bản đồ bit khớp đúng LED đang sáng | | |
| **TC-17** | — | `AUTO →` | `← AUTO_MODE_READY` (lệnh còn là stub, chỉ cần trả lời đúng) | | |

---

## 11.4 Nhóm C — Giao thức Bluetooth: lỗi và biên

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-18** | FR-07 | `ON 9 →` | `← BAD_CHANNEL`, **không LED nào đổi** | | |
| **TC-19** | FR-07 | `ON 0 →` | `← BAD_CHANNEL` | | |
| **TC-20** | FR-07 | `ON 12 →` | `← BAD_CHANNEL` — **không** được hiểu thành kênh 1 | | |
| **TC-21** | FR-07 | `ON abc →` | `← BAD_CHANNEL` | | |
| **TC-22** | FR-12 | `ONLINE →` | `← Invalid Command` — **không** được hiểu thành `ON` | | |
| **TC-23** | FR-12 | `on 1 →` (chữ thường) | `← Invalid Command` (giao thức phân biệt hoa thường) | | |
| **TC-24** | FR-12 | `HELLO →` | `← Invalid Command` | | |
| **TC-25** | FR-13 | Đặt app về **line ending = CR**, gửi `ON 2` | `← OUT2_ON`, không có bản tin lỗi thừa | | |
| **TC-26** | FR-13 | Đặt app về **line ending = LF**, gửi `OFF 2` | `← OUT2_OFF` | | |
| **TC-27** | FR-13 | Đặt app về **line ending = CR+LF**, gửi `ON 2` | `← OUT2_ON` và **không có** `Invalid Command` theo sau (khung rỗng phải bị bỏ qua lặng lẽ) | | |
| **TC-28** | FR-13 | Đặt app về **no line ending**, gõ `ON 4` rồi gửi, chờ | Sau ~250 ms: `← OUT4_ON` | | |
| **TC-29** | — | Gửi một chuỗi **dài hơn 128 ký tự** | `← Fail, try again!`, thiết bị vẫn nhận lệnh bình thường sau đó | | |
| **TC-30** | — | Gửi một dòng **rỗng** (chỉ Enter) | Không có phản hồi nào, **không** có `Invalid Command` | | |

---

## 11.5 Nhóm D — Nút bấm và giao diện OLED

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-31** | FR-19, FR-20 | Từ HOME, bấm **NEXT** 5 lần, quan sát tiêu đề mỗi lần | `HOME` → `OUTPUTS` → `DHT11 SENSOR` → `LOG` → `HUONG DAN` → quay lại `HOME`. Chỉ số góc phải chạy `1/5` → `5/5` → `1/5` | | |
| **TC-32** | FR-20 | Từ HOME, bấm **PREV** 1 lần | Nhảy thẳng tới `HUONG DAN` (`5/5`) | | |
| **TC-33** | FR-19 | Xem trang OUTPUTS | 5 dòng đúng thứ tự: `1 OUT-1 PA8`, `2 OUT-2 PB15`, `3 OUT-3 PB14`, `4 OUT-4 PB13`, `5 OUT-5 PB12` | | |
| **TC-34** | FR-20 | Ở trang OUTPUTS, bấm **DOWN** 4 lần | Dòng đảo màu (đang chọn) di chuyển 1 → 2 → 3 → 4 → 5 | | |
| **TC-35** | FR-20 | Bấm **DOWN** thêm 1 lần nữa | Con trỏ **quay vòng** về dòng 1 | | |
| **TC-36** | FR-20 | Bấm **UP** 1 lần từ dòng 1 | Con trỏ quay vòng ngược về dòng 5 | | |
| **TC-37** | FR-05 | Ở trang OUTPUTS, chọn kênh 3, bấm **OK** | Cột trạng thái dòng 3 đổi `OFF` → `ON`, **LED D3 sáng** | | |
| **TC-38** | FR-05 | Bấm **OK** lần nữa | Dòng 3 đổi lại `ON` → `OFF`, LED D3 tắt | | |
| **TC-39** | FR-19 | Chọn kênh 2 ở trang OUTPUTS rồi bấm **PREV** về HOME | Ô số 2 ở trang HOME đang có **khung chọn bao ngoài** (con trỏ dùng chung giữa các trang) | | |
| **TC-40** | FR-19 | Ở trang HOME, bật vài kênh rồi quan sát hàng ô | Ô của kênh BẬT được **tô đặc**, kênh TẮT để **rỗng**, khớp với LED thật | | |
| **TC-41** | FR-19 | Xem trang HOME liên tục 1 phút | `hh:mm:ss` góc trái dưới tăng đều mỗi giây | | |
| **TC-42** | FR-19 | Xem trang DHT11 SENSOR | Hiện `TEMP <n>°C`, `HUMI <n>%`, hai thanh mức có độ dài tương ứng, và `LAST OK <n>s` với `n` ≤ 3 | | |
| **TC-43** | FR-21 | Ở trang LOG, bấm **UP** 3 lần | Cửa sổ cuộn về quá khứ, dải `x-y/12` trên tiêu đề thay đổi tương ứng | | |
| **TC-44** | FR-21 | Bấm **UP** liên tục tới khi không cuộn được nữa | Dừng lại ở `1-5/12`, **không** cuộn quá dòng cũ nhất | | |
| **TC-45** | FR-21 | Đang cuộn giữa chừng, bấm **OK** | Nhảy thẳng về bám đáy (`8-12/12`), **không** có kênh nào bị bật/tắt | | |
| **TC-46** | FR-19 | Xem trang HUONG DAN | Hiện 5 dòng hướng dẫn, danh sách lệnh khớp với [06](06-giao-thuc-bluetooth.md) §6.3 | | |
| **TC-47** | FR-22 | Bấm **NEXT** dứt khoát đúng 1 lần, 10 lần liên tiếp (nghỉ ~1 s giữa mỗi lần) | Mỗi lần chuyển **đúng 1 trang** — không có lần nào nhảy 2 trang | | |
| **TC-48** | FR-22 | **Giữ** nút NEXT 3 giây rồi nhả | Chuyển **đúng 1 trang**; lúc nhả tay **không** chuyển thêm trang nào nữa | | |
| **TC-49** | FR-22 | Bấm UP rồi bấm OK **ngay lập tức** (trong vòng < 100 ms) | **Cả hai** thao tác đều có tác dụng: con trỏ di chuyển **và** kênh được bật/tắt | | |
| **TC-50** | FR-23 | Chưa kết nối app: xem tiêu đề. Rồi kết nối app, gửi 1 lệnh, xem lại | Trước: chỉ có `1/5`. Sau: có `BT 1/5` | | |

---

## 11.6 Nhóm E — Cảm biến DHT11

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-51** | FR-16 | Xem trang LOG trong 20 giây | Xuất hiện `DHT OK` đều đặn, **khoảng 2 giây một dòng** | | |
| **TC-52** | FR-16 | So `TEMP →` với nhiệt kế phòng | Chênh lệch trong ±3 °C | | |
| **TC-53** | FR-17 | **Rút cảm biến DHT11** khỏi J6 khi board đang chạy. Chờ 10 giây | Trang LOG hiện `DHT FAIL` đều đặn 2 giây một dòng | | |
| **TC-54** | FR-17 | Ngay sau TC-53, gửi `TEMP →` | Trả về **giá trị cũ trước khi rút**, không phải `0` và không phải số rác | | |
| **TC-55** | FR-18 | Ngay sau TC-53, xem trang SENSOR | `LAST OK <n>s` với `n` **tăng dần** mỗi giây | | |
| **TC-56** | FR-17 | **Cắm lại** cảm biến. Chờ 5 giây | LOG quay lại `DHT OK`, `LAST OK` reset về số nhỏ — **không cần reset board** | | |
| **TC-57** | FR-18 | Reset board với cảm biến **chưa cắm**, xem trang SENSOR | Hiện `NO DATA` (không phải `LAST OK 0s`) | | |
| **TC-58** | NFR-03 | Trong lúc cảm biến bị rút (mỗi phép đo mất 500 ms), gửi `ON 2 →` | Vẫn trả `OUT2_ON` — có thể trễ tới ~0,5 s nhưng **không được mất lệnh** | | |

---

## 11.7 Nhóm F — Hội tụ hai đường điều khiển

Đây là nhóm quan trọng nhất — kiểm tra yêu cầu FR-06.

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-59** | FR-06 | `OFF ALL →`. Dùng **nút bấm** bật kênh 3. Rồi gửi `STATUS →` | `← ... OUT=00100` — trạng thái do nút tạo ra được phản ánh đúng | | |
| **TC-60** | FR-06 | Gửi `ON 4 →`. Rồi xem **trang OUTPUTS** | Dòng 4 hiện `ON` — trạng thái do lệnh Bluetooth tạo ra được phản ánh đúng trên màn hình | | |
| **TC-61** | FR-06 | Gửi `ON 2 →`, rồi dùng **nút OK** tắt kênh 2, rồi `STATUS →` | `← ... OUT=00000` (giả sử các kênh khác đang tắt) — nút tắt được kênh do lệnh bật | | |
| **TC-62** | FR-06 | Dùng **nút** bật kênh 5, rồi gửi `OFF 5 →` | `← OUT5_OFF`, LED D5 tắt, trang OUTPUTS dòng 5 hiện `OFF` | | |
| **TC-63** | FR-21 | Bật kênh 1 bằng **nút**, rồi bật kênh 2 bằng **lệnh**. Xem trang LOG | Có **cả hai** dòng `OUT1 ON` và `OUT2 ON` — nhật ký ghi cả hai đường điều khiển | | |
| **TC-64** | FR-06 | `ON ALL →`, rồi dùng nút tắt kênh 3, rồi `STATUS →` | `← ... OUT=11011` | | |

---

## 11.8 Nhóm G — Kết nối và độ bền

| ID | FR | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|---|
| **TC-65** | FR-14 | Kết nối app rồi gửi lệnh đầu tiên. Xem trang LOG | Có dòng `BT LINK UP` (chỉ xuất hiện **một lần** duy nhất) | | |
| **TC-66** | FR-14 | Sau khi đã kết nối, **không gửi gì** trong 15 giây | Nhận được `← Disconnected` sau khoảng 10 giây | | |
| **TC-67** | FR-14 | Ngay sau TC-66, gửi `STATUS →` | Trả lời bình thường với `BT=OK` — kết nối tự phục hồi | | |
| **TC-68** | FR-09 | Kết nối app, **không thao tác gì**, đếm bản tin trong 30 giây | Nhận đúng khoảng **10 bản tin** trạng thái (3 s/bản) | | |
| **TC-69** | — | Tắt nguồn module Bluetooth (hoặc tắt Bluetooth điện thoại) trong 30 giây rồi bật lại | Board vẫn chạy bình thường: LED PC13 vẫn nháy, OLED vẫn cập nhật, nút vẫn dùng được. Kết nối lại được | | |
| **TC-70** | — | Gửi 20 lệnh `ON 1` / `OFF 1` xen kẽ, **mỗi lệnh chờ nhận trả lời rồi mới gửi tiếp** | Đủ 20 câu trả lời, LED D1 đổi trạng thái đúng 20 lần | | |
| **TC-71** | — | Bấm nút **liên tục** trong lúc đang gửi lệnh Bluetooth (30 giây) | Không treo, không mất lệnh, không có ký tự rác trên terminal | | |
| **TC-72** | NFR-09 | Để board chạy **liên tục 30 phút**, sau đó kiểm tra | LED PC13 vẫn nháy đều, uptime trang HOME đúng ~`00:30:00`, `STATUS` vẫn trả lời | | |
| **TC-73** | FR-25 | Bật vài kênh, **cắt nguồn rồi cấp lại** | Cả 5 kênh về **TẮT** (hệ thống không lưu trạng thái — đây là hành vi đúng) | | |
| **TC-74** | — | Nhấn nút RESET trên Blue Pill | Board khởi động lại đầy đủ: `MKE-M15 ready`, OLED về `BOOTING` rồi HOME, LOG chỉ còn `BOOT OK` | | |

---

## 11.9 Nhóm H — Kiểm tra tải thật (sau khi mọi nhóm trên đã đạt)

> ⚠️ Chỉ chạy nhóm này khi **toàn bộ TC-01..TC-74 đều Pass**. Làm việc với module công suất
> và tải điện lưới đòi hỏi cẩn trọng — xem [03](03-thiet-ke-phan-cung.md) §3.7.

| ID | Các bước | Kết quả mong đợi | KQ | Ghi chú |
|---|---|---|---|---|
| **TC-75** | Đo điện áp chân SIG của J1 khi kênh 1 TẮT | Gần 0 V | | |
| **TC-76** | Đo điện áp chân SIG của J1 khi kênh 1 BẬT | Mức logic cao (≈3,3 V trừ sụt áp trên điện trở 330 Ω) | | |
| **TC-77** | Cắm **một** module relay vào J1. Gửi `ON 1` / `OFF 1` | Nghe rõ tiếng relay đóng/mở đúng theo lệnh | | |
| **TC-78** | Cắm module relay vào cả 5 connector. Gửi `ON ALL` | Cả 5 relay đóng; kiểm tra nguồn 5 V không sụt gây reset MCU (LED PC13 vẫn nháy đều) | | |
| **TC-79** | Với đủ 5 module, chạy `ON ALL` / `OFF ALL` xen kẽ 10 lần | Hoạt động ổn định, không reset, không mất kết nối Bluetooth | | |

---

## 11.10 Bảng tổng hợp

| Nhóm | Số ca | Pass | Fail | Không chạy |
|---|---|---|---|---|
| A — Khởi động | 6 | | | |
| B — Lệnh hợp lệ | 11 | | | |
| C — Lệnh lỗi và biên | 13 | | | |
| D — Nút bấm và OLED | 20 | | | |
| E — Cảm biến | 8 | | | |
| F — Hội tụ hai đường điều khiển | 6 | | | |
| G — Kết nối và độ bền | 10 | | | |
| H — Tải thật | 5 | | | |
| **Tổng** | **79** | | | |

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
