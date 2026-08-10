/* Bảng font 5x7 pixel: khoảng trắng, '0'-'9', 'A'-'Z', 'a'-'z'. */
#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT5X7_WIDTH  5
#define FONT5X7_HEIGHT 7

/* Trả về con trỏ tới glyph 5 cột (mỗi cột 1 byte, bit0 = hàng trên cùng,
 * cùng layout với GDDRAM) của ký tự `ch`, hoặc NULL nếu ký tự không có
 * trong bảng (chỉ hỗ trợ ' ', '0'-'9', 'A'-'Z', 'a'-'z'). */
const uint8_t* font5x7_get_glyph (char ch);

#ifdef __cplusplus
}
#endif

#endif /* FONT5X7_H */
