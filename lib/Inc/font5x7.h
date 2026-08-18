/* Bảng font 5x7 pixel: toàn bộ ASCII in được 0x20..0x7E, cộng ký hiệu độ. */
#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT5X7_WIDTH  5
#define FONT5X7_HEIGHT 7

/* Bề rộng thực tế mỗi ký tự chiếm khi vẽ chuỗi (glyph + 1 cột ngăn cách).
 * SSD1306_WriteChar() trả về đúng giá trị này. */
#define FONT5X7_ADVANCE (FONT5X7_WIDTH + 1)

/* Ký hiệu độ (°) không nằm trong ASCII nên được nhét vào ô 0x7F.
 * Dùng trong chuỗi C: "26.5\x7FC". */
#define FONT5X7_DEGREE_CHAR '\x7F'

/* Trả về con trỏ tới glyph 5 cột (mỗi cột 1 byte, bit0 = hàng trên cùng,
 * cùng layout với GDDRAM) của ký tự `ch`, hoặc NULL nếu ký tự nằm ngoài
 * dải 0x20..0x7F. */
const uint8_t* font5x7_get_glyph (char ch);

#ifdef __cplusplus
}
#endif

#endif /* FONT5X7_H */
