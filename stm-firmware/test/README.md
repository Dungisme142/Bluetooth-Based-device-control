# test/

Project hiện **chưa có unit test tự động**. Việc kiểm chứng firmware được làm **thủ công
trên board thật** theo bộ 79 ca test ở:

→ [`docs/11-ke-hoach-kiem-thu.md`](../../docs/11-ke-hoach-kiem-thu.md)

Thư mục này để trống, chờ khung unit test chạy trên host (Unity + CTest). Các module trong
`lib/` là C thuần, không phụ thuộc HAL, nên biên dịch và test được ngay trên PC:

| Module | Ca test cần có |
|---|---|
| `ring_buffer.c` | Ghi/đọc bình thường, ghi khi đầy, đọc khi rỗng, quay vòng |
| `command_selector.c` | `ON`, `ON 3`, `ONLINE`, `on`, chuỗi rỗng, lệnh ngoài bảng |
| `uart.c` — `Text_Filting` / `Frame_Building` | CR, LF, CRLF, backspace, khung tràn, khung rỗng |

Lưu ý: thư mục này **không** nằm trong `file(GLOB_RECURSE ...)` của `CMakeLists.txt`, nên
file `.c` đặt ở đây sẽ không được biên dịch vào firmware. Khung test khi bổ sung phải có
`CMakeLists.txt` riêng cho build host.

Xem thêm [`docs/12-han-che-va-huong-phat-trien.md`](../../docs/12-han-che-va-huong-phat-trien.md) §12.2.
