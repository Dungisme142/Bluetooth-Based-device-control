# 10 — Build và nạp firmware

Toàn bộ project firmware nằm trong thư mục [`stm-firmware/`](../stm-firmware/). Mọi lệnh
dưới đây chạy **từ thư mục đó**.

Công cụ: **CMake + Ninja + arm-none-eabi-gcc**, nạp qua **ST-Link / SWD**.
Không có PlatformIO, không có Makefile, và **không có file `.ioc`** — project viết tay,
đừng sinh lại bằng STM32CubeMX.

## 10.1 Yêu cầu môi trường

| Công cụ | Kiểm tra | Ghi chú |
|---|---|---|
| ARM GCC toolchain | `arm-none-eabi-gcc --version` | Phải nằm trên `PATH` |
| CMake ≥ 3.22 | `cmake --version` | |
| Ninja | `ninja --version` | |
| **Windows**: STM32CubeProgrammer | `STM32_Programmer_CLI --version` | Cung cấp công cụ nạp + driver ST-Link |
| **Linux/macOS**: stlink-tools | `st-info --version` | `sudo apt install stlink-tools` |

Nếu `cmake -G Ninja` báo không tìm thấy compiler, kiểm tra
[`cmake/gcc-arm-none-eabi.cmake`](../stm-firmware/cmake/gcc-arm-none-eabi.cmake) xem đường
dẫn toolchain có khớp máy không.

## 10.2 Cấu hình build

| Thuộc tính | Giá trị |
|---|---|
| Target | STM32F103C8T6 — Cortex-M3, 64 KB flash, 20 KB RAM |
| Chuẩn C | C11 |
| Build type mặc định | `Debug` (`-O0 -g3`); `Release` là `-Os -g0` |
| Cảnh báo | `-Wall -Wextra -Wpedantic` |
| Tối ưu kích thước | `-fdata-sections -ffunction-sections` + `-Wl,--gc-sections` |
| Thư viện C | `--specs=nano.specs` (newlib-nano) |
| Linker script | `STM32F103xx_FLASH.ld` |
| Định nghĩa | `STM32F103xB`, `USE_HAL_DRIVER` |
| Sản phẩm | `build/firmware.elf`, `.hex`, `.bin`, `.map` |

`CMakeLists.txt` gom source bằng `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` trên `src/`,
`lib/Src/`, `Drivers/`. **`CONFIGURE_DEPENDS` là bắt buộc**: không có nó, danh sách file bị
đóng băng từ lần configure đầu và build sẽ gãy với "missing and no known rule to make it"
sau khi thêm/xoá file.

Các file `*_template.c` và `stm32f1xx_ll_*.c` bị loại khỏi build.

> ℹ️ Trước đây `cmake/gcc-arm-none-eabi.cmake` ghi tên linker script sai chữ hoa/thường
> (`STM32F103XX_FLASH.ld` thay vì `STM32F103xx_FLASH.ld`), khiến bước link fail trên
> Linux/macOS trong khi Windows vẫn chạy được. **Đã sửa.**

## 10.3 Build

```bash
# Build sạch từ đầu
rm -rf build && cmake -G Ninja -B build && ninja -C build

# Build nhanh (incremental) — dùng khi đang sửa code liên tục
ninja -C build

# Bản Release, nhỏ hơn nhiều
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release && ninja -C build
```

Kiểm tra kích thước firmware so với ngân sách 64 KB flash / 20 KB RAM:

```bash
arm-none-eabi-size build/firmware.elf
```

Hiện tại (bản Debug):

```
   text    data     bss     dec     hex  filename
  27776     196    4044   32016    7d10  build/firmware.elf
```

→ Flash = `text + data` ≈ **27,3 KB / 64 KB (43 %)**;
RAM = `data + bss` ≈ **4,1 KB / 20 KB (21 %)**.

Linker cũng in sẵn bảng này nhờ `-Wl,--print-memory-usage`.

## 10.4 Nối dây SWD

| ST-Link V2 | Blue Pill |
|---|---|
| SWDIO | **PA13** |
| SWCLK | **PA14** |
| GND | GND |
| 3.3V | 3V3 |

> ⚠️ Nối **3.3 V**, không phải 5 V. Nếu board đã có nguồn riêng thì **chỉ nối
> SWDIO / SWCLK / GND**, đừng nối 3V3 — tránh chập hai nguồn.

Kiểm tra ST-Link có thấy chip không:

```bash
st-info --probe                        # Linux/macOS — phải thấy chip ID 0x410
STM32_Programmer_CLI -c port=SWD -q    # Windows — chỉ connect rồi thoát
```

## 10.5 Luồng A — ST-Link cắm thẳng máy dev

### Windows

```bat
build_and_flash.bat
```

Script làm 4 bước, dừng ngay khi bước nào lỗi:

1. **Xoá sạch `build/`**
2. `cmake -G "Ninja" -B build`
3. `ninja -C build`, dò file `.bin` trong `build/` rồi copy thành `build\app_firmware.bin`
4. `STM32_Programmer_CLI -c port=SWD -w build/app_firmware.bin 0x08000000 -v -rst`
   (`-v` = verify sau khi ghi, `-rst` = reset để chip chạy code ngay)

> ⚠️ Script này **xoá sạch `build/` mỗi lần chạy** → build lại từ đầu kể cả toàn bộ HAL,
> rất chậm. Khi đang sửa code liên tục, làm tay cho nhanh:
> ```bat
> ninja -C build
> copy /y build\firmware.bin build\app_firmware.bin
> STM32_Programmer_CLI -c port=SWD -w build/app_firmware.bin 0x08000000 -v -rst
> ```

Nếu `STM32_Programmer_CLI` không trên `PATH`, mặc định nó ở
`C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\`.

### Linux / macOS / Git Bash

```bash
./build_and_flash.sh
```

Script này **incremental** — chỉ chạy `cmake` khi thiếu `build/build.ninja`, nên nhanh hơn
bản `.bat` nhiều. Sau đó `ninja -C build`, dò `.bin`, rồi `st-flash write ... 0x08000000`
và `st-flash reset`.

## 10.6 Luồng B — nạp qua Raspberry Pi

Dùng khi board đặt xa máy dev, hoặc không cài được driver ST-Link trên máy dev.

```
Máy dev (Windows/Linux)            Raspberry Pi              Blue Pill
┌────────────────────┐  SSH/SCP   ┌──────────────┐   USB    ┌──────────┐
│ ninja -C build     │───────────►│ /tmp/stm32-  │ ST-Link  │ SWD      │
│  → firmware.bin    │            │  firmware/   │─────────►│ PA13/14  │
└────────────────────┘            │  st-flash    │          └──────────┘
                                  └──────────────┘
```

### Setup một lần

Trên Pi:

```bash
sudo apt update && sudo apt install stlink-tools
st-info --probe          # cắm ST-Link vào USB của Pi, nối SWD tới board
```

SSH key (để khỏi gõ mật khẩu mỗi lần nạp):

```bash
ssh-keygen -t ed25519                  # nếu chưa có key
ssh-copy-id pi@raspberrypi.local
ssh pi@raspberrypi.local 'echo OK'     # phải in OK mà không hỏi mật khẩu
```

Trên Windows không có `ssh-copy-id`:

```powershell
type $env:USERPROFILE\.ssh\id_ed25519.pub | ssh pi@raspberrypi.local "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
```

Tạo file cấu hình từ mẫu:

```bash
cp scripts/.env.example scripts/.env
# rồi sửa RPI_HOST cho đúng
```

`scripts/.env` đã nằm trong `.gitignore` — không commit, vì chứa hostname/IP nội bộ.

| Biến | Mặc định | Ý nghĩa |
|---|---|---|
| `RPI_HOST` | *(bắt buộc)* | Hostname / IP / alias trong `~/.ssh/config` |
| `RPI_USER` | `pi` | Tài khoản SSH |
| `RPI_PORT` | `22` | Cổng SSH |
| `RPI_REMOTE_DIR` | `/tmp/stm32-firmware` | Thư mục trung chuyển trên Pi |
| `FLASH_ADDR` | `0x08000000` | Địa chỉ nạp — giữ nguyên cho mọi STM32F1 |
| `BUILD_DIR` | `build` | Thư mục build cục bộ |

### Nạp

```bash
./scripts/deploy_via_rpi.sh      # Linux / Git Bash
scripts\deploy_via_rpi.bat       # Windows
```

## 10.7 Xem log sau khi nạp

> **Không có cổng serial nào nối tới máy dev.** ST-Link chỉ làm SWD, không có VCP. Mở PuTTY
> hay `pio device monitor` sẽ không thấy gì.

Log đi qua **USART2 (PA2/PA3) @ 9600 8N1** → module MKE-M15 → điện thoại:

1. Bật Bluetooth trên điện thoại, ghép cặp với module (PIN thường là `1234` hoặc `0000`).
2. Mở app SPP terminal (ví dụ *Serial Bluetooth Terminal* trên Android).
3. Kết nối tới module.

Thấy dòng `MKE-M15 ready` nghĩa là firmware đã boot thành công. Sau đó cứ 3 giây một bản
tin trạng thái. Xem [06 — Giao thức Bluetooth](06-giao-thuc-bluetooth.md).

Nếu terminal hiện ký tự rác: baud không khớp. Firmware dùng **9600** (mặc định xuất xưởng
của MKE-M15); kiểm tra cả app điện thoại lẫn cấu hình module.

### Ba dấu hiệu sống không cần terminal

| Dấu hiệu | Nghĩa |
|---|---|
| LED PC13 nháy 1 Hz | Vòng lặp chính đang chạy |
| OLED thoát khỏi màn hình `BOOTING` | `UI_Task()` đã chạy được ít nhất một lần |
| Trang LOG có dòng `BOOT OK` | Khởi tạo hoàn tất |

LED **đứng yên** = treo ở `Error_Handler()` hoặc trong một ISR nào đó.

### Gỡ lỗi sâu bằng GDB

```bash
st-util &                                   # gdbserver ở port 4242
arm-none-eabi-gdb build/firmware.elf
(gdb) target extended-remote :4242
(gdb) p output_on
(gdb) p last_temp
```

Chạy được cả trên Pi (luồng B) — SSH vào Pi rồi làm y hệt.

## 10.8 Lỗi thường gặp

### Lỗi build

| Triệu chứng | Nguyên nhân | Xử lý |
|---|---|---|
| `cannot open linker script file STM32F103XX_FLASH.ld` | Sai chữ hoa/thường trong `cmake/gcc-arm-none-eabi.cmake:39` | Chỉ xảy ra trên Linux/macOS — sửa tên trong file cmake |
| `arm-none-eabi-gcc: not found` | Toolchain không trên `PATH` | Thêm vào `PATH` hoặc sửa file toolchain cmake |
| `missing and no known rule to make it` | Đã xoá/đổi tên file `.c` | `rm -rf build` rồi configure lại |
| CMake cache trỏ đường dẫn cũ sau khi di chuyển repo | `build/CMakeCache.txt` lưu đường dẫn tuyệt đối | Xoá cả thư mục `build/` rồi configure lại |
| `region FLASH overflowed` | Firmware vượt 64 KB | Build ở chế độ `Release` (`-Os`) |

### Lỗi nạp

| Triệu chứng | Nguyên nhân | Xử lý |
|---|---|---|
| `No STLINK detected` | Driver, cáp USB, hoặc ST-Link hỏng | Đổi cáp (nhiều cáp USB chỉ có dây nguồn); Windows: kiểm tra Device Manager |
| `Error: Target not responding` | Firmware cũ đang treo SWD (ví dụ `Error_Handler()` đã gọi `__disable_irq()` rồi loop) | Windows: thêm `mode=UR`.<br/>Linux: giữ nút RESET, chạy `st-flash`, thả khi thấy "Attempting to write" |
| Nạp xong chip không chạy | BOOT0 ở mức HIGH → boot vào bootloader | Kéo jumper **BOOT0 về 0**, nhấn RESET |
| `Unknown chip id` | Nối SWD sai chân hoặc thiếu GND | Kiểm tra lại §10.4 |

Xoá sạch flash khi chip bị treo SWD:

```bash
st-flash erase                                     # Linux
STM32_Programmer_CLI -c port=SWD mode=UR -e all    # Windows
```

### Lỗi sau khi nạp

| Triệu chứng | Nguyên nhân có thể |
|---|---|
| LED PC13 không nháy | Treo ở `Error_Handler()` — một `MX_*_Init()` thất bại, hoặc thạch anh HSE không dao động |
| OLED trắng / không hiện gì | Sai địa chỉ I2C, đứt dây SDA/SCL, hoặc OLED chưa được cấp 3.3 V |
| OLED chạy nhưng không có log Bluetooth | Đảo TX/RX giữa MCU và MKE-M15 (PA2 phải nối RX của module) |
| Ký tự rác trên terminal | Baud không khớp — cả hai đầu phải là 9600 |
| `DHT FAIL` liên tục | Cảm biến chưa cắm, sai chân SIG, hoặc vấn đề mức áp ở §3.6 |
