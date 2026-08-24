# 07 — Đặc tả giao diện OLED

## 7.1 Khung màn hình chung

Màn hình SSD1306 **128 × 64** đơn sắc. Font 5×7, mỗi ký tự chiếm 6 px bề ngang → một dòng
chứa tối đa **21 ký tự** (21 × 6 = 126 px).

```
      y=0   ┌────────────────────────────────┐  thanh tiêu đề đảo màu
            │ HOME                    BT 1/5 │  (nền trắng, chữ đen)
      y=11  ├────────────────────────────────┤
            │                                │
            │        phần thân riêng          │
            │        của từng trang           │
            │                                │
      y=63  └────────────────────────────────┘
```

### Thanh tiêu đề

Có ở **mọi** trang. Bên trái là tên trang, bên phải là `BT <trang>/<tổng>`.

- Chữ `BT ` **chỉ xuất hiện khi đã có liên lạc Bluetooth**. Sự vắng mặt của nó chính là dấu
  hiệu mất kết nối — không cần thêm chữ "NO LINK" chiếm chỗ.
- Ví dụ: `BT 2/5` = đang ở trang 2 trong 5, Bluetooth đang có liên lạc.
  `3/5` (không có `BT`) = đang ở trang 3, chưa có liên lạc.
- Riêng trang LOG chèn thêm dải `<đầu>-<cuối>/<tổng>` ở giữa thanh tiêu đề.

Kỹ thuật: tô đặc cả dải bằng màu trắng rồi viết chữ **đen** đè lên — glyph "khoét" ra khỏi
nền trắng.

## 7.2 Năm trang

Thứ tự cố định, chuyển vòng tròn bằng NEXT / PREV:

```
HOME ⇄ OUTPUTS ⇄ SENSOR ⇄ LOG ⇄ HƯỚNG DẪN ⇄ (quay lại HOME)
```

Trang HƯỚNG DẪN đặt cuối vòng có chủ ý: từ HOME bấm **PREV một cái** là tới ngay.

---

### Trang 1 — HOME

Dashboard tổng hợp: nhiệt độ, độ ẩm, trạng thái 5 kênh, thời gian chạy.

```
┌────────────────────────────────┐
│ HOME                    BT 1/5 │
├────────────────────────────────┤
│ TEMP 28°C          HUMI 65%    │
│ ▐████████░░░▌      ▐██████░░▌  │   ← hai thanh mức
│────────────────────────────────│
│ OUT  1     2     3     4     5 │
│     ┌─┐   ╔═╗   ┌─┐   ┌─┐   ┌─┐│   ← ô: rỗng=TẮT, đặc=BẬT
│     └─┘   ╚═╝   └─┘   └─┘   └─┘│      khung ngoài = đang chọn
│ 00:12:34                DHT OK │
└────────────────────────────────┘
```

| Phần tử | Ý nghĩa |
|---|---|
| `TEMP 28°C` | Nhiệt độ mới nhất |
| Thanh mức nhiệt | Thang **0–50 °C** (`UI_TEMP_SCALE_MAX_C`) |
| `HUMI 65%` | Độ ẩm mới nhất |
| Thanh mức ẩm | Dùng thẳng phần trăm |
| 5 ô vuông | Trạng thái 5 kênh: rỗng = TẮT, tô đặc = BẬT |
| Khung bao quanh một ô | Kênh đang được con trỏ trỏ tới |
| `00:12:34` | Thời gian chạy `hh:mm:ss` kể từ khi khởi động |
| `DHT OK` / `DHT --` | Đã từng đọc được cảm biến hay chưa |

---

### Trang 2 — OUTPUTS

Danh sách 5 kênh dạng bảng, thao tác bật/tắt chính diễn ra ở đây.

```
┌────────────────────────────────┐
│ OUTPUTS                 BT 2/5 │
├────────────────────────────────┤
│ 1 OUT-1 PA8     ON             │
│ ██2 OUT-2 PB15   OFF ██████████│   ← dòng đang chọn: đảo màu
│ 3 OUT-3 PB14    OFF            │
│ 4 OUT-4 PB13     ON            │
│ 5 OUT-5 PB12    OFF            │
└────────────────────────────────┘
```

Mỗi dòng: `<số kênh> <tên> <tên chân>  <ON|OFF>`, các cột được căn thẳng bằng `%-5s`,
`%-4s`, `%3s`.

Tên và tên chân lấy trực tiếp từ `OUTn_NAME` / `OUTn_PIN_NAME` trong `pin_config.h` — nhãn
trên màn hình không thể trôi khỏi bảng chân thật.

Dòng đang chọn được **tô trắng cả dải rồi ghi chữ đen đè lên**, nhìn rõ hơn hẳn so với một
dấu `>` đầu dòng.

---

### Trang 3 — DHT11 SENSOR

Chi tiết cảm biến kèm thanh mức rộng và thông tin độ tươi của số liệu.

```
┌────────────────────────────────┐
│ DHT11 SENSOR            BT 3/5 │
├────────────────────────────────┤
│ TEMP                      28°C │
│ ▐███████████░░░░░░░░░░░░░░░░░▌ │
│ HUMI                       65% │
│ ▐████████████████████░░░░░░░░▌ │
│ LAST OK 4s             BT PAIR │
└────────────────────────────────┘
```

| Phần tử | Ý nghĩa |
|---|---|
| `LAST OK <n>s` | Số giây kể từ lần đọc **thành công** gần nhất |
| `NO DATA` | Chưa bao giờ đọc được |
| `BT PAIR` / `BT ----` | Tình trạng liên lạc Bluetooth |

`LAST OK` là cách phân biệt **số đo tươi với số đo đã chết** (yêu cầu FR-18): khi cảm biến
hỏng, giá trị cũ vẫn hiện trên màn hình, và nếu không có mốc thời gian thì người dùng không
có cách nào biết. Con số cứ tăng dần = cảm biến đã ngừng trả lời.

Trang này **không in phần thập phân**: DHT11 chỉ có độ phân giải 1 °C / 1 %, byte thập phân
của nó luôn bằng 0 nên `.0` chỉ là con số trang trí giả.

---

### Trang 4 — LOG

12 sự kiện gần nhất, xem 5 dòng một lúc, cuộn bằng UP/DOWN.

```
┌────────────────────────────────┐
│ LOG      8-12/12        BT 4/5 │
├────────────────────────────────┤
│ 00:02 DHT OK                   │
│ 00:04 OUT3 ON                  │
│ 00:04 DHT OK                   │
│ 00:06 OUT3 OFF                 │
│ 00:06 DHT OK                   │
└────────────────────────────────┘
```

| Thuộc tính | Giá trị |
|---|---|
| Sức chứa | **12** dòng (`UI_LOG_LINES`) |
| Hiển thị cùng lúc | **5** dòng |
| Độ dài mỗi dòng | 15 ký tự (cắt bớt nếu dài hơn) |
| Định dạng | `mm:ss <nội dung>`, mốc thời gian kể từ khi khởi động |
| Vị trí mặc định | Bám đáy — dòng mới nhất luôn ở cuối màn hình |
| `8-12/12` trên tiêu đề | Đang xem dòng 8 đến 12 trong tổng 12 |
| Khi chưa có gì | Hiện `(EMPTY)` |

Các sự kiện được ghi:

| Nội dung | Khi nào |
|---|---|
| `BOOT OK` | Khởi động xong |
| `BT LINK UP` | Nhận được byte đầu tiên từ module Bluetooth |
| `DHT OK` / `DHT FAIL` | Sau mỗi phép đo cảm biến (2 giây một lần) |
| `OUTn ON` / `OUTn OFF` | Mỗi lần một kênh đổi trạng thái, **từ cả hai đường điều khiển** |

> Lưu ý khi dùng: `DHT OK` được ghi mỗi 2 giây nên nhật ký 12 dòng bị lấp đầy trong khoảng
> **24 giây**. Muốn xem lại một sự kiện `OUTn` thì phải xem ngay. Xem
> [12](12-han-che-va-huong-phat-trien.md) §12.1.

---

### Trang 5 — HƯỚNG DẪN

Nội dung tĩnh, nhắc nhanh tập lệnh và vai trò 5 nút.

```
┌────────────────────────────────┐
│ HUONG DAN               BT 5/5 │
├────────────────────────────────┤
│ BTN: NEXT PREV UP DN           │
│      OK = BAT/TAT              │
│ CMD: ON n / OFF n              │
│      ON ALL/OFF ALL            │
│      STATUS TEMP HUM           │
└────────────────────────────────┘
```

Danh sách lệnh ở đây **phải khớp** với `Command_Menu[]` trong `main.c` — thêm lệnh mới thì
phải sửa cả hai chỗ (xem [06](06-giao-thuc-bluetooth.md) §6.9).

## 7.3 Bảng hành vi của nút theo trang

| Nút | HOME | OUTPUTS | SENSOR | LOG | HƯỚNG DẪN |
|---|---|---|---|---|---|
| **NEXT** (PA5) | → OUTPUTS | → SENSOR | → LOG | → HƯỚNG DẪN | → HOME |
| **PREV** (PB1) | → HƯỚNG DẪN | → HOME | → OUTPUTS | → SENSOR | → LOG |
| **UP** (PB0) | con trỏ lên 1 kênh (vòng) | con trỏ lên 1 kênh (vòng) | con trỏ lên | **cuộn về quá khứ** | con trỏ lên |
| **DOWN** (PA6) | con trỏ xuống 1 kênh (vòng) | con trỏ xuống 1 kênh (vòng) | con trỏ xuống | **cuộn về hiện tại** | con trỏ xuống |
| **OK** (PA7) | bật/tắt kênh đang chọn | bật/tắt kênh đang chọn | bật/tắt kênh đang chọn | **nhảy về bám đáy** | bật/tắt kênh đang chọn |

Con trỏ chọn kênh là **một biến dùng chung cho mọi trang** (`ui_selected_output`): chọn kênh
3 ở trang OUTPUTS rồi chuyển sang HOME thì ô số 3 vẫn đang được khung bao.

Nút OK ở trang SENSOR và HƯỚNG DẪN vẫn bật/tắt kênh đang chọn dù trang không hiển thị con
trỏ — hệ quả của việc `UI_HandleEvent()` xử lý LOG như ngoại lệ duy nhất.

## 7.4 Nhịp vẽ lại màn hình

| Điều kiện | Hành vi |
|---|---|
| Có sự kiện nút | Vẽ lại **ngay** ở lượt vòng lặp kế tiếp |
| Có dòng nhật ký mới **và** đang ở trang LOG | Vẽ lại ngay |
| Không có gì xảy ra | Vẽ lại mỗi **500 ms** |

Một khung hình đẩy khoảng 1 KB qua I2C 400 kHz, mất **~25 ms** và **chặn** vòng lặp trong
thời gian đó. Đây là lý do màn hình không vẽ lại liên tục.

## 7.5 Chống dội phím

Mỗi nút là một máy trạng thái hai mức với **mốc thời gian riêng**:

```mermaid
stateDiagram-v2
    [*] --> Nhả
    Nhả --> Nhấn: cạnh EXTI + đã qua 25ms<br/>+ đọc lại chân = LOW<br/>➜ SINH SỰ KIỆN
    Nhấn --> Nhả: cạnh EXTI + đã qua 25ms<br/>+ đọc lại chân = HIGH<br/>(không sinh sự kiện)
    Nhấn --> Nhả: UI_ReleaseStaleButtons()<br/>gỡ trạng thái kẹt
```

Bốn quyết định thiết kế đằng sau:

1. **Bắt cả hai cạnh**, không chỉ cạnh xuống. Tiếp điểm cơ khí nảy cả lúc nhấn **lẫn** lúc
   nhả; nếu chỉ bắt cạnh xuống thì tiếng nảy lúc nhả — cũng toàn cạnh xuống — không phân
   biệt được với một cú bấm mới.
2. **Cạnh chỉ là lời mời đi kiểm tra; mức của chân mới là sự thật.** Hết 25 ms, ISR đọc lại
   `HAL_GPIO_ReadPin()` để chốt trạng thái.
3. **Sự kiện chỉ sinh ở lần chuyển NHẢ → NHẤN.** Nhả tay chỉ mở khoá cho cú bấm sau.
4. **Mốc thời gian riêng cho từng nút.** Một mốc dùng chung sẽ nuốt mất thao tác hai nút
   liên tiếp — chọn kênh bằng UP rồi bấm OK ngay sau đó.

**Vì sao 25 ms**: tiếp điểm nút bấm phổ thông hết nảy trong 5–10 ms. Không đặt lớn hơn
nhiều được, vì khoảng này cũng là thời gian tối thiểu của một cú chạm — nhấn rồi nhả nhanh
hơn thế thì lần nhả sẽ bị bỏ qua.

**`UI_ReleaseStaleButtons()`** chạy mỗi vòng lặp chính để gỡ trạng thái kẹt: nếu cạnh lúc
nhả tay rơi đúng vào lúc chân đang nảy và bị đọc nhầm thành vẫn-đang-nhấn, sẽ không còn
cạnh nào tới nữa và nút kẹt vĩnh viễn. Hàm này **cố ý chỉ đi theo chiều nhả và không bao
giờ sinh sự kiện** — nếu cả ISR lẫn vòng lặp chính cùng sinh được sự kiện thì hai bên có
thể chen nhau và đếm một cú bấm thành hai.

## 7.6 Ranh giới trách nhiệm

**UI không bao giờ chạm GPIO của thiết bị.** Nó chỉ điền một đề nghị:

```c
typedef struct {
    bool    toggle_output;   /* true = xin đảo trạng thái một kênh */
    uint8_t channel;         /* Kênh cần đảo, 0..OUT_COUNT-1 */
} UI_Request_t;              /* ui.h:40-43 */
```

`main.c` đọc đề nghị này và thi hành bằng `Set_Output()`. Ba hệ quả:

1. Chiều phụ thuộc một hướng: `main.c` → `ui.h`, không có chiều ngược lại.
2. Mọi lối vào bật/tắt — nút bấm **và** lệnh Bluetooth — đều đi qua đúng một hàm (FR-06).
3. ISR không đổi trạng thái thiết bị, nên không phải ghi nhật ký từ trong ngắt.

Mỗi lượt `UI_Task()` chỉ chở về **một** đề nghị; các sự kiện còn lại nằm yên trong hàng đợi
và được xử lý ở vòng lặp kế — chỉ vài chục micro-giây sau.

## 7.7 Màn hình khởi động

Trước khi vào vòng lặp chính, `UI_Init()` vẽ một màn hình chào:

```
┌────────────────────────────────┐
│ BOOTING                        │
├────────────────────────────────┤
│ STM32 BT NODE                  │
│                                │
│ 5 OUTPUTS  5 BUTTONS           │
│                                │
│ PA5/PB1 = DOI TRANG            │
└────────────────────────────────┘
```

Màn hình này bị thay ngay ở lần vẽ đầu tiên của `UI_Task()` (trong vòng 500 ms), nên nếu nó
**đứng yên** thì hệ thống đã treo ở đâu đó sau `UI_Init()`.
