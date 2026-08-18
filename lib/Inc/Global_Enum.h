/*
 * Global_Enum.h — Các kiểu liệt kê dùng chung cho mọi module.
 *
 * Chỉ đặt ở đây những enum mà từ hai module trở lên cần tới. Enum riêng của
 * một module thì để trong header của module đó.
 *
 * Đúng/sai thuần tuý dùng `bool` của <stdbool.h>, không định nghĩa kiểu logic
 * riêng: enum chỉ dành cho trạng thái có nhiều hơn hai giá trị.
 */
#ifndef GLOBAL_ENUM_H_
#define GLOBAL_ENUM_H_

/* Kết quả của một hành động: mọi hàm public trả kiểu này thay vì int trần. */
typedef enum {
    DEV_SUCCESS = 0,
    DEV_FAIL
} Developer_Action_Result_t;

#endif /* GLOBAL_ENUM_H_ */
