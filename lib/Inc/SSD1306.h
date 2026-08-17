/**
 * @file SSD1306.h
 * @brief Driver cho màn hình OLED SSD1306 (I2C) trên STM32 HAL: gồm lớp giao
 * tiếp phần cứng (gửi lệnh/dữ liệu qua I2C), lớp framebuffer (vẽ trong RAM)
 * và lớp văn bản (vẽ ký tự/chuỗi dùng font5x7).
 */
#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
	MIT License

	Copyright (c) 2017-2018, Alexey Dynda

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

enum ESsd1306Commands {
	SSD1306_SETLOWCOLUMN		= 0x00,
	SSD1306_SETHIGHCOLUMN		= 0x10,
	SSD1306_MEMORYMODE			= 0x20,
	SSD1306_COLUMNADDR			= 0x21,
	SSD1306_PAGEADDR			= 0x22,
	SSD1306_SETSTARTLINE		= 0x40,
	SSD1306_DEFAULT_ADDRESS		= 0x78,
	SSD1306_SETCONTRAST			= 0x81,
	SSD1306_CHARGEPUMP			= 0x8D,
	SSD1306_SEGREMAP			= 0xA0,
	SSD1306_DISPLAYALLON_RESUME = 0xA4,
	SSD1306_DISPLAYALLON		= 0xA5,
	SSD1306_NORMALDISPLAY		= 0xA6,
	SSD1306_INVERTDISPLAY		= 0xA7,
	SSD1306_SETMULTIPLEX		= 0xA8,
	SSD1306_DISPLAYOFF			= 0xAE,
	SSD1306_DISPLAYON			= 0xAF,
	SSD1306_SETPAGE				= 0xB0,
	SSD1306_COMSCANINC			= 0xC0,
	SSD1306_COMSCANDEC			= 0xC8,
	SSD1306_SETDISPLAYOFFSET	= 0xD3,
	SSD1306_SETDISPLAYCLOCKDIV	= 0xD5,
	SSD1306_SETPRECHARGE		= 0xD9,
	SSD1306_SETCOMPINS			= 0xDA,
	SSD1306_SETVCOMDETECT		= 0xDB,

	SSD1306_SWITCHCAPVCC = 0x02,
	SSD1306_NOP			 = 0xE3,
};

enum ESsd1306MemoryMode {
	HORIZONTAL_ADDRESSING_MODE = 0x00,
	VERTICAL_ADDRESSING_MODE   = 0x01,
	PAGE_ADDRESSING_MODE	   = 0x02,
};

/* Kích thước màn hình - đổi thành 32 nếu dùng module 128x32 */
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

/* Mỗi byte trong buffer chứa 8 pixel dọc (1 page) -> tổng số byte cần thiết */
#define SSD1306_BUFFER_SIZE ((SSD1306_WIDTH * SSD1306_HEIGHT) / 8)

typedef enum {
	SSD1306_COLOR_BLACK = 0x00, /* pixel tắt */
	SSD1306_COLOR_WHITE = 0x01, /* pixel sáng */
} SSD1306_COLOR;

/* Đối tượng màn hình: giữ framebuffer cục bộ (RAM của MCU), được vẽ lên bằng
 * các hàm SSD1306_Draw... / SSD1306_Write..., sau đó đẩy toàn bộ sang chip
 * qua SSD1306_UpdateScreen(). */
typedef struct {
	uint8_t buffer[SSD1306_BUFFER_SIZE];
	uint16_t width;
	uint16_t height;
} ssd1306_t;

/* ======================= Lớp giao tiếp phần cứng (I2C) =======================
 * Làm việc trực tiếp với chip qua HAL I2C, không đụng tới framebuffer.
 * Đây là lớp thấp nhất, các lớp phía trên (framebuffer/vẽ) dựng trên đây. */

/**
 * @brief Khởi tạo chip SSD1306: gửi chuỗi lệnh cấu hình, bật nguồn charge
 * pump, bật display. Gọi 1 lần sau khi I2C peripheral đã init xong.
 */
HAL_StatusTypeDef SSD1306_Init (I2C_HandleTypeDef* hi2c);

/** @brief Gửi 1 byte lệnh tới chip (control byte 0x00). */
HAL_StatusTypeDef SSD1306_SendCommand (I2C_HandleTypeDef* hi2c, uint8_t command);

/** @brief Gửi khối dữ liệu pixel tới GDDRAM của chip (control byte 0x40). */
HAL_StatusTypeDef SSD1306_SendData (I2C_HandleTypeDef* hi2c, uint8_t* data, size_t size);

/** @brief Bật display (không xoá nội dung GDDRAM, chỉ bật nguồn hiển thị). */
HAL_StatusTypeDef SSD1306_DisplayOn (I2C_HandleTypeDef* hi2c);
/** @brief Tắt display (không xoá nội dung GDDRAM, chỉ tắt nguồn hiển thị). */
HAL_StatusTypeDef SSD1306_DisplayOff (I2C_HandleTypeDef* hi2c);

/** @brief Đặt độ tương phản: value 0x00 - 0xFF (lệnh SSD1306_SETCONTRAST). */
HAL_StatusTypeDef SSD1306_SetContrast (I2C_HandleTypeDef* hi2c, uint8_t value);

/**
 * @brief Đảo màu toàn màn hình: invert = 1 bật, 0 tắt (SSD1306_INVERTDISPLAY
 * / SSD1306_NORMALDISPLAY).
 */
HAL_StatusTypeDef SSD1306_InvertDisplay (I2C_HandleTypeDef* hi2c, uint8_t invert);

/**
 * @brief Đặt con trỏ ghi GDDRAM về (column, page). page = y / 8 (0..7 với
 * màn 64px). Cần dùng nếu module đang ở PAGE_ADDRESSING_MODE; nếu dùng
 * HORIZONTAL_ADDRESSING_MODE (mặc định trong ssd1306_init_cmds hiện tại) thì
 * có thể dùng SSD1306_COLUMNADDR/SSD1306_PAGEADDR để set vùng rồi stream cả
 * buffer trong 1 lần SendData.
 */
HAL_StatusTypeDef SSD1306_SetCursor (I2C_HandleTypeDef* hi2c, uint8_t x, uint8_t page);

/* ======================= Lớp framebuffer (vẽ trong RAM)
 * ======================= Các hàm này chỉ sửa ssd->buffer, KHÔNG giao tiếp I2C.
 * Phải gọi SSD1306_UpdateScreen() sau đó để đẩy hình lên màn hình thật. */

/** @brief Tô toàn bộ buffer bằng 1 màu. */
void SSD1306_Fill (ssd1306_t* ssd, SSD1306_COLOR color);

/** @brief Tiện ích = SSD1306_Fill(ssd, SSD1306_COLOR_BLACK). */
void SSD1306_Clear (ssd1306_t* ssd);

/** @brief Bật/tắt 1 điểm ảnh tại (x, y). Nhớ bound-check x < width, y < height. */
void SSD1306_DrawPixel (ssd1306_t* ssd, uint16_t x, uint16_t y, SSD1306_COLOR color);

/**
 * @brief Vẽ bitmap 1-bit (mảng byte, mỗi byte 8 pixel dọc giống layout
 * GDDRAM, hoặc tự định nghĩa layout riêng miễn là DrawPixel dùng đúng layout
 * đó).
 */
void SSD1306_DrawBitmap (ssd1306_t* ssd,
uint16_t x,
uint16_t y,
const uint8_t* bitmap,
uint16_t w,
uint16_t h,
SSD1306_COLOR color);

/**
 * @brief Đẩy toàn bộ ssd->buffer sang GDDRAM của chip qua I2C. Gọi sau mỗi
 * lần vẽ xong 1 "frame" (không nên gọi sau từng DrawPixel vì rất chậm).
 */
HAL_StatusTypeDef SSD1306_UpdateScreen (I2C_HandleTypeDef* hi2c, ssd1306_t* ssd);

/* ======================= Lớp văn bản =======================
 * Cần 1 bảng font 5x7 (hoặc kích thước khác) dạng const uint8_t[] - chưa có
 * sẵn trong project này, bạn cần thêm (VD: font5x7.h) rồi include vào
 * SSD1306.c. */

/**
 * @brief Vẽ 1 ký tự tại toạ độ góc trên-trái (x, y).
 * @return Bề rộng ký tự vừa vẽ (để gọi tiếp cho ký tự kế, hữu ích khi tự
 * viết SSD1306_WriteString), hoặc 0 nếu không tìm thấy glyph cho ch.
 */
uint8_t SSD1306_WriteChar (ssd1306_t* ssd, uint16_t x, uint16_t y, char ch, SSD1306_COLOR color);

/** @brief Vẽ chuỗi ký tự, tự xuống dòng khi chạm mép phải (width). */
void SSD1306_WriteString (ssd1306_t* ssd, uint16_t x, uint16_t y, const char* str, SSD1306_COLOR color);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
