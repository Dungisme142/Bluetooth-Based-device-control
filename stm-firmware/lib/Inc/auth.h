/*
 * auth.h — Quản lý xác thực và phiên đăng nhập độc quyền (Exclusive Session).
 *
 * Ba trạng thái quyền:
 *      AUTH_NONE   Chưa ai đăng nhập: chỉ đọc được telemetry, cấm điều khiển GPIO.
 *      AUTH_LOCAL  Người dùng tại chỗ (OLED + nút bấm) nắm quyền điều khiển.
 *      AUTH_BLE    Người dùng từ xa qua Bluetooth nắm quyền điều khiển.
 *
 * Quy tắc:
 *      - Mã PIN cố định 4 chữ số "1234" (không lưu Flash, không lockout).
 *      - Local đăng nhập luôn có quyền tối thượng: kick phiên BLE ngay lập tức.
 *      - Phiên tự động kết thúc sau 60 giây không có hoạt động.
 */
#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>

#define AUTH_PIN_CODE           "1234"
#define AUTH_PIN_LEN            4u
#define AUTH_SESSION_TIMEOUT_MS 60000u
#define AUTH_KEYPAD_TIMEOUT_MS  30000u

typedef enum {
    AUTH_NONE = 0,
    AUTH_LOCAL,
    AUTH_BLE
} Auth_Owner_t;

typedef enum {
    AUTH_LOGIN_OK = 0,
    AUTH_LOGIN_FAIL,
    AUTH_LOGIN_BUSY_LOCAL
} Auth_Login_Result_t;

/**
 * @brief Khởi tạo trạng thái xác thực lúc boot.
 */
void Auth_Init(void);

/**
 * @brief Lấy chủ sở hữu phiên hiện tại.
 */
Auth_Owner_t Auth_GetOwner(void);

/**
 * @brief Kiểm tra xem claimant có phải là chủ phiên hiện tại hay không.
 */
bool Auth_IsOwner(Auth_Owner_t claimant);

/**
 * @brief Kiểm tra mã PIN có đúng định dạng và khớp "1234" hay không.
 */
bool Auth_VerifyPIN(const char *pin);

/**
 * @brief Yêu cầu đăng nhập phiên độc quyền.
 *
 * @param claimant       Bên yêu cầu: AUTH_LOCAL hoặc AUTH_BLE.
 * @param pin            Chuỗi mã PIN (4 ký tự).
 * @param out_kicked_ble Trả về true nếu Local đăng nhập và đã thu hồi quyền của BLE.
 * @return Auth_Login_Result_t Kết quả đăng nhập.
 */
Auth_Login_Result_t Auth_Login(Auth_Owner_t claimant, const char *pin, bool *out_kicked_ble);

/**
 * @brief Đăng xuất phiên điều khiển.
 *
 * @param claimant Bên yêu cầu đăng xuất.
 * @return true nếu đăng xuất thành công, false nếu không phải chủ phiên.
 */
bool Auth_Logout(Auth_Owner_t claimant);

/**
 * @brief Báo nhận có hoạt động từ chủ phiên để gia hạn session timer (60s).
 */
void Auth_NotifyActivity(Auth_Owner_t claimant);

/**
 * @brief Quản lý kiểm tra timeout phiên trong superloop.
 *
 * @param now_ms Mốc tick hiện tại (HAL_GetTick()).
 */
void Auth_Task(uint32_t now_ms);

/**
 * @brief Chuyển đổi Auth_Owner_t sang chuỗi ("NONE", "LOCAL", "BLE").
 */
const char *Auth_OwnerToString(Auth_Owner_t owner);

#endif /* AUTH_H */
