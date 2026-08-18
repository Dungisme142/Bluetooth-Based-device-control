# Convention Review Code — Bluetooth-Based Device Control

Bộ quy tắc review cho firmware STM32F103xB bare-metal (HAL, không RTOS) của repo này.
Mọi ví dụ Đạt/Không đạt đều lấy từ code thật trong `src/` và `lib/`, không phải lý thuyết chung.

Mức độ: 🔴 **chặn merge** · 🟡 **cần sửa trước khi đóng PR** · ⚪ **khuyến nghị**

---

## 0. Cách dùng & nguyên tắc nền

### 0.1 Nguyên tắc nền

> **Review ở đây là để tối ưu và kết nối code đã có, không phải để xoá đi xây lại.**
> Các thư viện trong `lib/` là do tác giả tự viết và **được giữ lại**. Mọi đề xuất phải ở dạng
> *"sửa chỗ này trong module đang có"*, không được ở dạng *"bỏ module này, thay bằng thư viện khác"*
> hay *"viết lại từ đầu cho sạch"*.

Hệ quả cụ thể khi review:

- Thấy `Ring_Buffer.c` / `Frame_Builder.c` / `Text_Filter.c` chưa tối ưu ⇒ đề xuất **tách hàm phụ, gộp nhánh, đổi tên hằng** ngay trong file đó. Không đề xuất thay bằng ring buffer của CMSIS/FreeRTOS.
- Thấy hai module rời rạc ⇒ đề xuất **nối lại** (đưa init của module con vào init của module cha — xem mục 7B). Không đề xuất viết lớp bọc mới chồng lên.
- Đề xuất phải **surgical**: nêu rõ sửa hàm nào, dòng nào; giữ nguyên API công khai nếu có thể để các chỗ gọi không phải đổi.
- Chỉ khi một module vừa sai chức năng vừa không cứu được thì mới nói tới viết lại — và phải nêu rõ lý do, không mặc định chọn đường đó.

### 0.2 Checklist rút gọn (copy vào PR description)

```markdown
### Review checklist
- [ ] 1. ISR: biến chia sẻ có `volatile`; ISR không blocking; error callback đã cài
- [ ] 2. Timing: không `HAL_Delay`; so sánh tick dạng `(now - last) >= T`; mọi vòng chờ có timeout
- [ ] 3. Kiểu: `<stdint.h>`, hậu tố `u`, cast tường minh, không float vô cớ
- [ ] 4. Enum: hằng `UPPER_SNAKE` + tiền tố; phần tử đầu `= 0`; không magic number; `switch` đủ nhánh
- [ ] 5. Bộ nhớ: không malloc; `snprintf` + `sizeof`; dữ liệu chạy không nằm ở vị trí format string
- [ ] 6. Scope: mọi thứ không có trong header đều `static`; `lib/` không biết về app
- [ ] 7. Module: hàm nằm đúng file (7A); module một-người-dùng đã gộp (7B)
- [ ] 8. Trùng lặp: không có tri thức nào bị mã hoá ở 2 chỗ trở lên
- [ ] 9. Phần cứng: chân qua `pin_config.h`; kiểm return của `HAL_*`
- [ ] 10. Lỗi: mọi đường lỗi về trạng thái xác định; input ngoài coi là không tin cậy
- [ ] 11. Style: naming thống nhất; comment tiếng Việt **có dấu**; Doxygen cho hàm public
- [ ] 12. Build: sạch `-Wall -Wextra`; không commit `build/`, `local/`
```

---

## 1. ISR & concurrency 🔴

Mục nặng nhất của một project bare-metal. Lỗi ở đây không crash ngay mà biểu hiện thành "thỉnh thoảng treo", rất khó truy.

### Biến chia sẻ giữa ISR và main loop **phải** `volatile`

Thiếu `volatile`, trình biên dịch được phép giữ biến trong thanh ghi và không bao giờ đọc lại từ RAM — vòng lặp chính sẽ chờ một giá trị vĩnh viễn không đổi.

❌ **Không đạt** — `lib/Src/DHT11.c:10`

```c
DHT11_Data_t dht_data = {0};      /* ISR ghi, main loop đọc — thiếu volatile */
```

`DHT11_ParseData()` chạy trong ngữ cảnh ngắt ghi vào struct này, `DHT11_ReadData()` ở main loop đọc ra. Struct 6 byte nên **không nguyên tử**: main loop có thể đọc được nửa cũ nửa mới nếu ngắt chen vào giữa. Cùng file, `g_dht_pin` (dòng 14) cũng chia sẻ với ISR mà không `volatile`.

✅ **Đạt** — `src/ui.c:70-73`

```c
/* ISR ghi hai bien nay, vong lap chinh doc chung. Deu la 1 byte nen tren
 * Cortex-M3 moi lan doc/ghi la mot lenh don, khong the bi cat doi. */
static volatile uint8_t ui_current_page;
static volatile uint8_t ui_redraw_pending;
```

Đúng cả hai vế: có `volatile`, **và** comment nói rõ vì sao ở đây không cần critical section.

> **Luật rút ra**: ≤ 1 byte trên Cortex-M3 thì `volatile` là đủ. Lớn hơn 1 byte (struct, `uint32_t` đọc theo cụm, mảng) thì phải bọc critical section:
> ```c
> uint32_t primask = __get_PRIMASK();
> __disable_irq();
> /* ... đọc/ghi ... */
> __set_PRIMASK(primask);   /* KHÔNG dùng __enable_irq() trần: sẽ bật ngắt cả khi trước đó đang tắt */
> ```

### ISR chỉ làm việc tối thiểu

Trong ISR: **không** hàm blocking, **không** `HAL_Delay`, **không** `printf`/`snprintf`, **không** cấp phát động, **không** chờ cờ phần cứng.

✅ Repo hiện đạt — grep toàn bộ, `HAL_Delay` chỉ còn xuất hiện trong comment.

✅ **Mẫu chuẩn** — `src/main.c:317-327`: ISV UART chỉ đẩy 1 byte vào ring buffer rồi mở lại phiên nhận. Toàn bộ việc dựng khung, tra bảng lệnh, gửi trả lời đều nằm ở `UART_Task()` trong main loop.

✅ **Mẫu chuẩn thứ hai** — `src/ui.c:161-164`:

```c
/* Chi bao "phai ve lai"; viec ve that su do UI_Task() lam o vong lap chinh
 * vi SSD1306_UpdateScreen() blocking khoang 25 ms tren I2C. */
ui_redraw_pending = 1u;
```

ISR nút bấm chỉ dựng cờ. Nếu vẽ thẳng trong ISR, 25 ms chiếm CPU đó sẽ nuốt mất hàng chục byte UART.

### Thứ tự ưu tiên NVIC phải được ghi lại kèm lý do

✅ **Đạt** — `src/stm32f1xx_it.c:12-17` liệt kê toàn bộ bảng ưu tiên ở đầu file:

```
TIM2        2   watchdog/timeout cua DHT11
EXTI15_10   5   giai ma bit DHT11 — PHAI cao hon UART
USART1      6   nhan lenh Bluetooth
EXTI0/1     7   nut NEXT/PREV chuyen trang UI
SysTick    15   TICK_INT_PRIORITY, thap nhat
```

Review PR đụng vào ngắt: kiểm bảng này còn đúng không, và số ưu tiên mới có làm đảo thứ tự không.

### Mọi callback lỗi của HAL phải được cài 🔴

Đây là loại lỗi "chết câm" đặc trưng của HAL: khi USART1 tràn (ORE), `HAL_UART_IRQHandler` báo lỗi và **huỷ luôn phiên `Receive_IT`**. Không bắt lại thì RXNE không bao giờ nổi nữa — Bluetooth ngừng hẳn trong khi mọi thứ khác vẫn chạy bình thường, LED heartbeat vẫn nhấp nháy.

✅ **Đạt** — `src/main.c:337-346`:

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* Doc SR roi DR la trinh tu xoa co ORE tren F1 */
        (void)huart->Instance->DR;
        HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1u);
    }
}
```

Nguyên nhân gốc của ORE ở repo này nằm ở mục 2 — main loop có lúc bị chặn 500 ms.

---

## 2. Timing & main loop 🔴🟡

### Không dùng `HAL_Delay` trong code ứng dụng 🔴

`HAL_Delay` khoá CPU và phụ thuộc SysTick. Dùng hiệu `HAL_GetTick()`.

✅ **Đạt** — `src/main.c:229-234`:

```c
/* Giu nhip poll bang hieu tick thay vi HAL_Delay: khong khoa CPU
 * trong SysTick handler va khong phu thuoc do phan giai cua no. */
if ((HAL_GetTick() - last_poll_ms) < DHT11_POLL_INTERVAL_MS) {
    continue;
}
last_poll_ms += DHT11_POLL_INTERVAL_MS;
```

### So sánh tick **luôn** viết dạng `(now - last) >= T` 🔴

`HAL_GetTick()` là `uint32_t`, tràn sau ~49,7 ngày. Phép trừ unsigned vẫn cho kết quả đúng khi tràn; phép cộng thì không.

```c
if ((now - last) >= PERIOD) { ... }        /* ✅ đúng cả khi tick tràn */
if (now >= last + PERIOD) { ... }          /* ❌ sai khi last + PERIOD tràn */
```

✅ Repo đang theo đúng ở toàn bộ 5 chỗ: `src/main.c:117,122,129`, `src/ui.c:121,148`, `lib/Src/uart.c:40,111`.

### Ngân sách thời gian của mỗi tác vụ trong superloop 🟡

Superloop chỉ chạy đúng khi không tác vụ nào chiếm CPU quá lâu. PR thêm tác vụ mới phải nêu được thời gian tệ nhất của nó.

⚠️ **Nợ kỹ thuật đang có** — ghi lại ở đây để không ai vô tình làm nặng thêm:

| Tác vụ | Thời gian chặn | Vị trí |
|---|---|---|
| `Dht11_ReadOnce()` | tới **500 ms** | `src/main.c:214` |
| `SSD1306_UpdateScreen()` | ~**25 ms** | qua `UI_Task()` |

Ở 9600 baud (1 byte ≈ 1 ms), 500 ms chặn nghĩa là tới 500 byte đổ về trong lúc main loop không rút được — đây chính là nguồn ORE ở mục 1. Hai chỗ này đã được bù bằng ring buffer 128 byte và vòng `while` rút liên tục (`lib/Src/uart.c:85-88`), nhưng **đừng thêm tác vụ blocking thứ ba**.

### Mọi vòng chờ phải có đường thoát 🔴

Không viết `while (flag) { }` trần. Mẫu đúng: `src/main.c:226` — vòng `while` bao bởi `DHT11_POLL_TIMEOUT_MS` và trả `0u` khi hết giờ.

---

## 3. Kiểu dữ liệu & phép toán 🟡

- Dùng `<stdint.h>`: `uint8_t`, `uint16_t`, `int32_t`. Không dùng `int`/`long` trần cho dữ liệu phần cứng — kích thước phụ thuộc trình biên dịch.
- **Hằng unsigned có hậu tố `u`**: `500u`, `1u`. Repo đã tự áp dụng ở `src/`; `lib/` còn vài chỗ thiếu (`Ring_Buffer.c:12-13`, `Frame_Builder.c:12`).
- **So sánh tường minh** thay vì truthiness: `if (x != 0u)` chứ không `if (x)`. Đạt: `src/main.c:140`, `src/ui.c:123`.
- **Cast tường minh** khi thu hẹp kiểu: `(uint8_t)(page + 1u)`, `(unsigned)data->temperature_c`. Đạt: `src/ui.c:155-157`, `src/main.c:266`.
- **Cảnh giác integer promotion**: `uint8_t a, b;` thì `a + b` được nâng lên `int`. Gán ngược về `uint8_t` phải cast, và phải chắc kết quả không tràn.
- **Không dùng float trên Cortex-M3** (không có FPU) trừ khi có lý do rõ ràng — mỗi phép toán là một lời gọi thư viện phần mềm. Ưu tiên số nguyên hoặc fixed-point. Repo hiện không dùng float ở đâu cả — giữ nguyên như vậy.
- **`(void)param;` cho tham số không dùng** để `-Wunused-parameter` không kêu. Đạt: các handler lệnh ở `src/main.c:275,282,...`.

---

## 4. Enum & mã trạng thái 🟡

Repo dùng enum đúng tinh thần (hàm trả `Developer_Action_Result_t` thay vì `int`; trường struct khai kiểu enum chứ không `uint8_t`), nhưng **quy ước đặt tên đang có ba style song song**.

### Đặt tên: type `Module_Xxx_t`, hằng số `UPPER_SNAKE` có tiền tố trùng tên type 🔴

✅ **Đạt** — `lib/Inc/DHT11.h:8-15`:

```c
typedef enum {
    DHT11_STATE_IDLE = 0,
    DHT11_STATE_START_LOW,
    ...
} DHT11_State_t;
```

❌ **Không đạt** — `lib/Inc/Framing_Base.h:7-13` và `lib/Inc/uart.h:18-21`:

```c
typedef enum{
    filting_result_save_and_continue,     /* viết thường — tại chỗ dùng nhìn như biến local */
    ...
}Filting_Result_t;

typedef enum{
    developer_uart_connected,             /* viết thường */
    developer_uart_disconnected
}Developer_UART_Connecting_Status_t;
```

Tên viết thường làm mất tín hiệu thị giác "đây là hằng số"; đọc `if (x == developer_uart_connected)` không phân biệt được với so sánh hai biến.

### Phần tử đầu tiên gán `= 0` tường minh 🟡

Để giá trị 0 là **chủ ý** (thường là IDLE / thành công) chứ không phải tình cờ, và để struct `= {0}` cho ra trạng thái đúng. `Filting_Result_t` và `Developer_UART_Connecting_Status_t` hiện thiếu.

### Không khai báo giá trị enum rồi bỏ không dùng 🟡

❌ `DEV_ON_PROGRESS` — `lib/Inc/Global_Enum.h:12`. Grep toàn repo: không chỗ nào dùng. Hoặc dùng nó cho các hàm non-blocking (`Frame_Building` khi khung chưa xong là ứng viên tự nhiên), hoặc xoá.

### Chỉ một cách biểu diễn boolean cho toàn project 🟡

Hiện tồn tại song song hai cách cho cùng một khái niệm:

| Cách | Khai ở | Dùng ở |
|---|---|---|
| `Developer_Logic_t` (`DEV_TRUE`/`DEV_FALSE`) | `lib/Inc/Global_Enum.h:4-7` | `Framing_Base.h:20`, `Frame_Builder.c`, `uart.c` |
| `bool` của `<stdbool.h>` | — | `DHT11_Data_t.is_valid`, `lib/Inc/DHT11.h:24` |

**Chốt**: dùng `bool` cho đúng/sai thuần tuý; giữ enum cho trạng thái **nhiều hơn hai giá trị** hoặc có ngữ nghĩa riêng. Đây là nợ kỹ thuật cần một lần thống nhất — nhưng theo nguyên tắc 0.1, làm bằng cách đổi kiểu tại chỗ, không đụng logic.

### Đã có enum thì không trả magic number 🟡

❌ **Không đạt** — `src/main.c:244,246,254`: `Dht11_ReadOnce()` trả `1u`/`0u` kiểu `uint8_t` trong khi `Developer_Action_Result_t` đã tồn tại sẵn cho đúng mục đích này. Chỗ gọi (`src/main.c:118`) phải viết `== 1u`, đọc không biết 1 nghĩa là gì.

### `switch` trên enum: liệt kê đủ mọi giá trị, không `default` nuốt phần còn lại 🟡

Có `default`, thêm một state mới sẽ **im lặng** rơi vào đó — lỗi trôi sang runtime. Không có `default`, `-Wswitch` báo ngay lúc biên dịch mọi chỗ cần cập nhật.

⚠️ `src/ui.c:204-207` hiện gộp `case UI_PAGE_HELP:` với `default:`. Ở đây chấp nhận được vì `ui_current_page` là `uint8_t` (có thể mang giá trị ngoài enum), nhưng phải có comment nói rõ là cố ý.

### Ba luật riêng của embedded

- **Không dùng enum làm bitmask/cờ.** Giá trị enum là danh sách liên tiếp (0,1,2,3), không phải bit độc lập. Cần cờ thì dùng macro `(1u << n)` với kiểu `uint8_t`/`uint32_t`.
- **Không giả định kích thước enum.** ARM GCC thường cấp 4 byte nhưng `-fshort-enums` đổi được. ⇒ **không đặt enum trong struct ánh xạ khung truyền hoặc ghi flash**, **không gửi thẳng giá trị enum qua UART** — chuyển sang `uint8_t` tường minh ở biên giao tiếp.
- **Biến enum chia sẻ với ISR phải `volatile`** (mục 1). ✅ Đạt: `volatile DHT11_State_t current_state` — `lib/Src/DHT11.c:9`.

### Vị trí khai báo

Enum dùng chung nhiều module → `lib/Inc/Global_Enum.h`. Enum riêng của một module → header của module đó. ✅ Repo đang theo đúng.

---

## 5. Bộ nhớ 🔴

- **Cấm `malloc`/`free`** sau giai đoạn init. Buffer cấp phát tĩnh, kích thước bằng macro. ✅ Repo đạt: `uart_rx_buffer`, `uart_frame_buffer`, `ui_log` đều là mảng static với macro kích thước.
- **Luôn `snprintf` + `sizeof(buf)`**, không `sprintf`/`strcpy`. ✅ Đạt ở mọi chỗ. `strncpy` ở `src/ui.c:172-173` có chốt `'\0'` thủ công ngay sau — đúng, vì `strncpy` không tự kết thúc chuỗi khi nguồn dài bằng đích.
- **Dữ liệu chạy không bao giờ đặt ở vị trí format string** 🔴

Bug thật đã gặp trong repo này: chuỗi kết quả chứa `%` (từ `"HUM=61%"`); đưa thẳng vào vị trí format thì `vsnprintf` đọc `%` là đặc tả định dạng và lấy đối số không tồn tại.

```c
UART_Print(h, "%s", return_msg);   /* ✅ */
UART_Print(h, return_msg);         /* ❌ */
```

✅ Đã được phòng thủ ở hai lớp: gọi đúng cách tại `lib/Src/uart.c:68` và `src/main.c:265`, **và** `__attribute__((format(printf, 2, 3)))` ở `lib/Inc/uart.h:39-40` để GCC bắt ngay lúc biên dịch. Giữ nguyên attribute này khi sửa chữ ký hàm.

- **Kiểm biên mảng trước khi ghi.** ✅ Mẫu: `lib/Src/Frame_Builder.c:26` chặn theo `max_frame_size`; `lib/Src/Ring_Buffer.c:37` chặn khi buffer đầy.
- **Ước lượng stack** cho đường gọi sâu nhất; `_estack` khai trong `STM32F103xx_FLASH.ld`. Chú ý `tx_buffer[256]` trong `Developer_UART_HandleTypeDef` là static (nằm trong handle), không phải stack — đúng.

---

## 6. Scope, module & API 🟡

### Mọi biến/hàm không nằm trong header **phải** `static`

Không `static` thì ký hiệu rò ra link scope: va tên với module khác, và người đọc không biết được nó có đang bị ai đó ngoài file dùng hay không.

❌ **Không đạt** — `lib/Src/DHT11.c:9-14`: cả 6 biến đều global non-static.

```c
volatile DHT11_State_t current_state = DHT11_STATE_IDLE;   /* tên rất chung, dễ va */
DHT11_Data_t dht_data = {0};
volatile uint8_t raw_data[5] = {0};
volatile uint8_t bit_index = 0;
volatile uint8_t byte_index = 0;
DHT11_Config_t g_dht_pin;
```

`current_state`, `raw_data`, `bit_index` là những cái tên bất kỳ module nào cũng có thể đặt. Sửa: thêm `static` — không cái nào trong số này xuất hiện trong `DHT11.h`.

✅ **Đạt** — toàn bộ `src/ui.c`, `src/main.c`, và các hàm nội bộ như `UART_Frame_Reset` (`lib/Src/uart.c:48`), `Ring_Buffer_Status_SingleCheck` (`lib/Src/Ring_Buffer.c:22`).

### Không `extern` biến của module khác

⚠️ `lib/Src/DHT11.c:8`: `extern TIM_HandleTypeDef htim2;` — driver móc thẳng vào handle của `board.c`, tạo coupling ngầm không nhìn thấy từ header. Sửa đúng hướng (giữ module, chỉ nối lại cho tường minh): thêm `TIM_HandleTypeDef *htim` vào `DHT11_Config_t` và truyền vào từ `App_Init()`, giống cách `Port`/`Pin`/`IRQn` đang làm.

### `lib/` không được biết về app

Driver trong `lib/` không include `main.h`, không tham chiếu `system_state`, không hardcode chân. Chân lấy từ `lib/Inc/pin_config.h` — single source of truth, giữ nguyên vai trò này.

Câu hỏi kiểm tra: *"bê file này sang project khác, nó có kéo theo cái gì không thuộc về nó không?"*

### Hợp đồng của hàm public

- Trả `Developer_Action_Result_t` (`lib/Inc/Global_Enum.h`) thay vì `int` hoặc `void`.
- **Kiểm NULL đầu hàm** với mọi tham số con trỏ. ✅ Mẫu: `lib/Src/uart.c:14-16`, `Ring_Buffer.c:8`, `Frame_Builder.c:8`.
- Header có include guard, chỉ include cái nó thực sự cần.

---

## 7. Ranh giới module: đặt đúng chỗ & gộp đúng chỗ

Hai luật đối trọng nhau. 7A chống dồn hết vào một file; 7B chống băm nhỏ thành quá nhiều file.

### 7A — Phân bổ code đúng file 🔴

> **Mỗi hàm phải nằm trong file source đúng trách nhiệm của nó.**
> `main.c` chỉ chứa `main()`, vòng lặp chính, `App_Init()` và các HAL callback — **không chứa phần thân của bất kỳ tính năng nào**.

Tiêu chí đặt hàm: hàm thuộc về file mà nó **thao tác dữ liệu của file đó**. Handler lệnh thao tác trạng thái ứng dụng → file lệnh của tầng app. Hàm vẽ màn hình → `ui.c`. Hàm điều khiển relay → file relay.

❌ **Không đạt — ví dụ chính của mục này**

`Command_ON`, `Command_OFF`, `Command_STATUS`, `Command_TEMP`, `Command_HUM`, `Command_AUTO` cùng bảng `Command_Menu[]` đang nằm trong `src/main.c:75-94` và `src/main.c:273-313`.

Đáng chú ý: `lib/Inc/Command_Selector.h:18` **đã ghi rõ chỗ đúng**:

```c
/* Bang lenh cua ung dung — dinh nghia trong src/app_command.c */
extern Command_HandleTypeDef Command_Menu[];
```

`src/app_command.c` chưa được tạo — code đã trôi khỏi thiết kế mà chính tác giả đã khai báo.

✅ **Đúng** — tách sang `src/app_command.c` (+ `.h`), phân vai rõ **cơ chế** vs **nội dung**:

```
lib/Src/Command_Selector.c   cơ chế: Command_Selecting() tra bảng, khớp tiền tố, tách args
src/app_command.c            nội dung: Command_Menu[] + 6 handler, chạm system_state/relay1
src/main.c                   chỉ còn: main loop, App_Init(), các HAL callback
```

Ranh giới này là hệ quả trực tiếp của luật ở mục 6: nếu dồn handler vào `lib/Src/Command_Selector.c` thì file lib buộc phải `extern system_state`, `relay1`, `developer_uart_handler` từ `main.c` — vi phạm chính luật đó và làm module mất khả năng tái sử dụng.

**Đặt tên file**: file .c của tầng app dùng tiền tố `app_*`; file trong `lib/` đặt tên theo module. Một file = một trách nhiệm nêu được bằng một câu.

### 7B — Gộp module chỉ-một-người-dùng 🟡

Tách đúng chỗ là tốt, nhưng **băm thành nhiều file mà mỗi file chỉ được đúng một module khác dùng thì lại làm mất tính mạch lạc** — đọc một luồng phải nhảy qua 5 file.

> Nếu `X.h` chỉ được include bởi **đúng một** module Y (không kể `X.c`), thì X không phải là một module — nó là **chi tiết nội bộ của Y** ⇒ gộp X vào Y.

Cách kiểm nhanh: `rtk grep '#include "X.h"'` toàn repo, đếm số file khác nhau. Bằng 1 ⇒ ứng viên gộp.

Áp vào repo (đã rà toàn bộ `#include` trong `src/` + `lib/`):

| Module | Ai dùng | Kết luận |
|---|---|---|
| `Text_Filter.{c,h}` | **chỉ** `lib/Src/uart.c` | ✅ gộp vào `uart` |
| `Framing_Base.h` | `Frame_Builder.h`, `Text_Filter.h`, `uart.h` — cả 3 đều là cụm UART | ✅ gộp vào `uart.h` |
| `Frame_Builder.{c,h}` | `uart.c` **và** `src/main.c:13` | ⚠️ gộp được **sau khi** dời `Frame_Builder_Init()` vào trong `Developer_UART_Handler_Init()` — lúc đó app hết lý do biết tới nó |
| `font5x7.{c,h}` | `SSD1306.c` **và** `src/ui.c:18` (dùng `FONT5X7_ADVANCE`, `FONT5X7_DEGREE_CHAR`) | ⚠️ gộp được **sau khi** `SSD1306.h` phơi hai hằng đó ra; giữ `font5x7.c` riêng vì là bảng dữ liệu thuần |
| `Ring_Buffer.{c,h}` | `uart` **và** `main.c` | ❌ **giữ riêng** — cấu trúc dữ liệu tổng quát, có giá trị dùng lại độc lập |
| `Command_Selector.{c,h}` | `uart.c` **và** `main.c` | ❌ giữ riêng (nội dung lệnh tách ra theo 7A) |
| `Global_Enum.h`, `pin_config.h` | nhiều nơi | ❌ giữ riêng |

**Kết quả nếu làm đủ**: cụm framing (`Framing_Base.h` + `Text_Filter.{c,h}` + `Frame_Builder.{c,h}` — 5 file) thu về trong `uart.{c,h}`; `lib/Inc/` giảm ~4 header; và toàn bộ đường đi của một byte — từ ISR → ring buffer → lọc ký tự → dựng khung → tra bảng lệnh — nằm gọn trong **một** cặp file đọc từ trên xuống.

**Không gộp khi:**

- Module là **cấu trúc dữ liệu / thuật toán tổng quát**, không dính ngữ cảnh dùng (`Ring_Buffer`).
- Module là **bảng dữ liệu lớn** (`font5x7.c`) — gộp vào làm loãng file logic.
- Gộp xong file vượt mức đọc thoải mái (mốc mềm: ~400-500 dòng cho một `.c`).
- Module đang có kế hoạch dùng ở chỗ thứ hai trong tương lai gần — ghi kế hoạch đó vào comment, đừng để mặc định.

**Gộp là di chuyển nguyên vẹn**, không viết lại: giữ nguyên tên hàm, chữ ký, comment của tác giả (xem mục 0.1). Hàm nào sau khi gộp chỉ còn dùng nội bộ thì thêm `static` (mục 6).

---

## 8. Trùng lặp & tái sử dụng 🟡

> **Một mẩu tri thức chỉ được mã hoá ở một chỗ.**
> Rule-of-three: lặp lần 2 thì ghi chú, lặp lần 3 thì bắt buộc tách hàm.

Repo hiện có 4 điểm tồn đọng, xếp theo mức nguy hiểm.

### 8.1 — Cùng một chuỗi trạng thái được format ở 3 nơi, 3 định dạng khác nhau 🔴

| Nơi | Định dạng |
|---|---|
| `src/main.c:265` `SendStatusToPhone()` | `TEMP=%uC HUM=%u%% RELAY=%s` |
| `src/main.c:290` `Command_STATUS()` | `TEMP=%uC HUM=%u%% RELAY=%s BT=%s` |
| `src/ui.c:302-312` `UI_DrawSensorPage()` | `TEMP:%u°C` / `HUMI:%u%%` / `BT:%s` |

Đây là loại trùng lặp tệ nhất: **đã trôi khỏi nhau** — chỗ có `BT=`, chỗ không; chỗ dùng `=`, chỗ dùng `:`. Thêm một trường mới hoặc đổi đơn vị phải sửa 3 chỗ, và chắc chắn có chỗ bị quên.

**Cách sửa** (giữ nguyên cả 3 hàm, chỉ rút phần chung ra): một hàm `App_FormatStatus(char *out, size_t n, uint8_t fields)` đặt ở `src/app_command.c`, ba nơi kia gọi vào.

### 8.2 — `Frame_Building()` lặp cùng một khối trong 3 nhánh 🔴

`lib/Src/Frame_Builder.c:26-75` chia 3 nhánh theo `framing_result_index`, mỗi nhánh viết lại cùng thao tác:

- Biểu thức con trỏ `base + index * data_size_byte` viết tay **3 lần** (dòng 28, 33, 49).
- Khối "chốt khung" (`memset('\0')` + `ready_to_read = DEV_TRUE`) lặp **2 lần** (33-35, 49-51).
- Xử lý backspace lặp **3 lần** (38-40, 55, 65) — và **không giống nhau**: chỉ nhánh đầu có bảo vệ `index > 0`, hai nhánh kia không. Ở đây chưa gây lỗi vì các nhánh đó chỉ chạy khi `index >= max-1`, nhưng đúng là kiểu trùng lặp sinh bug khi ai đó chỉnh điều kiện nhánh.

**Cách sửa** (giữ nguyên `Frame_Building()` và API của nó): tách ba `static` helper trong cùng file — `Frame_Slot_Ptr()`, `Frame_Commit()`, `Frame_Backspace()` — rồi 3 nhánh chỉ còn khác nhau ở điều kiện.

### 8.3 — Số học con trỏ phần tử lặp giữa hai module 🟡

Cùng biểu thức `(uint8_t*)base + index * elem_size` + `memcpy` xuất hiện ở `Ring_Buffer.c:38`, `Ring_Buffer.c:57` và 3 chỗ trong `Frame_Builder.c`.

Ứng viên cho một `static inline` dùng chung — **nhưng cân nhắc**: hai module đang độc lập nhau, gom lại sẽ tạo phụ thuộc mới đi ngược mục 7B. Đây là **quyết định có đánh đổi**, không phải luật cứng. Nếu 8.2 được làm thì phần lặp bên `Frame_Builder` tự biến mất và vấn đề này gần như hết.

### 8.4 — Các handler lệnh gần trùng nhau ⚪

`Command_ON`/`Command_OFF` (`src/main.c:273-285`) chỉ khác đúng một tham số `1u`/`0u`; `Command_TEMP`/`Command_HUM` chỉ khác chuỗi format.

Ở quy mô 6 lệnh, viết tay vẫn **rõ ràng hơn** là gộp. **Ghi nhận nhưng không bắt sửa.** Ngưỡng: khi bảng lệnh vượt ~10 mục thì chuyển sang bảng có tham số.

### Nguyên tắc chống trùng lặp cho lần sau

- Trước khi viết hàm mới, tìm xem `lib/Inc/` đã có chưa — repo sẵn có ring buffer, frame builder, text filter, bảng lệnh.
- Hằng số dùng ở nhiều nơi phải là macro một chỗ; đặc biệt là chân, baud, kích thước buffer (`lib/Inc/pin_config.h` đang giữ vai trò này).
- **Hàm tiện ích đặt sai chỗ là mầm mống trùng lặp**: `delay_us()` khai trong `lib/Inc/DHT11.h:62` là tiện ích chung nhưng nằm trong header của một cảm biến — module khác cần trễ µs sẽ tự viết lại thay vì include header DHT11. Liên kết mục 7.

**Hai cảnh báo ngược** — để checklist này không bị áp máy móc:

- **Trùng lặp được phép**: trong đường ISR nóng vì lý do tốc độ, hoặc hai đoạn "trông giống nhau nhưng thay đổi vì lý do khác nhau" (như `Ring_Buffer_Write_SingleData`/`Read_SingleData` đối xứng nhau). Giữ nguyên, chỉ cần comment nói rõ là cố ý.
- **Đừng trừu tượng hoá sớm**: hai đoạn giống nhau lần đầu chưa chắc là cùng một tri thức. Gộp sai còn khó gỡ hơn là để lặp.

---

## 9. Phần cứng & peripheral 🟡

- **Không magic number cho chân/địa chỉ.** Mọi chân qua `lib/Inc/pin_config.h`. ✅ Đạt toàn repo.
- **Kiểm mọi giá trị trả về của `HAL_*` khi khởi tạo**, không nuốt lỗi im lặng. ✅ Mẫu: `src/board.c:51,62,138,155,178,189` đều rẽ về `Error_Handler()`.
- **Cấu hình GPIO ghi rõ Mode/Pull/Speed.** Open-drain vs push-pull phải có lý do — DHT11 dùng open-drain vì bus một dây hai chiều (`lib/Src/DHT11.c:58+`).
- **IRQn lấy từ cấu hình, không hardcode.** ✅ `lib/Src/DHT11.c:53` có ghi rõ bẫy: PB15 dùng `EXTI15_10_IRQn` chứ không phải `EXTI2`.
- **Nêu rõ giả định về clock.** ✅ Mẫu rất tốt: `src/board.c:27-37` ghi cả cây clock lẫn lý do `FLASH_LATENCY_2`; `src/board.c:171-179` tính prescaler TIM2 từ clock thực tế thay vì ghi cứng, và `Error_Handler()` nếu không chia ra được 1 MHz chính xác. **PR đổi clock tree phải rà lại mọi tính toán baud/timer.**
- **Ghi mức an toàn trước khi init GPIO** để tránh nhấp nháy lúc boot. ✅ `src/board.c:83-85`.

---

## 10. Xử lý lỗi & robustness 🟡

- **Mọi đường lỗi phải đưa hệ thống về trạng thái xác định.** ✅ Mẫu: `UART_Frame_Reset()` (`lib/Src/uart.c:48-51`) được gọi ở cả đường thành công lẫn đường lỗi.
- **State machine: mọi state có đường thoát, state trung gian có timeout.** DHT11 FSM có `DHT11_STATE_ERROR` và timeout qua TIM2 — giữ nguyên tính chất này khi thêm state.
- **Input từ ngoài luôn coi là không tin cậy.** Bluetooth là kênh mở: giới hạn độ dài khung (`max_frame_size`), lọc ký tự (`Text_Filting`), và xử lý khung rỗng.

  ✅ Mẫu xử lý đúng ca biên — `lib/Src/uart.c:57-60`:

  ```c
  /* Khung rong khong phai la lenh — bo qua chu khong bao "Invalid Command".
   * Truong hop hay gap nhat: app gui CRLF, ky tu '\r' chot lenh that roi
   * '\n' con lai chot them mot khung rong ngay sau do. */
  ```

- **Đường truyền im lặng cũng là một sự kiện.** ✅ `lib/Src/uart.c:110-113` tự chốt khung sau `UART_FRAME_IDLE_TIMEOUT_MS` — và comment giải thích vì sao phải đợi buffer rỗng mới kiểm tra mốc thời gian.

---

## 11. Style & comment ⚪ (nhưng bắt buộc thống nhất)

### Naming — chốt lại theo cái repo đang dùng

| Loại | Quy ước | Ví dụ |
|---|---|---|
| Hàm public | `Module_Action()` | `DHT11_StartRequest`, `Ring_Buffer_Write_SingleData`, `UART_Task` |
| Hàm static | `Module_Action()` hoặc `Module_Verb_Noun()` | `UART_Frame_Dispatch`, `UI_DrawHeader` |
| Biến local / static file-scope | `snake_case` | `last_poll_ms`, `ui_current_page` |
| Macro / hằng | `UPPER_SNAKE` | `DHT11_POLL_TIMEOUT_MS` |
| Type | `*_HandleTypeDef` hoặc `*_t` | `Ring_Buffer_HandleTypeDef`, `DHT11_State_t` |
| Hằng enum | `UPPER_SNAKE` + tiền tố type | `DHT11_STATE_IDLE` (xem mục 4) |

### Comment

- **Tiếng Việt có dấu UTF-8, thống nhất toàn repo.** ⚠️ Hiện `src/*.c` viết không dấu, `lib/Inc/pin_config.h` và `lib/Src/DHT11.c` viết có dấu — cần một lần chuẩn hoá, không kèm thay đổi logic.
- **Comment giải thích *tại sao*, không mô tả lại code.** Đây là điểm mạnh nhất của repo — giữ chuẩn mực này.

  ✅ Mẫu — `lib/Src/uart.c:103-109`:

  ```c
  /* Toi day bo dem da rong. Con khung do dang ma duong truyen im lang du lau
   * thi coi nhu app khong gui ky tu ket thuc — tu chot khung.
   *
   * Phai doi bo dem rong moi kiem tra: last_received_tick do thoi diem byte
   * ve toi ISR, khong phai luc no duoc xu ly. */
  ```

  Comment này ghi lại một quyết định thiết kế và cái bẫy đằng sau nó — thứ không đọc ra được từ code.

- **Doxygen `@brief` / `@param` / `@retval`** cho mọi hàm public trong header.
- `/* */` cho block, `//` cho cuối dòng. Indent 4 space, không tab.

---

## 12. Build & commit ⚪

- **Build sạch cảnh báo với `-Wall -Wextra`.** Kiểm `CMakeLists.txt` xem đã bật chưa; chưa thì bật (xem mục 13).
- **Không sửa file trong `Drivers/`** (HAL của ST). Cần thay đổi thì làm ở `Drivers/Inc/stm32f1xx_hal_conf.h` và ghi chú lý do.
- **Không commit** `build/` và `local/`.
- Commit message: dòng đầu ≤ 72 ký tự, mô tả **cái gì + tại sao**.

---

## 13. Bước tiếp theo (đề xuất, chưa làm)

Xếp theo thứ tự nên làm — mỗi việc đều là **sửa tại chỗ**, không viết lại module nào:

1. Thêm `static` cho 6 biến ở `lib/Src/DHT11.c:9-14`; thêm `volatile` cho `dht_data`, `g_dht_pin` (mục 1, 6).
2. Tách `src/app_command.c` theo mục 7A — đúng chỗ mà `Command_Selector.h:18` đã chỉ định.
3. Gộp cụm framing (`Framing_Base.h` + `Text_Filter` + `Frame_Builder`) vào `uart.{c,h}` theo mục 7B.
4. Gom `App_FormatStatus()` (8.1) và dọn 3 nhánh của `Frame_Building()` (8.2).
5. Chuẩn hoá tên hằng enum; chốt một cách biểu diễn boolean (mục 4).
6. Chuẩn hoá comment `src/` sang tiếng Việt có dấu (mục 11).
7. Hạ tầng: thêm `.clang-format`, bật `-Wall -Wextra` trong `CMakeLists.txt`, thêm cppcheck.
8. Dựng Unity test cho các module thuần logic — `Ring_Buffer.c`, `Frame_Builder.c`, `Text_Filter.c`, `Command_Selector.c` đều **chạy được trên PC, không cần MCU**. Đây là khoảng trống rõ nhất của repo: `test/` hiện có 4 file placeholder mỗi file đúng 1 dòng comment.
