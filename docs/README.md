# Tài liệu dự án — Bluetooth-Based Device Control

Trung tâm điều khiển thiết bị công suất qua Bluetooth, dùng STM32F103C8T6 và module
MKE-M15. Đồ án cuối khoá Embedded C.

Bộ tài liệu được chia nhỏ theo chủ đề: mỗi file trả lời đúng một câu hỏi, đọc rời được,
và có thể sửa độc lập khi firmware thay đổi.

## Mục lục

| # | Tài liệu | Trả lời câu hỏi | Dành cho |
|---|---|---|---|
| 01 | [Tổng quan dự án](01-tong-quan.md) | Hệ thống này làm gì, phạm vi tới đâu? | Mọi người |
| 02 | [Đặc tả yêu cầu](02-dac-ta-yeu-cau.md) | Hệ thống *phải* làm được những gì? (FR/NFR) | Người chấm, người kiểm thử |
| 03 | [Thiết kế phần cứng](03-thiet-ke-phan-cung.md) | Mạch gồm những khối nào, linh kiện gì? | Người lắp mạch |
| 04 | [Sơ đồ chân & ngoại vi](04-so-do-chan.md) | Chân nào nối gì, ngắt nào ưu tiên bao nhiêu? | Người đấu dây, người sửa firmware |
| 05 | [Kiến trúc phần mềm](05-kien-truc-phan-mem.md) | Firmware tổ chức ra sao, dữ liệu chảy thế nào? | Người phát triển |
| 06 | [Giao thức Bluetooth](06-giao-thuc-bluetooth.md) | Gửi lệnh gì, nhận lại gì? | Người viết app điều khiển |
| 07 | [Đặc tả giao diện OLED](07-dac-ta-giao-dien-oled.md) | 5 trang hiển thị gì, nút nào làm gì? | Người dùng cuối, người kiểm thử |
| 08 | [Đặc tả cảm biến DHT11](08-dac-ta-cam-bien-dht11.md) | Đọc cảm biến 1-wire bằng ngắt ra sao? | Người phát triển |
| 09 | [Timing và ngắt](09-timing-va-ngat.md) | Vì sao ưu tiên ngắt đặt như vậy, tốn bao nhiêu thời gian? | Người phát triển, người chấm |
| 10 | [Build và nạp firmware](10-build-va-nap.md) | Làm sao dịch và nạp được? | Mọi người |
| 11 | [Kế hoạch kiểm thử](11-ke-hoach-kiem-thu.md) | Nghiệm thu bằng cách nào? | Người kiểm thử |
| 12 | [Hạn chế và hướng phát triển](12-han-che-va-huong-phat-trien.md) | Còn thiếu gì, làm tiếp thế nào? | Mọi người |

## Tài nguyên kèm theo

- [`images/schematic.pdf`](images/schematic.pdf) — sơ đồ nguyên lý xuất từ KiCad.

## Quy ước

- **Kênh ngõ ra** đánh số **1..5** khi nói với người dùng (lệnh Bluetooth, màn hình OLED),
  nhưng đánh số **0..4** trong code. Mọi tài liệu ở đây dùng cách đánh số của người dùng
  trừ khi trích dẫn code.
- Mã yêu cầu `FR-xx` / `NFR-xx` định nghĩa ở [02](02-dac-ta-yeu-cau.md) và được các
  test case ở [11](11-ke-hoach-kiem-thu.md) tham chiếu ngược lại.
- Nguồn sự thật của phần cứng là schematic; nguồn sự thật của bảng chân trong firmware là
  `stm-firmware/lib/Inc/pin_config.h`. Tài liệu mô tả lại hai thứ đó, không thay thế chúng.
