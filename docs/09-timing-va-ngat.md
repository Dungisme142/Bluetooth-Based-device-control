# 09 — Timing và thiết kế ngắt

Tài liệu này giải thích **vì sao** các con số timing trong firmware được chọn như vậy —
phần khó nhất và cũng đáng chấm điểm nhất của một hệ thống bare-metal.

## 9.1 Bài toán trung tâm

Hệ thống có bốn nguồn sự kiện chạy song song trên **một** lõi CPU không có RTOS:

| Nguồn | Ràng buộc thời gian | Hậu quả nếu trễ |
|---|---|---|
| Giải mã bit DHT11 | **77–124 µs** giữa hai sườn | Đọc sai bit → hỏng cả phép đo |
| Nhận byte UART | ~1 ms một byte ở 9600 baud | Tràn ORE → mất byte |
| Nút bấm | Hàng chục ms (giới hạn bởi con người) | Không cảm nhận được |
| Vẽ màn hình | 500 ms một khung | Không ai để ý |

Khoảng cách giữa yêu cầu chặt nhất (77 µs) và lỏng nhất (500 ms) là **hơn 6000 lần**. Toàn
bộ thiết kế ưu tiên ngắt xuất phát từ chênh lệch này.

## 9.2 Ngân sách thời gian của vòng lặp chính

| Tác vụ | Tần suất | Thời gian chặn | Ghi chú |
|---|---|---|---|
| `DHT11_ReadOnce()` | 2 s một lần | **~25 ms** bình thường, **tới 500 ms** khi cảm biến không đáp | Tác vụ nặng nhất |
| `SSD1306_UpdateScreen()` | ≤ 2 lần/s | **~25 ms** | 1 KB qua I2C 400 kHz, blocking |
| `Send_Status()` | 3 s một lần | < 10 µs | Chỉ nạp bộ đệm rồi `Transmit_IT` |
| `UART_Task()` | mỗi vòng lặp | < 100 µs | Rút bộ đệm + tra bảng lệnh |
| `Fill_UI_Data()` | mỗi vòng lặp | < 10 µs | Sao chép vài biến |
| Còn lại | mỗi vòng lặp | < 10 µs | So sánh tick, đảo LED |

**Chu kỳ vòng lặp điển hình**: dưới 200 µs khi không có gì đến hạn — tức vòng lặp quay
khoảng 5000 lần mỗi giây. Trường hợp xấu nhất: **~525 ms** (đọc cảm biến quá hạn + một
khung hình OLED rơi vào cùng lượt).

Đó là lý do **NFR-03**: chỉ được có một tác vụ chặn dài. Thêm một tác vụ 500 ms nữa thì
trường hợp xấu nhất thành 1 giây, vượt sức chứa của bộ đệm nhận 128 byte và làm giao diện
giật thấy rõ.

### Vì sao vẫn chấp nhận được

Không tác vụ nào trong vòng lặp có **hạn chót cứng**. Việc thật sự gấp — đo độ rộng xung
DHT11 — nằm hoàn toàn trong ISR và **không chờ vòng lặp**. Vòng lặp chính chỉ làm những
việc mà trễ 500 ms cũng không sao: vẽ màn hình, trả lời lệnh, đảo LED.

## 9.3 Thang ưu tiên ngắt

```
   ưu tiên cao
        ▲   TIM2      (2)  ── watchdog + chuyển pha FSM của DHT11
        │   EXTI4     (5)  ── giải mã bit DHT11        ← ràng buộc 77 µs
        │   USART2    (6)  ── byte Bluetooth           ← ràng buộc 1 ms
        │   EXTI0/1/9_5 (7) ── nút bấm                 ← ràng buộc hàng chục ms
        ▼   SysTick   (15) ── HAL_GetTick()
   ưu tiên thấp
```

Thứ tự này **đúng bằng** thứ tự chặt-lỏng của các ràng buộc ở §9.1. Đó không phải trùng
hợp — đây là nguyên tắc *deadline monotonic*: việc có hạn chót ngắn nhất được ưu tiên cao nhất.

Ràng buộc bất biến `TIM2 < EXTI4 < USART2 < EXTI(nút)` được ghi thành comment ngay tại
`pin_config.h:34` và `pin_config.h:178` để lần sửa sau không vô tình phá vỡ.

### Vì sao DHT11 phải cao hơn UART

Xét trường hợp ngược lại — UART ưu tiên cao hơn:

1. DHT11 đang truyền bit, hai sườn cách nhau 77 µs.
2. Một byte Bluetooth về, ISR USART2 chèn vào.
3. ISR USART2 chạy ~5–10 µs (đẩy 1 byte vào ring buffer) — **tự nó không nguy hiểm**.
4. Nhưng ISR EXTI4 bị hoãn, và nó **đo thời gian bằng cách đọc bộ đếm TIM2 lúc vào ISR**.
   Trễ 10 µs làm phép đo lệch 10 µs.
5. Dải phân loại bit 0 là 60–100 µs, bit 1 là 100–140 µs. Một xung 98 µs (bit 0) bị đo
   thành 108 µs → đọc thành **bit 1**.

Kết quả: sai lặng lẽ một bit, checksum bắt được nên chỉ hiện `DHT FAIL` — nhưng nếu bit sai
rơi đúng vào byte checksum thì lỗi lọt qua và số đo sai được hiển thị. Rất khó truy.

Với thứ tự hiện tại, tình huống đảo ngược: ISR USART2 bị EXTI4 chèn, byte đến sau có thể
tràn ORE. Nhưng hậu quả đó **đã được xử lý**:

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)  /* main.c:775 */
{
    (void)huart2.Instance->DR;                       /* xoá cờ ORE */
    (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1u);   /* mở lại phiên nhận */
}
```

Không có callback này, HAL huỷ luôn phiên `Receive_IT` khi gặp ORE, RXNE không bao giờ nổi
lại, và **Bluetooth "chết câm" vĩnh viễn** sau lần tràn đầu tiên trong khi mọi thứ khác vẫn
chạy bình thường. Đây là một trong những lỗi khó tìm nhất của thiết kế này, và nó là **hệ
quả trực tiếp** của việc chọn ưu tiên DHT11 cao hơn UART — một đánh đổi có ý thức, kèm biện
pháp bù.

### Vì sao TIM2 cao hơn cả EXTI4

TIM2 làm watchdog. Nếu bus DHT11 đứt giữa phiên, chỉ có watchdog mới đưa FSM thoát khỏi
`READING`. Để watchdog bị chèn bởi chính ngắt mà nó đang giám sát là vô nghĩa.

### Vì sao nút bấm thấp nhất

ISR nút chỉ làm hai việc: kiểm tra mốc chống dội và đẩy một byte vào hàng đợi. Hoãn nó vài
chục µs không ai cảm nhận được, và nó **tuyệt đối không được** làm trễ việc giải mã bit.

Đây cũng là lý do **không nút nào được đặt lên EXTI line 4**: line đó là vector riêng của
DHT11. Nút dùng chung vector sẽ khiến mỗi cú bấm giữa lúc đo cảm biến làm hỏng phép đo.

## 9.4 Ba quyết định timing khác

### Hoãn `UI_Log("BT LINK UP")` ra khỏi ISR

```c
/* main.c:224-230 — trong vòng lặp chính, KHÔNG trong RxCpltCallback */
if (bluetooth_connected && !bluetooth_logged) {
    bluetooth_logged = true;
    UI_Log("BT LINK UP");
}
```

`UI_Log()` ghi vào chính bộ đệm vòng mà `UI_Task()` đang đọc để vẽ. Gọi từ ISR thì nó có thể
chen vào giữa lúc `UI_DrawLogPage()` đang duyệt bộ đệm — nội dung hiển thị bị lẫn, và chỉ
số vòng có thể lệch. Cờ `bluetooth_connected` được ISR đặt; việc ghi nhật ký được hoãn ra
vòng lặp (chậm nhất vài trăm µs sau).

### Dùng hiệu `now - last`, không dùng `now >= last + T`

```c
if ((now_ms - last_sensor_tick_ms) >= SENSOR_PERIOD_MS) { ... }
```

`HAL_GetTick()` là `uint32_t` và tràn sau **~49,7 ngày**. Phép trừ unsigned vẫn cho kết quả
đúng khi tràn (số học modulo 2³²); phép cộng `last + T` thì tràn và điều kiện thành sai
vĩnh viễn. Một thiết bị điều khiển thiết bị nhà là thứ có thể chạy liên tục nhiều tháng —
lỗi này sẽ xuất hiện đúng vào ngày thứ 50 và không ai đoán ra nguyên nhân.

Quy tắc này áp dụng nhất quán ở **mọi** chỗ so mốc thời gian trong project.

### Mỗi lượt vòng lặp chỉ thực thi một lệnh UART

`UART_Print()` dùng một bộ đệm phát duy nhất với `HAL_UART_Transmit_IT`. Nếu xử lý liền hai
lệnh trong cùng một lượt, câu trả lời thứ hai ghi đè lên câu thứ nhất khi nó còn đang được
gửi. `UART_Task()` trả quyền về vòng lặp ngay sau khi phát một lệnh (`uart.c:287-292`) để
HAL kịp gửi xong.

Đổi lại, nếu app gửi liên tiếp nhiều lệnh rất nhanh, một số câu trả lời sẽ bị bỏ (xem
`UART_Print()` kiểm tra `gState != HAL_UART_STATE_READY`). Xem
[12](12-han-che-va-huong-phat-trien.md) §12.1.

## 9.5 Đánh đổi: I2C blocking

`SSD1306_UpdateScreen()` đẩy 1024 byte framebuffer qua I2C 400 kHz bằng
`HAL_I2C_Master_Transmit` — **blocking ~25 ms**.

| Phương án | Ưu | Nhược |
|---|---|---|
| **Blocking (đang dùng)** | Driver đơn giản, không cần đồng bộ, không có trạng thái dở dang | Chặn vòng lặp 25 ms mỗi khung |
| I2C theo ngắt | Không chặn | Phải xử lý "đang gửi khung dở" trong khi framebuffer có thể bị ghi tiếp |
| I2C + DMA | Gần như không tốn CPU | Thêm cấu hình DMA, vẫn phải khoá framebuffer |

Chọn blocking vì 25 ms nằm gọn trong ngân sách: nó nhỏ hơn nhiều so với 500 ms của DHT11,
và các ISR quan trọng vẫn chạy bình thường trong lúc I2C đang chặn — **blocking chỉ chặn
vòng lặp chính, không chặn ngắt**.

Hai biện pháp giảm tác động:

1. Vẽ lại tối đa 2 lần/giây khi không có thao tác (`UI_REFRESH_PERIOD_MS` = 500 ms).
2. Có sự kiện nút thì vẽ ngay — người dùng thấy phản hồi tức thì, không phải chờ hết chu kỳ.

## 9.6 Các hằng số timing tập trung

| Hằng số | Giá trị | Nơi khai báo | Ý nghĩa |
|---|---|---|---|
| `SENSOR_PERIOD_MS` | 2000 | `main.c:30` | Chu kỳ đọc DHT11 |
| `STATUS_PERIOD_MS` | 3000 | `main.c:31` | Chu kỳ phát trạng thái |
| `HEARTBEAT_PERIOD_MS` | 1000 | `main.c:32` | Chu kỳ đảo LED PC13 |
| `DHT11_POLL_INTERVAL_MS` | 5 | `main.c:35` | Nhịp hỏi FSM cảm biến |
| `DHT11_POLL_TIMEOUT_MS` | 500 | `main.c:36` | Hạn chót một phép đo |
| `UI_REFRESH_PERIOD_MS` | 500 | `ui.c:51` | Chu kỳ vẽ lại màn hình |
| `BTN_DEBOUNCE_MS` | 25 | `pin_config.h:155` | Chống dội phím |
| `UART_FRAME_IDLE_TIMEOUT_MS` | 250 | `uart.h:28` | Tự chốt khung khi app không gửi CR/LF |
| `UART_LINK_TIMEOUT_MS` | 10000 | `uart.h:31` | Coi như mất kết nối Bluetooth |
| `DHT11_START_LOW_TIME_US` | 18000 | `DHT11.h:39` | Độ dài lệnh Start |
| `DHT11_WATCHDOG_US` | 500 | `DHT11.c:19` | Watchdog trong phiên đo |
| `TIM2_COUNTER_FREQ_HZ` | 1000000 | `main.c:45` | Tần số bộ đếm µs |

**Không có hằng số timing nào nằm rải rác trong code** — mọi con số đều có tên và đều nằm ở
một trong ba nơi trên.

## 9.7 Tóm tắt cho người bảo trì

Bốn điều không được phá vỡ:

1. **Thứ tự ưu tiên** `TIM2 < EXTI4 < USART2 < EXTI(nút)`.
2. **Không thêm tác vụ chặn dài thứ hai** vào vòng lặp chính.
3. **Không gọi `UI_Log()` hay I2C từ ISR.**
4. **Không so mốc thời gian bằng phép cộng** — luôn dùng hiệu.

Đổi cây clock (`SystemClock_Config`) thì phải rà lại toàn bộ tính toán baud và prescaler
timer. `MX_TIM2_Init()` đã tự bảo vệ bằng `Error_Handler()` nếu không chia ra được 1 MHz,
nhưng UART thì không có kiểm tra tương đương.
