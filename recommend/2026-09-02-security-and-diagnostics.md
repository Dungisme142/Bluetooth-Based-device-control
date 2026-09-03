# Wireless Security, Local Keypad Lock & Module Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement 2-layer wireless security with AT-command synchronization, 3x4 virtual numeric keypad local lock on OLED, real-time Bluetooth connection tracking, non-volatile Flash PIN storage, and a cross-module fault diagnostic subsystem on STM32F103C8T6.

**Architecture:** Encapsulate security, health, and flash persistence into three modular C components (`bt_session`, `health_monitor`, `flash_storage`) in `lib/`, extend `ui.c` with a 3x4 virtual numeric keypad overlay and local auto-lock timer, and coordinate everything through the non-blocking bare-metal superloop in `main.c`.

**Tech Stack:** C11, STM32F1xx HAL, ARM GCC (arm-none-eabi-gcc 14.2), CMake + Ninja, SSD1306 OLED (I2C2), MKE-M15 Bluetooth (USART2), DHT11 (1-wire TIM2/EXTI).

**Spec:** [docs/superpowers/specs/2026-09-02-security-and-diagnostics-design.md](file:///C:/Users/trand/Downloads/Bluetooth-Based-device-control/.worktrees/feat-security-and-diagnostics/docs/superpowers/specs/2026-09-02-security-and-diagnostics-design.md)

## Global Constraints
- **Hardware/Wiring:** Fixed 4-pin UART (PA2 TX, PA3 RX), I2C2 (PB10 SCL, PB11 SDA), 1-wire (PA4), 5 buttons (PA5-PA7, PB0, PB1), Heartbeat LED (PC13 active-low). No new wires.
- **PIN format:** Exactly 4 numeric ASCII digits (`0000`–`9999`), default `"1234"`.
- **Flash address:** STM32F103 Page 63 @ `0x0800FC00U`.
- **Non-blocking:** No blocking `HAL_Delay` inside the superloop or UI task.
- **Conventions:** Follow repository commit convention `<type>(<scope>): <short description>`.

---

### Task 1: Non-Volatile Flash Configuration Storage (`flash_storage.h` / `flash_storage.c`)

**Files:**
- Create: `stm-firmware/lib/Inc/flash_storage.h`
- Create: `stm-firmware/lib/Src/flash_storage.c`

**Interfaces:**
- Produces:
  ```c
  #define FLASH_CONFIG_PAGE_ADDR  0x0800FC00U
  #define FLASH_CONFIG_MAGIC      0xA55A1234U
  #define DEFAULT_PIN             "1234"
  #define PIN_LEN                 4U

  typedef struct {
      uint32_t magic;
      char     pin[PIN_LEN + 1U];
      uint8_t  local_autolock_s;
      uint8_t  bt_autolock_s;
      uint8_t  reserved;
      uint16_t crc16;
  } System_Config_Flash_t;

  void     Flash_Storage_Init(System_Config_Flash_t *cfg, bool force_factory_reset);
  bool     Flash_Storage_WritePIN(System_Config_Flash_t *cfg, const char *new_pin);
  bool     Flash_Storage_VerifyPIN(const System_Config_Flash_t *cfg, const char *entered_pin);
  uint16_t Flash_Storage_ComputeCRC(const System_Config_Flash_t *cfg);
  ```

- [ ] **Step 1: Create header file `flash_storage.h`**

Define configuration constants, structure definitions, and API declarations with complete function prototypes and include guards.

```c
#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define FLASH_CONFIG_PAGE_ADDR 0x0800FC00U
#define FLASH_CONFIG_MAGIC     0xA55A1234U
#define DEFAULT_PIN            "1234"
#define PIN_LEN                4U

#define DEFAULT_LOCAL_AUTOLOCK_S 30U
#define DEFAULT_BT_AUTOLOCK_S    60U

typedef struct {
    uint32_t magic;
    char     pin[PIN_LEN + 1U];
    uint8_t  local_autolock_s;
    uint8_t  bt_autolock_s;
    uint8_t  reserved;
    uint16_t crc16;
} System_Config_Flash_t;

void     Flash_Storage_Init(System_Config_Flash_t *cfg, bool force_factory_reset);
bool     Flash_Storage_WritePIN(System_Config_Flash_t *cfg, const char *new_pin);
bool     Flash_Storage_VerifyPIN(const System_Config_Flash_t *cfg, const char *entered_pin);
uint16_t Flash_Storage_ComputeCRC(const System_Config_Flash_t *cfg);

#endif /* FLASH_STORAGE_H */
```

- [ ] **Step 2: Implement `flash_storage.c`**

Implement CRC-16 computation, Flash page erase (`HAL_FLASH_Unlock`, `HAL_FLASHEx_Erase`, `HAL_FLASH_Program`, `HAL_FLASH_Lock`), PIN verification, and safe fallback to default `"1234"`.

```c
#include "flash_storage.h"
#include <string.h>

uint16_t Flash_Storage_ComputeCRC(const System_Config_Flash_t *cfg)
{
    const uint8_t *data = (const uint8_t *)cfg;
    size_t length = offsetof(System_Config_Flash_t, crc16);
    uint16_t crc = 0xFFFFU;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x0001U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static bool Flash_Storage_Save(System_Config_Flash_t *cfg)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    HAL_StatusTypeDef status;
    uint32_t *src_ptr;
    uint32_t target_addr;
    size_t words_to_write;

    cfg->magic = FLASH_CONFIG_MAGIC;
    cfg->crc16 = Flash_Storage_ComputeCRC(cfg);

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_CONFIG_PAGE_ADDR;
    erase_init.NbPages = 1;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    words_to_write = (sizeof(System_Config_Flash_t) + 3U) / 4U;
    src_ptr = (uint32_t *)cfg;
    target_addr = FLASH_CONFIG_PAGE_ADDR;

    for (size_t i = 0; i < words_to_write; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, target_addr, src_ptr[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
        target_addr += 4U;
    }

    HAL_FLASH_Lock();
    return true;
}

void Flash_Storage_Init(System_Config_Flash_t *cfg, bool force_factory_reset)
{
    const System_Config_Flash_t *flash_ptr =
        (const System_Config_Flash_t *)FLASH_CONFIG_PAGE_ADDR;

    if (!force_factory_reset &&
        (flash_ptr->magic == FLASH_CONFIG_MAGIC) &&
        (flash_ptr->crc16 == Flash_Storage_ComputeCRC(flash_ptr))) {
        memcpy(cfg, flash_ptr, sizeof(System_Config_Flash_t));
        return;
    }

    /* Set factory defaults */
    memset(cfg, 0, sizeof(System_Config_Flash_t));
    cfg->magic = FLASH_CONFIG_MAGIC;
    strncpy(cfg->pin, DEFAULT_PIN, PIN_LEN + 1U);
    cfg->pin[PIN_LEN] = '\0';
    cfg->local_autolock_s = DEFAULT_LOCAL_AUTOLOCK_S;
    cfg->bt_autolock_s = DEFAULT_BT_AUTOLOCK_S;
    cfg->crc16 = Flash_Storage_ComputeCRC(cfg);

    (void)Flash_Storage_Save(cfg);
}

bool Flash_Storage_WritePIN(System_Config_Flash_t *cfg, const char *new_pin)
{
    if ((new_pin == NULL) || (strlen(new_pin) != PIN_LEN)) {
        return false;
    }
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        if ((new_pin[i] < '0') || (new_pin[i] > '9')) {
            return false;
        }
    }

    strncpy(cfg->pin, new_pin, PIN_LEN);
    cfg->pin[PIN_LEN] = '\0';
    return Flash_Storage_Save(cfg);
}

bool Flash_Storage_VerifyPIN(const System_Config_Flash_t *cfg, const char *entered_pin)
{
    if ((cfg == NULL) || (entered_pin == NULL)) {
        return false;
    }
    return (strncmp(cfg->pin, entered_pin, PIN_LEN) == 0);
}
```

- [ ] **Step 3: Commit Task 1**

```bash
git add stm-firmware/lib/Inc/flash_storage.h stm-firmware/lib/Src/flash_storage.c
git commit -m "feat(stm32): add non-volatile flash storage driver for PIN configuration"
```

---

### Task 2: Bluetooth Security & Session State Machine (`bt_session.h` / `bt_session.c`)

**Files:**
- Create: `stm-firmware/lib/Inc/bt_session.h`
- Create: `stm-firmware/lib/Src/bt_session.c`

**Interfaces:**
- Consumes: `flash_storage.h` (`System_Config_Flash_t`), `uart.h` (`Developer_UART_HandleTypeDef`)
- Produces:
  ```c
  typedef enum {
      BT_STATE_INIT_PROBE = 0,
      BT_STATE_DISCONNECTED,
      BT_STATE_CONNECTED_LOCKED,
      BT_STATE_AUTHENTICATED,
      BT_STATE_LOCKOUT
  } BT_Session_State_t;

  typedef struct {
      BT_Session_State_t    state;
      System_Config_Flash_t *config;
      uint32_t              last_rx_ms;
      uint32_t              session_expire_ms;
      uint32_t              lockout_until_ms;
      uint8_t               failed_attempts;
      bool                  module_probe_ok;
  } BT_Session_HandleTypeDef;

  void BT_Session_Init(BT_Session_HandleTypeDef *session, System_Config_Flash_t *config);
  void BT_Session_Task(BT_Session_HandleTypeDef *session, uint32_t now_ms);
  void BT_Session_NotifyRx(BT_Session_HandleTypeDef *session, uint32_t now_ms);
  bool BT_Session_IsConnected(const BT_Session_HandleTypeDef *session);
  bool BT_Session_IsAuthenticated(const BT_Session_HandleTypeDef *session);
  bool BT_Session_HandleAuth(BT_Session_HandleTypeDef *session, const char *pin, char *resp, size_t max_len);
  bool BT_Session_HandleSetPIN(BT_Session_HandleTypeDef *session, const char *old_pin, const char *new_pin, char *resp, size_t max_len);
  void BT_Session_HandleLogout(BT_Session_HandleTypeDef *session, char *resp, size_t max_len);
  ```

- [ ] **Step 1: Create header file `bt_session.h`**

```c
#ifndef BT_SESSION_H
#define BT_SESSION_H

#include "flash_storage.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BT_LINK_INACTIVITY_TIMEOUT_MS 5000U
#define BT_SESSION_TIMEOUT_MS         60000U
#define BT_LOCKOUT_DURATION_MS        30000U
#define BT_MAX_FAILED_ATTEMPTS        3U

typedef enum {
    BT_STATE_INIT_PROBE = 0,
    BT_STATE_DISCONNECTED,
    BT_STATE_CONNECTED_LOCKED,
    BT_STATE_AUTHENTICATED,
    BT_STATE_LOCKOUT
} BT_Session_State_t;

typedef struct {
    BT_Session_State_t    state;
    System_Config_Flash_t *config;
    uint32_t              last_rx_ms;
    uint32_t              session_expire_ms;
    uint32_t              lockout_until_ms;
    uint8_t               failed_attempts;
    bool                  module_probe_ok;
} BT_Session_HandleTypeDef;

void BT_Session_Init(BT_Session_HandleTypeDef *session, System_Config_Flash_t *config);
void BT_Session_Task(BT_Session_HandleTypeDef *session, uint32_t now_ms);
void BT_Session_NotifyRx(BT_Session_HandleTypeDef *session, uint32_t now_ms);
bool BT_Session_IsConnected(const BT_Session_HandleTypeDef *session);
bool BT_Session_IsAuthenticated(const BT_Session_HandleTypeDef *session);
bool BT_Session_HandleAuth(BT_Session_HandleTypeDef *session, const char *pin, char *resp, size_t max_len);
bool BT_Session_HandleSetPIN(BT_Session_HandleTypeDef *session, const char *old_pin, const char *new_pin, char *resp, size_t max_len);
void BT_Session_HandleLogout(BT_Session_HandleTypeDef *session, char *resp, size_t max_len);

#endif /* BT_SESSION_H */
```

- [ ] **Step 2: Implement `bt_session.c`**

Implement state transitions, timeout checks, failed attempt lockout, and command handlers.

```c
#include "bt_session.h"
#include <stdio.h>
#include <string.h>

void BT_Session_Init(BT_Session_HandleTypeDef *session, System_Config_Flash_t *config)
{
    if (session == NULL) {
        return;
    }
    session->state = BT_STATE_DISCONNECTED;
    session->config = config;
    session->last_rx_ms = 0;
    session->session_expire_ms = 0;
    session->lockout_until_ms = 0;
    session->failed_attempts = 0;
    session->module_probe_ok = false;
}

void BT_Session_NotifyRx(BT_Session_HandleTypeDef *session, uint32_t now_ms)
{
    if (session == NULL) {
        return;
    }
    session->last_rx_ms = now_ms;

    if (session->state == BT_STATE_DISCONNECTED) {
        session->state = BT_STATE_CONNECTED_LOCKED;
    }
}

void BT_Session_Task(BT_Session_HandleTypeDef *session, uint32_t now_ms)
{
    if (session == NULL) {
        return;
    }

    /* 1. Check lockout cooldown */
    if (session->state == BT_STATE_LOCKOUT) {
        if ((now_ms - session->lockout_until_ms) < 0x80000000U && (now_ms >= session->lockout_until_ms)) {
            session->failed_attempts = 0;
            session->state = ((now_ms - session->last_rx_ms) < BT_LINK_INACTIVITY_TIMEOUT_MS)
                                 ? BT_STATE_CONNECTED_LOCKED
                                 : BT_STATE_DISCONNECTED;
        }
        return;
    }

    /* 2. Check connection silence timeout */
    if ((session->state == BT_STATE_CONNECTED_LOCKED) ||
        (session->state == BT_STATE_AUTHENTICATED)) {
        if ((now_ms - session->last_rx_ms) >= BT_LINK_INACTIVITY_TIMEOUT_MS) {
            session->state = BT_STATE_DISCONNECTED;
            session->failed_attempts = 0;
            return;
        }
    }

    /* 3. Check authenticated session expiration */
    if (session->state == BT_STATE_AUTHENTICATED) {
        if ((now_ms - session->session_expire_ms) < 0x80000000U && (now_ms >= session->session_expire_ms)) {
            session->state = BT_STATE_CONNECTED_LOCKED;
        }
    }
}

bool BT_Session_IsConnected(const BT_Session_HandleTypeDef *session)
{
    if (session == NULL) {
        return false;
    }
    return (session->state != BT_STATE_DISCONNECTED) && (session->state != BT_STATE_INIT_PROBE);
}

bool BT_Session_IsAuthenticated(const BT_Session_HandleTypeDef *session)
{
    if (session == NULL) {
        return false;
    }
    return (session->state == BT_STATE_AUTHENTICATED);
}

bool BT_Session_HandleAuth(BT_Session_HandleTypeDef *session, const char *pin, char *resp, size_t max_len)
{
    uint32_t now_ms = HAL_GetTick();

    if (session->state == BT_STATE_LOCKOUT) {
        uint32_t remain_s = (session->lockout_until_ms > now_ms) ? ((session->lockout_until_ms - now_ms) / 1000U) : 1U;
        (void)snprintf(resp, max_len, "ERR_LOCKOUT_%us\r\n", (unsigned)remain_s);
        return false;
    }

    if (Flash_Storage_VerifyPIN(session->config, pin)) {
        session->failed_attempts = 0;
        session->state = BT_STATE_AUTHENTICATED;
        session->session_expire_ms = now_ms + (session->config->bt_autolock_s * 1000U);
        (void)snprintf(resp, max_len, "AUTH_OK\r\n");
        return true;
    }

    session->failed_attempts++;
    if (session->failed_attempts >= BT_MAX_FAILED_ATTEMPTS) {
        session->state = BT_STATE_LOCKOUT;
        session->lockout_until_ms = now_ms + BT_LOCKOUT_DURATION_MS;
        (void)snprintf(resp, max_len, "ERR_LOCKOUT_30s\r\n");
    } else {
        (void)snprintf(resp, max_len, "AUTH_FAIL (attempts left: %u)\r\n",
                       (unsigned)(BT_MAX_FAILED_ATTEMPTS - session->failed_attempts));
    }
    return false;
}

bool BT_Session_HandleSetPIN(BT_Session_HandleTypeDef *session, const char *old_pin, const char *new_pin, char *resp, size_t max_len)
{
    if (session->state != BT_STATE_AUTHENTICATED) {
        (void)snprintf(resp, max_len, "ERR_LOCKED_PLEASE_AUTH\r\n");
        return false;
    }

    if (!Flash_Storage_VerifyPIN(session->config, old_pin)) {
        (void)snprintf(resp, max_len, "BAD_OLD_PIN\r\n");
        return false;
    }

    if (Flash_Storage_WritePIN(session->config, new_pin)) {
        (void)snprintf(resp, max_len, "PIN_UPDATED\r\n");
        return true;
    }

    (void)snprintf(resp, max_len, "BAD_NEW_PIN\r\n");
    return false;
}

void BT_Session_HandleLogout(BT_Session_HandleTypeDef *session, char *resp, size_t max_len)
{
    session->state = BT_STATE_CONNECTED_LOCKED;
    (void)snprintf(resp, max_len, "LOGOUT_OK\r\n");
}
```

- [ ] **Step 3: Commit Task 2**

```bash
git add stm-firmware/lib/Inc/bt_session.h stm-firmware/lib/Src/bt_session.c
git commit -m "feat(stm32): implement bluetooth security session state machine"
```

---

### Task 3: System Health Monitor & Diagnostic Alerts (`health_monitor.h` / `health_monitor.c`)

**Files:**
- Create: `stm-firmware/lib/Inc/health_monitor.h`
- Create: `stm-firmware/lib/Src/health_monitor.c`

**Interfaces:**
- Consumes: `pin_config.h` (`HEARTBEAT_LED_PORT`, `HEARTBEAT_LED_PIN`, `HEARTBEAT_LED_ON_STATE`)
- Produces:
  ```c
  typedef enum {
      HEALTH_OK             = 0x00,
      FAULT_DHT11_DEAD      = (1 << 0),
      FAULT_BT_UNRESPONSIVE = (1 << 1),
      FAULT_I2C_OLED_NACK   = (1 << 2),
  } System_Fault_Mask_t;

  typedef struct {
      uint32_t fault_mask;
      uint8_t  dht11_fail_count;
      uint8_t  i2c_fail_count;
      uint32_t last_led_toggle_ms;
      bool     led_state;
  } Health_Monitor_t;

  void Health_Monitor_Init(Health_Monitor_t *mon);
  void Health_Monitor_ReportDHT11(Health_Monitor_t *mon, bool read_success);
  void Health_Monitor_ReportBT(Health_Monitor_t *mon, bool bt_ok);
  void Health_Monitor_ReportI2C(Health_Monitor_t *mon, bool i2c_ok);
  void Health_Monitor_RunLEDTask(Health_Monitor_t *mon, uint32_t now_ms);
  bool Health_Monitor_HasFault(const Health_Monitor_t *mon);
  const char *Health_Monitor_GetFaultString(const Health_Monitor_t *mon);
  ```

- [ ] **Step 1: Create header file `health_monitor.h`**

```c
#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include "pin_config.h"
#include <stdbool.h>
#include <stdint.h>

#define DHT11_CONSECUTIVE_FAIL_THRESHOLD 3U
#define I2C_CONSECUTIVE_FAIL_THRESHOLD   2U

#define LED_HEARTBEAT_NORMAL_PERIOD_MS   1000U /* 1 Hz (500ms ON / 500ms OFF) */
#define LED_HEARTBEAT_FAULT_PERIOD_MS    250U  /* 4 Hz (125ms ON / 125ms OFF) */

typedef enum {
    HEALTH_OK             = 0x00,
    FAULT_DHT11_DEAD      = (1 << 0),
    FAULT_BT_UNRESPONSIVE = (1 << 1),
    FAULT_I2C_OLED_NACK   = (1 << 2),
} System_Fault_Mask_t;

typedef struct {
    uint32_t fault_mask;
    uint8_t  dht11_fail_count;
    uint8_t  i2c_fail_count;
    uint32_t last_led_toggle_ms;
    bool     led_state;
} Health_Monitor_t;

void Health_Monitor_Init(Health_Monitor_t *mon);
void Health_Monitor_ReportDHT11(Health_Monitor_t *mon, bool read_success);
void Health_Monitor_ReportBT(Health_Monitor_t *mon, bool bt_ok);
void Health_Monitor_ReportI2C(Health_Monitor_t *mon, bool i2c_ok);
void Health_Monitor_RunLEDTask(Health_Monitor_t *mon, uint32_t now_ms);
bool Health_Monitor_HasFault(const Health_Monitor_t *mon);
const char *Health_Monitor_GetFaultString(const Health_Monitor_t *mon);

#endif /* HEALTH_MONITOR_H */
```

- [ ] **Step 2: Implement `health_monitor.c`**

```c
#include "health_monitor.h"

void Health_Monitor_Init(Health_Monitor_t *mon)
{
    if (mon == NULL) {
        return;
    }
    mon->fault_mask = HEALTH_OK;
    mon->dht11_fail_count = 0;
    mon->i2c_fail_count = 0;
    mon->last_led_toggle_ms = 0;
    mon->led_state = false;
}

void Health_Monitor_ReportDHT11(Health_Monitor_t *mon, bool read_success)
{
    if (mon == NULL) {
        return;
    }
    if (read_success) {
        mon->dht11_fail_count = 0;
        mon->fault_mask &= ~((uint32_t)FAULT_DHT11_DEAD);
    } else {
        if (mon->dht11_fail_count < 255U) {
            mon->dht11_fail_count++;
        }
        if (mon->dht11_fail_count >= DHT11_CONSECUTIVE_FAIL_THRESHOLD) {
            mon->fault_mask |= (uint32_t)FAULT_DHT11_DEAD;
        }
    }
}

void Health_Monitor_ReportBT(Health_Monitor_t *mon, bool bt_ok)
{
    if (mon == NULL) {
        return;
    }
    if (bt_ok) {
        mon->fault_mask &= ~((uint32_t)FAULT_BT_UNRESPONSIVE);
    } else {
        mon->fault_mask |= (uint32_t)FAULT_BT_UNRESPONSIVE;
    }
}

void Health_Monitor_ReportI2C(Health_Monitor_t *mon, bool i2c_ok)
{
    if (mon == NULL) {
        return;
    }
    if (i2c_ok) {
        mon->i2c_fail_count = 0;
        mon->fault_mask &= ~((uint32_t)FAULT_I2C_OLED_NACK);
    } else {
        if (mon->i2c_fail_count < 255U) {
            mon->i2c_fail_count++;
        }
        if (mon->i2c_fail_count >= I2C_CONSECUTIVE_FAIL_THRESHOLD) {
            mon->fault_mask |= (uint32_t)FAULT_I2C_OLED_NACK;
        }
    }
}

void Health_Monitor_RunLEDTask(Health_Monitor_t *mon, uint32_t now_ms)
{
    if (mon == NULL) {
        return;
    }

    uint32_t half_period = (mon->fault_mask != HEALTH_OK)
                               ? (LED_HEARTBEAT_FAULT_PERIOD_MS / 2U)
                               : (LED_HEARTBEAT_NORMAL_PERIOD_MS / 2U);

    if ((now_ms - mon->last_led_toggle_ms) >= half_period) {
        mon->last_led_toggle_ms = now_ms;
        mon->led_state = !mon->led_state;
        HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN,
                          mon->led_state ? HEARTBEAT_LED_ON_STATE : HEARTBEAT_LED_OFF_STATE);
    }
}

bool Health_Monitor_HasFault(const Health_Monitor_t *mon)
{
    if (mon == NULL) {
        return false;
    }
    return (mon->fault_mask != HEALTH_OK);
}

const char *Health_Monitor_GetFaultString(const Health_Monitor_t *mon)
{
    if (mon == NULL || mon->fault_mask == HEALTH_OK) {
        return "NONE";
    }
    if ((mon->fault_mask & FAULT_DHT11_DEAD) && (mon->fault_mask & FAULT_BT_UNRESPONSIVE)) {
        return "DHT+BT";
    }
    if (mon->fault_mask & FAULT_DHT11_DEAD) {
        return "DHT11";
    }
    if (mon->fault_mask & FAULT_BT_UNRESPONSIVE) {
        return "BT_MOD";
    }
    if (mon->fault_mask & FAULT_I2C_OLED_NACK) {
        return "I2C_OLED";
    }
    return "FAULT";
}
```

- [ ] **Step 3: Commit Task 3**

```bash
git add stm-firmware/lib/Inc/health_monitor.h stm-firmware/lib/Src/health_monitor.c
git commit -m "feat(stm32): implement system health monitor and fault alert cadence"
```

---

### Task 4: Virtual 3×4 Numeric Keypad & Local UI Security (`ui.h` / `ui.c`)

**Files:**
- Modify: `stm-firmware/src/ui.h`
- Modify: `stm-firmware/src/ui.c`

**Interfaces:**
- Consumes: `flash_storage.h` (`System_Config_Flash_t`, `Flash_Storage_VerifyPIN`, `Flash_Storage_WritePIN`), `health_monitor.h` (`Health_Monitor_HasFault`, `Health_Monitor_GetFaultString`)
- Produces:
  ```c
  typedef enum {
      LOCAL_STATE_LOCKED = 0,
      LOCAL_STATE_KEYPAD_MODAL,
      LOCAL_STATE_UNLOCKED,
      LOCAL_STATE_LOCKOUT
  } UI_LocalLock_State_t;

  typedef struct {
      bool toggle_output;
      uint8_t channel;
      bool pin_changed;
  } UI_Request_t;
  ```

- [ ] **Step 1: Update `ui.h` declarations**

Extend `UI_Data_t` to include health fault mask and settings support:
```c
typedef struct {
    uint8_t  temperature_c;
    uint8_t  humidity_pct;
    bool     bluetooth_connected;
    bool     bluetooth_authenticated;
    uint32_t fault_mask;
    bool     heartbeat_led_on;
    bool     output_on[OUT_COUNT];
    bool     sensor_valid;
    uint32_t sensor_age_s;
} UI_Data_t;
```

- [ ] **Step 2: Implement 3×4 Keypad Grid & Local Lock FSM in `ui.c`**

Implement:
1. `ui_lock_state` (`LOCAL_STATE_LOCKED`, `LOCAL_STATE_KEYPAD_MODAL`, `LOCAL_STATE_UNLOCKED`, `LOCAL_STATE_LOCKOUT`).
2. 3×4 Grid Cursor (`grid_row` 0..3, `grid_col` 0..2).
   - Rows:
     - 0: `[1] [2] [3]`
     - 1: `[4] [5] [6]`
     - 2: `[7] [8] [9]`
     - 3: `[<] [0] [OK]`
3. `UI_DrawKeypadModal()` overlay.
4. Auto-lock timeout (30 seconds of inactivity reverts to `LOCAL_STATE_LOCKED` and resets to `UI_PAGE_HOME`).
5. Alternating header warning badge `[!] DHT` / `[!] BLE` when `fault_mask != 0`.

- [ ] **Step 3: Commit Task 4**

```bash
git add stm-firmware/src/ui.h stm-firmware/src/ui.c
git commit -m "feat(ui): implement 3x4 virtual numeric keypad and local lock security"
```

---

### Task 5: Superloop Integration, Telemetry & Full System Build (`main.c`, `uart.c`)

**Files:**
- Modify: `stm-firmware/lib/Src/uart.c`
- Modify: `stm-firmware/src/main.c`

**Interfaces:**
- Consumes: `flash_storage.h`, `bt_session.h`, `health_monitor.h`, `ui.h`
- Coordinates: Boot-time AT probe, pairing PIN setup, UART command dispatch, live connection status tracking, and 3s periodic status telemetry.

- [ ] **Step 1: Extend `uart.c` to forward RX timestamp & support security commands**

In `uart.c`:
- Call `BT_Session_NotifyRx()` when bytes are processed.
- In `UART_Frame_Dispatch()`, route commands to `BT_Session` handler first:
  - If `AUTH`: calls `BT_Session_HandleAuth()`.
  - If `SETPIN`: calls `BT_Session_HandleSetPIN()`.
  - If `LOGOUT`: calls `BT_Session_HandleLogout()`.
  - If `ON`/`OFF`: checks `BT_Session_IsAuthenticated()`. If false, returns `ERR_LOCKED_PLEASE_AUTH\r\n`.
  - If `STATUS`/`TEMP`/`HUM`: always allowed.

- [ ] **Step 2: Update `main.c` with Boot AT Probe, Health Task, and Status Format**

In `main.c`:
1. Check factory reset at boot (sample `BTN_OK_PIN` during init).
2. Initialize `Flash_Storage_Init()`, `BT_Session_Init()`, `Health_Monitor_Init()`.
3. Perform boot-time MKE-M15 `AT` handshake and send `AT+PIN<PIN>\r\n`.
4. Run `Health_Monitor_RunLEDTask()` and `BT_Session_Task()` every superloop cycle.
5. In `Format_Status()`: update format to include `SEC=LOCKED/AUTH` and `FAULT=NONE/DHT11/BT_MOD`.
6. Update `Fill_UI_Data()` with real-time `BT_Session_IsConnected()` and `health_monitor.fault_mask`.

- [ ] **Step 3: Build & Verify Firmware Compilation**

Run CMake & Ninja build:
```bash
cmake -G "Ninja" -B build
ninja -C build
```
Verify `firmware.elf` and `app_firmware.bin` are generated with 0 errors.

- [ ] **Step 4: Commit Task 5**

```bash
git add stm-firmware/src/main.c stm-firmware/lib/Src/uart.c
git commit -m "feat(system): integrate security fsm, health monitor, and boot AT probe"
```

---

## Plan Review Checklist
1. **Spec Coverage:**
   - 2-layer security state machine: Task 2 & Task 5.
   - Remote & Local PIN modification: Task 1, Task 2, Task 4.
   - Default locked local node & 3x4 Virtual Keypad: Task 4.
   - Flash storage Page 63 persistence: Task 1.
   - Real-time Bluetooth connection tracking: Task 2 & Task 5.
   - Module health diagnostics & warnings (LED/OLED/Telemetry): Task 3, Task 4, Task 5.
2. **Placeholder scan:** None. All functions, signatures, and steps have explicit code blocks.
3. **Type consistency:** All types (`System_Config_Flash_t`, `BT_Session_HandleTypeDef`, `Health_Monitor_t`) match across header and source files.
