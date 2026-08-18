/*
 * app_state.c — Trạng thái ứng dụng: relay, LED chỉ báo và chuỗi báo trạng thái.
 */

#include "app_state.h"

#include "board.h"
#include "MKE_M05_RELAY.h"

#include <stdarg.h>
#include <stdio.h>

System_State_t system_state;

static MKE_M05_RELAY_HandleTypeDef relay1 = { RELAY_PORT, RELAY_PIN, MKE_M05_RELAY_OFF };

static size_t App_State_Append(char *out, size_t out_size, size_t off, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

Developer_Action_Result_t App_State_Init(void)
{
    /* PB12 — relay. Board_Init() đã bật clock GPIOB trước đó. */
    if (MKE_M05_RELAY_Init(&relay1) != DEV_SUCCESS) {
        return DEV_FAIL;
    }

    App_State_SetRelay(false);
    return DEV_SUCCESS;
}

void App_State_SetRelay(bool on)
{
    system_state.relay_on = on;
    system_state.status_led_on = on;

    MKE_M05_RELAY_SetState(&relay1, on ? MKE_M05_RELAY_ON : MKE_M05_RELAY_OFF);
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN,
                      on ? STATUS_LED_ON_STATE : STATUS_LED_OFF_STATE);

    UI_Log(on ? "RELAY ON" : "RELAY OFF");
}

void App_State_ToggleHeartbeat(void)
{
    HAL_GPIO_TogglePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN);
    system_state.heartbeat_led_on =
        (HAL_GPIO_ReadPin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN) == HEARTBEAT_LED_ON_STATE);
}

/**
 * @brief Nối thêm một mẩu vào `out` tại vị trí `off`, không bao giờ tràn buffer.
 * @retval Vị trí ghi mới; bằng out_size-1 nếu buffer đã đầy.
 */
static size_t App_State_Append(char *out, size_t out_size, size_t off, const char *fmt, ...)
{
    va_list args;
    int     written;

    if (off >= (out_size - 1u)) {
        return off;
    }

    va_start(args, fmt);
    written = vsnprintf(out + off, out_size - off, fmt, args);
    va_end(args);

    if (written < 0) {
        return off;
    }
    if ((size_t)written >= (out_size - off)) {
        return out_size - 1u;   /* vsnprintf đã cắt bớt, buffer coi như đầy */
    }
    return off + (size_t)written;
}

size_t App_State_FormatStatus(char *out, size_t out_size, uint8_t fields)
{
    size_t off = 0u;

    if ((out == NULL) || (out_size == 0u)) {
        return 0u;
    }
    out[0] = '\0';

    /* Dấu cách chỉ chèn giữa hai trường, nên phụ thuộc vào việc đã có gì trước đó. */
    if ((fields & APP_STATUS_FIELD_TEMP) != 0u) {
        off = App_State_Append(out, out_size, off, "%sTEMP=%uC",
                               (off > 0u) ? " " : "",
                               (unsigned)system_state.temperature_c);
    }
    if ((fields & APP_STATUS_FIELD_HUM) != 0u) {
        off = App_State_Append(out, out_size, off, "%sHUM=%u%%",
                               (off > 0u) ? " " : "",
                               (unsigned)system_state.humidity_pct);
    }
    if ((fields & APP_STATUS_FIELD_RELAY) != 0u) {
        off = App_State_Append(out, out_size, off, "%sRELAY=%s",
                               (off > 0u) ? " " : "",
                               system_state.relay_on ? "ON" : "OFF");
    }
    if ((fields & APP_STATUS_FIELD_LINK) != 0u) {
        off = App_State_Append(out, out_size, off, "%sBT=%s",
                               (off > 0u) ? " " : "",
                               system_state.bluetooth_connected ? "OK" : "NO");
    }

    off = App_State_Append(out, out_size, off, "\r\n");
    return off;
}

void App_State_Snapshot(UI_Data_t *out)
{
    if (out == NULL) {
        return;
    }

    out->temperature_c = system_state.temperature_c;
    out->humidity_pct = system_state.humidity_pct;
    out->relay_on = system_state.relay_on;
    out->status_led_on = system_state.status_led_on;
    out->heartbeat_led_on = system_state.heartbeat_led_on;
    out->bluetooth_connected = system_state.bluetooth_connected;
}
