/**
 * @file SSD1306.c
 * @brief Cài đặt driver SSD1306: lớp giao tiếp I2C, lớp framebuffer (vẽ
 * trong RAM) và lớp văn bản (vẽ ký tự/chuỗi dùng font5x7).
 */
#include "SSD1306.h"
#include "font5x7.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_i2c.h"
#include <string.h>

#define SSD1306_I2C_ADDRESS \
	0x3C << 1 // Địa chỉ I2C của SSD1306 (thường là 0x3C hoặc 0x3D)


/** @brief Khởi tạo chip SSD1306 (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_Init (I2C_HandleTypeDef* hi2c) {
	// Mảng lệnh khởi tạo SSD1306 (128x64)
	uint8_t ssd1306_init_cmds[] = {
		0x00,		// Control byte: Co = 0, D/C# = 0
		0xAE,		// Display OFF (sleep mode)
		0xD5, 0x80, // Set display clock divide ratio/oscillator frequency
		0xA8, 0x3F, // Set multiplex ratio (0x3F = 63 cho màn 128x64, dùng 0x1F cho 128x32)
		0xD3, 0x00, // Set display offset = 0
		0x40,		// Set display start line = 0
		0x8D, 0x14, // Enable charge pump (bắt buộc để hiện hình)
		0x20, 0x00, // Set memory addressing mode = Horizontal
		0xA1,		// Set segment re-map (lật ngang, tùy layout)
		0xC8,		// Set COM output scan direction (lật dọc, tùy layout)
		0xDA, 0x12, // Set COM pins hardware config (0x02 cho 128x32)
		0x81, 0xCF, // Set contrast control
		0xD9, 0xF1, // Set pre-charge period
		0xDB, 0x40, // Set VCOMH deselect level
		0xA4,		// Entire display ON (resume RAM content display)
		0xA6,		// Set normal display (không đảo màu)
		0xAF		// Display ON <-- lệnh quan trọng nhất để "bật màn hình"
	};
	// Gửi các lệnh khởi tạo đến SSD1306 qua I2C
	HAL_I2C_Master_Transmit (hi2c, SSD1306_I2C_ADDRESS, ssd1306_init_cmds,
	sizeof (ssd1306_init_cmds), HAL_MAX_DELAY);
	return HAL_OK; // Khởi tạo thành công
}

/** @brief Gửi 1 byte lệnh tới chip (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_SendCommand (I2C_HandleTypeDef* hi2c, uint8_t command) {
	uint8_t cmd[2] = { 0x00, command }; // 0x00 là byte điều khiển cho lệnh
	return HAL_I2C_Master_Transmit (hi2c, SSD1306_I2C_ADDRESS, cmd, 2, HAL_MAX_DELAY);
}

/** @brief Gửi khối dữ liệu pixel tới GDDRAM (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_SendData (I2C_HandleTypeDef* hi2c, uint8_t* data, size_t size) {
	// Set command byte to 0x40 for data transfer
	if (HAL_I2C_Mem_Write (hi2c, SSD1306_DEFAULT_ADDRESS, 0x40,
		I2C_MEMADD_SIZE_8BIT, data, size, HAL_MAX_DELAY) != HAL_OK) {
		return HAL_ERROR;
	}
	return HAL_OK;
}

/* =======================================================================
 * SSD1306 OLED driver implementation
 * ======================================================================= */

/* ---------------- Điều khiển display (không đụng buffer) ---------------- */
/** @brief Bật display (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_DisplayOn (I2C_HandleTypeDef* hi2c) {
	// TODO: gửi lệnh SSD1306_DISPLAYON qua SSD1306_SendCommand()
	if (SSD1306_SendCommand (hi2c, SSD1306_DISPLAYON))
		return HAL_OK;
	return HAL_ERROR;
}

/** @brief Tắt display (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_DisplayOff (I2C_HandleTypeDef* hi2c) {
	if (SSD1306_SendCommand (hi2c, SSD1306_DISPLAYOFF))
		return HAL_OK;
	return HAL_ERROR;
}

/** @brief Đặt độ tương phản (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_SetContrast (I2C_HandleTypeDef* hi2c, uint8_t value) {
	// TODO: đây là lệnh 2 byte: gửi SSD1306_SETCONTRAST rồi gửi tiếp `value`
	// (2 lần gọi SSD1306_SendCommand, hoặc tự build mảng 2 command byte).
	if (SSD1306_SendCommand (hi2c, SSD1306_SETCONTRAST))
		return HAL_OK;
	if (SSD1306_SendCommand (hi2c, value))
		return HAL_OK;
	return HAL_ERROR;
}

/** @brief Đảo màu toàn màn hình (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_InvertDisplay (I2C_HandleTypeDef* hi2c, uint8_t invert) {
	if (SSD1306_SendCommand (hi2c, invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY))
		return HAL_OK;
	return HAL_ERROR;
}

/** @brief Đặt con trỏ ghi GDDRAM (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_SetCursor (I2C_HandleTypeDef* hi2c, uint8_t x, uint8_t page) {
	// TODO (page addressing mode): gửi 3 lệnh liên tiếp:
	//   SSD1306_SETPAGE | page
	//   SSD1306_SETLOWCOLUMN  | (x & 0x0F)
	//   SSD1306_SETHIGHCOLUMN | (x >> 4)

	if (SSD1306_SendCommand (hi2c, SSD1306_SETPAGE | page))
		return HAL_OK;
	if (SSD1306_SendCommand (hi2c, SSD1306_SETLOWCOLUMN | (x & 0x0F)))
		return HAL_OK;
	if (SSD1306_SendCommand (hi2c, SSD1306_SETHIGHCOLUMN | (x >> 4)))
		return HAL_OK;
	return HAL_ERROR;
}

/* ---------------- Framebuffer (chỉ sửa RAM, không I2C) ---------------- */

/** @brief Tô toàn bộ buffer bằng 1 màu (xem SSD1306.h). */
void SSD1306_Fill (ssd1306_t* ssd, SSD1306_COLOR color) {
	// TODO: memset ssd->buffer bằng 0x00 (đen) hoặc 0xFF (trắng) tuỳ color
	memset (ssd->buffer, (color == SSD1306_COLOR_WHITE) ? 0xFF : 0x00,
	sizeof (ssd->buffer));
}

/** @brief Xoá buffer (= Fill màu đen) (xem SSD1306.h). */
void SSD1306_Clear (ssd1306_t* ssd) {
	// TODO: gọi lại SSD1306_Fill(ssd, SSD1306_COLOR_BLACK)
	SSD1306_Fill (ssd, SSD1306_COLOR_BLACK);
}

/** @brief Bật/tắt 1 điểm ảnh tại (x, y) (xem SSD1306.h). */
void SSD1306_DrawPixel (ssd1306_t* ssd, uint16_t x, uint16_t y, SSD1306_COLOR color) {
	// Kiểm tra vị trí pixel (x, y) hợp lệ. Nếu hợp lệ thì ghi vào buffer.
	if (x >= ssd->width || y >= ssd->height)
		return;
	uint16_t index = x + (y / 8) * ssd->width;
	uint8_t bit	   = y % 8;
	if (color == SSD1306_COLOR_WHITE) {
		ssd->buffer[index] |= 1 << bit;
	} else {
		ssd->buffer[index] &= ~(1 << bit);
	}
}

/** @brief Vẽ bitmap 1-bit vào buffer (xem SSD1306.h). */
void SSD1306_DrawBitmap (ssd1306_t* ssd,
uint16_t x,
uint16_t y,
const uint8_t* bitmap,
uint16_t w,
uint16_t h,
SSD1306_COLOR color) {
	// Ghi từng bit của bitmap vào buffer bằng SSD1306_DrawPixel.
	uint16_t pages_per_col = (h + 7) / 8;
	for (uint16_t col = 0; col < w; col++) {
		for (uint16_t page = 0; page < pages_per_col; page++) {
			uint8_t byte = bitmap[col + page * w];
			for (uint8_t bit = 0; bit < 8; bit++) {
				uint16_t py = page * 8 + bit;
				if (py >= h)
					break;
				if (byte & (1 << bit))
					SSD1306_DrawPixel (ssd, x + col, y + py, color);
			}
		}
	}
}

/** @brief Đẩy buffer sang GDDRAM của chip qua I2C (xem SSD1306.h). */
HAL_StatusTypeDef SSD1306_UpdateScreen (I2C_HandleTypeDef* hi2c, ssd1306_t* ssd) {
	// Khi gửi SSD1306_COLUMNADDR và SSD1306_PAGEADDR thì 2 tham số tiếp theo
	// bắt đầu và kết thúc của horizontal scrolling

	SSD1306_SendCommand (hi2c, SSD1306_COLUMNADDR);
	SSD1306_SendCommand (hi2c, 0);				// Gửi cột đầu tiên
	SSD1306_SendCommand (hi2c, ssd->width - 1); // Gửi cột cuối cùng

	SSD1306_SendCommand (hi2c, SSD1306_PAGEADDR);
	SSD1306_SendCommand (hi2c, 0);					   // Gửi trang đầu tiên
	SSD1306_SendCommand (hi2c, (ssd->height / 8) - 1); // Gửi trang cuối cùng


	if (SSD1306_SendData (hi2c, ssd->buffer, SSD1306_BUFFER_SIZE) != HAL_ERROR)
		return HAL_OK;
	return HAL_ERROR;
}

/* ---------------- Văn bản (cần bảng font ngoài, VD font5x7.h) ---------------- */

/** @brief Vẽ 1 ký tự vào buffer, trả về bề rộng vừa vẽ (xem SSD1306.h). */
uint8_t SSD1306_WriteChar (ssd1306_t* ssd, uint16_t x, uint16_t y, char ch, SSD1306_COLOR color) {
	const uint8_t* glyph = font5x7_get_glyph (ch);
	if (glyph == NULL)
		return 0;
	SSD1306_DrawBitmap (ssd, x, y, glyph, FONT5X7_WIDTH, FONT5X7_HEIGHT, color);
	return FONT5X7_WIDTH + 1; // +1 cột khoảng cách giữa các ký tự
}

/** @brief Vẽ chuỗi ký tự, tự xuống dòng khi chạm mép phải (xem SSD1306.h). */
void SSD1306_WriteString (ssd1306_t* ssd, uint16_t x, uint16_t y, const char* str, SSD1306_COLOR color) {
	uint16_t cursor_x = x;
	uint16_t cursor_y = y;
	for (; *str != '\0'; str++) {
		if (cursor_x + FONT5X7_WIDTH > ssd->width) {
			cursor_x = x;
			cursor_y += FONT5X7_HEIGHT + 1;
		}
		cursor_x += SSD1306_WriteChar (ssd, cursor_x, cursor_y, *str, color);
	}
}
