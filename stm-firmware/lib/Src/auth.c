/*
 * auth.c — Cài đặt quản lý phiên đăng nhập độc quyền (Exclusive Session).
 */
#include "auth.h"
#include "stm32f1xx_hal.h"
#include <string.h>

static Auth_Owner_t current_owner = AUTH_NONE;
static uint32_t     last_activity_ms = 0u;

void Auth_Init(void)
{
    current_owner = AUTH_NONE;
    last_activity_ms = 0u;
}

Auth_Owner_t Auth_GetOwner(void)
{
    return current_owner;
}

bool Auth_IsOwner(Auth_Owner_t claimant)
{
    if (claimant == AUTH_NONE) {
        return false;
    }
    return (current_owner == claimant);
}

bool Auth_VerifyPIN(const char *pin)
{
    if (pin == NULL) {
        return false;
    }
    if (strlen(pin) != AUTH_PIN_LEN) {
        return false;
    }
    return (strcmp(pin, AUTH_PIN_CODE) == 0);
}

Auth_Login_Result_t Auth_Login(Auth_Owner_t claimant, const char *pin, bool *out_kicked_ble)
{
    if (out_kicked_ble != NULL) {
        *out_kicked_ble = false;
    }

    if (!Auth_VerifyPIN(pin)) {
        return AUTH_LOGIN_FAIL;
    }

    if (claimant == AUTH_BLE) {
        /* Local đang giữ phiên thì cấm BLE đăng nhập */
        if (current_owner == AUTH_LOCAL) {
            return AUTH_LOGIN_BUSY_LOCAL;
        }
        current_owner = AUTH_BLE;
        last_activity_ms = HAL_GetTick();
        return AUTH_LOGIN_OK;
    }

    if (claimant == AUTH_LOCAL) {
        /* Local đăng nhập luôn được ưu tiên, kick phiên BLE nếu có */
        if (current_owner == AUTH_BLE) {
            if (out_kicked_ble != NULL) {
                *out_kicked_ble = true;
            }
        }
        current_owner = AUTH_LOCAL;
        last_activity_ms = HAL_GetTick();
        return AUTH_LOGIN_OK;
    }

    return AUTH_LOGIN_FAIL;
}

bool Auth_Logout(Auth_Owner_t claimant)
{
    if ((claimant != AUTH_NONE) && (current_owner == claimant)) {
        current_owner = AUTH_NONE;
        return true;
    }
    return false;
}

void Auth_NotifyActivity(Auth_Owner_t claimant)
{
    if ((claimant != AUTH_NONE) && (current_owner == claimant)) {
        last_activity_ms = HAL_GetTick();
    }
}

void Auth_Task(uint32_t now_ms)
{
    if (current_owner != AUTH_NONE) {
        if ((now_ms - last_activity_ms) >= AUTH_SESSION_TIMEOUT_MS) {
            current_owner = AUTH_NONE;
        }
    }
}

const char *Auth_OwnerToString(Auth_Owner_t owner)
{
    switch (owner) {
    case AUTH_LOCAL:
        return "LOCAL";
    case AUTH_BLE:
        return "BLE";
    case AUTH_NONE:
    default:
        return "NONE";
    }
}
