/*
 * main.c — Toàn bộ firmware trong một file: khởi tạo phần cứng, bảng lệnh
 * Bluetooth, đọc DHT11, điều khiển 5 ngõ ra và vòng lặp chính.
 *
 * Chỉ có ba file khác trong src/:
 *      ui.c / ui.h        5 trang OLED và 5 nút điều khiển
 *      stm32f1xx_it.c     bảng vector ngắt
 *      sysmem.c           _sbrk cho newlib
 *
 * Mọi việc còn lại gọi thẳng driver trong lib/: uart.c (nhận lệnh), DHT11.c,
 * SSD1306.c (qua ui.c), Digital_Out.c, Ring_Buffer.c, Command_Selector.c.
 * Không có tầng trung gian nào giữa main.c và lib/.
 */
#include "main.h"

#include "Command_Selector.h"
#include "DHT11.h"
#include "Digital_Out.h"
#include "Ring_Buffer.h"
#include "auth.h"
#include "health_monitor.h"
#include "uart.h"
#include "ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*==================== Hằng số cấu hình ====================*/

/* Chu kỳ các tác vụ nền (ms) */
#define SENSOR_PERIOD_MS    2000u /* Đọc DHT11 */
#define STATUS_PERIOD_MS    3000u /* Báo trạng thái về điện thoại */
#define HEARTBEAT_PERIOD_MS 1000u /* Nhấp nháy LED PC13 báo còn sống */

/* Chờ DHT11 hoàn tất một phép đo: poll mỗi 5 ms, bỏ cuộc sau 500 ms */
#define DHT11_POLL_INTERVAL_MS 5u
#define DHT11_POLL_TIMEOUT_MS  500u

#define UART_RX_BUFFER_SIZE    128u
#define UART_FRAME_BUFFER_SIZE 128u

/* Đủ rộng cho chuỗi trạng thái dài nhất kèm CRLF */
#define STATUS_TEXT_SIZE 96u

/* Tần số bộ đếm TIM2 — 1 MHz để mỗi tick bằng đúng 1 us */
#define TIM2_COUNTER_FREQ_HZ 1000000u

/*==================== Handle ngoại vi ====================*/

/* Không static: stm32f1xx_it.c đẩy ngắt vào huart2 và htim2 qua extern. */
UART_HandleTypeDef huart2;
I2C_HandleTypeDef hi2c2;
TIM_HandleTypeDef htim2;

/*==================== Trạng thái hệ thống ====================*/

static uint8_t last_temp;          /* Nhiệt độ đọc lần cuối (độ C) */
static uint8_t last_humidity;      /* Độ ẩm đọc lần cuối (%) */
static bool output_on[OUT_COUNT];  /* true = kênh đang đóng */
static bool bluetooth_connected;   /* true = đang có liên lạc với module BT */
static bool sensor_valid;          /* true = đã từng đọc DHT11 thành công */
static uint32_t last_sensor_ok_ms; /* Mốc tick của lần đọc thành công cuối */

static Health_Monitor_t health_mon;
static bool             ble_kick_pending = false;
static UI_AlarmState_t  current_alarm_state = ALARM_STATE_DHT_FAULT;
static uint32_t         last_alarm_toggle_ms = 0u;
static GPIO_PinState    alarm_pin_level = GPIO_PIN_RESET;

/* 4 kênh ngõ ra PB15..PB12 */
static Digital_Out_HandleTypeDef outputs[OUT_COUNT] = {
    {OUT1_PORT, OUT1_PIN, DIGITAL_OUT_OFF},
    {OUT2_PORT, OUT2_PIN, DIGITAL_OUT_OFF},
    {OUT3_PORT, OUT3_PIN, DIGITAL_OUT_OFF},
    {OUT4_PORT, OUT4_PIN, DIGITAL_OUT_OFF},
};

static const GPIO_PinState output_on_state[OUT_COUNT] = {
    OUT1_ON_STATE, OUT2_ON_STATE, OUT3_ON_STATE, OUT4_ON_STATE,
};

/*==================== Cụm nhận lệnh qua UART ====================*/

/* Buffer cấp phát tĩnh, kích thước bằng macro — không có malloc sau init. */
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t uart_frame_buffer[UART_FRAME_BUFFER_SIZE];

/* Ô nhận một byte mà HAL_UART_Receive_IT() ghi vào. ISR đọc rồi đẩy ngay sang
 * bộ đệm vòng, nên không có ai khác chạm vào nó cùng lúc. */
static uint8_t uart_rx_byte;

static Ring_Buffer_HandleTypeDef ring_buffer_handler;
static Developer_UART_HandleTypeDef developer_uart_handler;

/*==================== Cấu hình DHT11 ====================*/

static DHT11_Config_t dht11_cfg = {DHT11_PORT, DHT11_PIN, DHT11_EXTI_IRQn, &htim2};

/*==================== Nguyên mẫu hàm ====================*/

void SystemClock_Config(void);
void Error_Handler(void);

static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM2_Init(void);

static void Set_Output(uint8_t channel, bool on);
static void Format_Status(char *out, size_t out_size);
static bool DHT11_ReadOnce(void);
static void Send_Status(void);
static void Fill_UI_Data(UI_Data_t *out);
static void Alarm_Task(uint32_t now_ms);

static void Command_LOGIN(char *return_msg, const char *args);
static void Command_LOGOUT(char *return_msg, const char *args);
static void Command_SetOutputs(char *return_msg, const char *args, bool on);
static void Command_ON(char *return_msg, const char *args);
static void Command_OFF(char *return_msg, const char *args);
static void Command_STATUS(char *return_msg, const char *args);
static void Command_TEMP(char *return_msg, const char *args);
static void Command_HUM(char *return_msg, const char *args);
static void Command_AUTO(char *return_msg, const char *args);

/*==================== Bảng lệnh Bluetooth ====================*/

Command_HandleTypeDef Command_Menu[] = {
    {"LOGIN", Command_LOGIN},   /* Đăng nhập phiên BLE */
    {"LOGOUT", Command_LOGOUT}, /* Đăng xuất phiên BLE */
    {"ON", Command_ON},         /* Đóng một kênh, hoặc tất cả */
    {"OFF", Command_OFF},       /* Mở một kênh, hoặc tất cả */
    {"STATUS", Command_STATUS}, /* Nhiệt độ, độ ẩm, ngõ ra, kết nối, phiên, cảnh báo */
    {"TEMP", Command_TEMP},     /* Chỉ nhiệt độ */
    {"HUM", Command_HUM},       /* Chỉ độ ẩm */
    {"AUTO", Command_AUTO}      /* Chế độ tự động — chưa triển khai */
};

uint8_t Command_Menu_Size = (uint8_t)(sizeof(Command_Menu) / sizeof(Command_Menu[0]));

/*==================== Vòng lặp chính ====================*/

int main(void)
{
    uint32_t now_ms;
    uint32_t last_sensor_tick_ms;
    uint32_t last_status_tick_ms;
    uint32_t last_heartbeat_tick_ms;
    uint8_t i;
    UI_Data_t ui_data;
    UI_Request_t ui_request;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C2_Init();
    MX_TIM2_Init();

    /* 4 ngõ ra PB15..PB12. MX_GPIO_Init() đã ghi sẵn mức TẮT lên các chân này nên bước
     * chuyển sang output dưới đây không làm thiết bị nháy. */
    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        if (Digital_Out_Init(&outputs[i]) != DEV_SUCCESS) {
            Error_Handler();
        }
        Set_Output(i, false);
    }

    Auth_Init();
    Health_Monitor_Init(&health_mon);

    if (DHT11_Init(&dht11_cfg) != DEV_SUCCESS) {
        Error_Handler();
    }

    UI_Init(&hi2c2);

    if (Ring_Buffer_Init(&ring_buffer_handler, uart_rx_buffer, UART_RX_BUFFER_SIZE,
                         sizeof(uint8_t)) != DEV_SUCCESS) {
        Error_Handler();
    }

    if (Developer_UART_Handler_Init(&developer_uart_handler, &huart2, &ring_buffer_handler,
                                    uart_frame_buffer, sizeof(uint8_t),
                                    UART_FRAME_BUFFER_SIZE) != DEV_SUCCESS) {
        Error_Handler();
    }

    if (HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1u) != HAL_OK) {
        Error_Handler();
    }

    UART_Print(&developer_uart_handler, "MKE-M15 ready\r\n");
    UI_Log("BOOT OK");

    last_sensor_tick_ms = HAL_GetTick();
    last_status_tick_ms = last_sensor_tick_ms;
    last_heartbeat_tick_ms = last_sensor_tick_ms;

    while (1) {
        now_ms = HAL_GetTick();

        /* Bảo trì timeout phiên xác thực độc quyền */
        Auth_Task(now_ms);

        /* Máy trạng thái cảnh báo tự động PA8 */
        Alarm_Task(now_ms);

        /* Mọi mốc thời gian đều so bằng hiệu (now - last) */
        if ((now_ms - last_sensor_tick_ms) >= SENSOR_PERIOD_MS) {
            bool read_ok = DHT11_ReadOnce();
            UI_Log(read_ok ? "DHT OK" : "DHT BAD");
            last_sensor_tick_ms = now_ms;
        }

        if ((now_ms - last_status_tick_ms) >= STATUS_PERIOD_MS) {
            Send_Status();
            last_status_tick_ms = now_ms;
        }

        /* Nhấp nháy PC13 mỗi giây */
        if ((now_ms - last_heartbeat_tick_ms) >= HEARTBEAT_PERIOD_MS) {
            HAL_GPIO_TogglePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN);
            last_heartbeat_tick_ms = now_ms;
        }

        /* Đồng bộ kết nối Bluetooth từ UART driver và log sự kiện */
        bool bt_is_connected = (developer_uart_handler.uart_connecting_status == DEVELOPER_UART_CONNECTED);
        Health_Monitor_ReportBT(&health_mon, bt_is_connected);
        if (bt_is_connected != bluetooth_connected) {
            bluetooth_connected = bt_is_connected;
            if (bluetooth_connected) {
                UI_Log("BT LINK UP");
            } else {
                UI_Log("BT LINK DOWN");
            }
        }

        UART_Task(&developer_uart_handler);

        /* Gửi bản tin LOCAL LOGIN - KICKED khi Local chiếm quyền và UART rảnh */
        if (ble_kick_pending && (developer_uart_handler.hal_huart->gState == HAL_UART_STATE_READY)) {
            UART_Print(&developer_uart_handler, "LOCAL LOGIN - KICKED\r\n");
            ble_kick_pending = false;
        }

        Fill_UI_Data(&ui_data);
        UI_Task(now_ms, &ui_data, &ui_request);

        /* Thao tác nút của người dùng Local gia hạn timeout phiên */
        if (ui_request.user_activity) {
            Auth_NotifyActivity(AUTH_LOCAL);
        }

        /* Người dùng Local đăng nhập thành công qua OLED keypad */
        if (ui_request.local_login) {
            bool kicked_ble = false;
            if (Auth_Login(AUTH_LOCAL, AUTH_PIN_CODE, &kicked_ble) == AUTH_LOGIN_OK) {
                UI_Log("LOCAL LOGIN");
                if (kicked_ble) {
                    ble_kick_pending = true;
                }
            }
        }

        /* Đảo ngõ ra từ OLED UI — chỉ thực thi khi Local đang sở hữu phiên độc quyền */
        if (ui_request.toggle_output && (ui_request.channel < (uint8_t)OUT_COUNT)) {
            if (Auth_IsOwner(AUTH_LOCAL)) {
                Set_Output(ui_request.channel, !output_on[ui_request.channel]);
            }
        }
    }
}

/*==================== Điều khiển ngõ ra ====================*/

/**
 * @brief Đóng/mở một kênh ngõ ra và ghi nhật ký UI.
 *
 * Đây là lối vào DUY NHẤT của việc bật/tắt thiết bị: cả lệnh Bluetooth lẫn nút
 * OK trên OLED đều đi qua đây, nên trạng thái không thể lệch giữa hai đường.
 *
 * @param channel  0..OUT_COUNT-1; ngoài dải thì hàm không làm gì.
 */
static void Set_Output(uint8_t channel, bool on)
{
    char label[16];

    if (channel >= (uint8_t)OUT_COUNT) {
        return;
    }

    output_on[channel] = on;

    /* Quy về mức logic thật của kênh rồi mới gọi driver: driver hiểu ON là mức
     * CAO, còn output_on_state[] mới là sự thật của phần cứng. */
    Digital_Out_SetState(&outputs[channel], (output_on_state[channel] == GPIO_PIN_SET)
                                                ? (on ? DIGITAL_OUT_ON : DIGITAL_OUT_OFF)
                                                : (on ? DIGITAL_OUT_OFF : DIGITAL_OUT_ON));

    (void)snprintf(label, sizeof(label), "OUT%u %s", (unsigned)(channel + 1u), on ? "ON" : "OFF");
    UI_Log(label);
}

/*==================== Chuỗi báo trạng thái ====================*/

/**
 * @brief Ghi chuỗi báo trạng thái đầy đủ vào `out`.
 *
 * Một chỗ duy nhất quyết định định dạng: dùng bởi cả Send_Status() (định kỳ
 * 3 s) lẫn Command_STATUS(). Trước đây hai chỗ này tự viết riêng và đã trôi
 * khỏi nhau — một bên có "BT=", một bên không.
 *
 * "OUT=10100" là bản đồ bit của cả 5 kênh, kênh 1 đứng trước. Dạng chuỗi 0/1
 * đọc bằng mắt được ngay trên terminal điện thoại.
 */
static void Format_Status(char *out, size_t out_size)
{
    char        map[OUT_COUNT + 1u];
    uint8_t     i;
    const char *dht_str = Health_Monitor_IsDHTHealthy(&health_mon) ? "OK" : "FAIL";
    const char *bt_str = bluetooth_connected ? "OK" : "NO";
    const char *auth_str = Auth_OwnerToString(Auth_GetOwner());
    const char *alarm_str = "NORMAL";

    if (current_alarm_state == ALARM_STATE_HIGH_HUMIDITY) {
        alarm_str = "HIGH";
    } else if (current_alarm_state == ALARM_STATE_DHT_FAULT) {
        alarm_str = "DHT_FAULT";
    }

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        map[i] = output_on[i] ? '1' : '0';
    }
    map[OUT_COUNT] = '\0';

    (void)snprintf(out, out_size,
                   "TEMP=%uC HUM=%u%% DHT=%s BT=%s AUTH=%s ALARM=%s OUT=%s\r\n",
                   (unsigned)last_temp, (unsigned)last_humidity,
                   dht_str, bt_str, auth_str, alarm_str, map);
}

static void Send_Status(void)
{
    char status[STATUS_TEXT_SIZE];

    Format_Status(status, sizeof(status));

    /* "%s" là bắt buộc: status là dữ liệu chạy chứ không phải chuỗi định dạng.
     * Nó chứa dấu '%' (từ "HUM=61%"), đưa thẳng vào vị trí format thì vsnprintf
     * bên trong UART_Print() sẽ đọc nó là đặc tả định dạng và lấy đối số không
     * tồn tại. */
    UART_Print(&developer_uart_handler, "%s", status);
}

/*==================== Đọc DHT11 ====================*/

/**
 * @brief Chạy một phép đo DHT11 từ đầu đến cuối và cập nhật trạng thái.
 *
 * CHẶN tới ~500 ms — đây là tác vụ nặng nhất của superloop. Ở 9600 baud nghĩa
 * là tới 500 byte có thể đổ về trong lúc main loop không rút được; bộ đệm vòng
 * 128 byte là để bù cho việc này. ĐỪNG thêm tác vụ chặn thứ hai vào vòng lặp.
 *
 * @retval true nếu đọc được giá trị hợp lệ, false nếu lỗi/quá hạn.
 */
static bool DHT11_ReadOnce(void)
{
    DHT11_Data_t dht_data = {0};
    uint32_t     start_ms;
    uint32_t     last_poll_ms;
    bool         success = false;

    DHT11_StartRequest();

    start_ms = HAL_GetTick();
    /* Lùi một chu kỳ để lần poll đầu tiên chạy ngay, không chờ 5 ms. */
    last_poll_ms = start_ms - DHT11_POLL_INTERVAL_MS;

    while ((HAL_GetTick() - start_ms) < DHT11_POLL_TIMEOUT_MS) {
        DHT11_State_t state;

        /* Giữ nhịp poll bằng hiệu tick thay vì HAL_Delay */
        if ((HAL_GetTick() - last_poll_ms) < DHT11_POLL_INTERVAL_MS) {
            continue;
        }
        last_poll_ms += DHT11_POLL_INTERVAL_MS;

        state = DHT11_ReadData(&dht_data);

        if (state == DHT11_STATE_COMPLETE) {
            if (dht_data.is_valid) {
                last_temp = dht_data.temp_int;
                last_humidity = dht_data.humidity_int;
                last_sensor_ok_ms = HAL_GetTick();
                sensor_valid = true;
                success = true;
            }
            break;
        }

        if (state == DHT11_STATE_ERROR) {
            break;
        }
    }

    /* Báo cáo kết quả đo vào hệ thống giám sát sức khỏe */
    Health_Monitor_ReportDHT11(&health_mon, success);

    return success;
}

/*==================== Máy trạng thái cảnh báo tự động PA8 ====================*/

static void Alarm_Task(uint32_t now_ms)
{
    UI_AlarmState_t target_state;

    if (!Health_Monitor_IsDHTHealthy(&health_mon)) {
        target_state = ALARM_STATE_DHT_FAULT;
    } else if (last_humidity > 90u) {
        target_state = ALARM_STATE_HIGH_HUMIDITY;
    } else {
        target_state = ALARM_STATE_NORMAL;
    }

    current_alarm_state = target_state;

    switch (current_alarm_state) {
    case ALARM_STATE_NORMAL:
        alarm_pin_level = GPIO_PIN_RESET;
        HAL_GPIO_WritePin(ALARM_PORT, ALARM_PIN, GPIO_PIN_RESET);
        break;

    case ALARM_STATE_HIGH_HUMIDITY:
        alarm_pin_level = GPIO_PIN_SET;
        HAL_GPIO_WritePin(ALARM_PORT, ALARM_PIN, GPIO_PIN_SET);
        break;

    case ALARM_STATE_DHT_FAULT:
    default:
        /* Nhấp nháy chu kỳ 250 ms (2 Hz) */
        if ((now_ms - last_alarm_toggle_ms) >= 250u) {
            last_alarm_toggle_ms = now_ms;
            alarm_pin_level = (alarm_pin_level == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
            HAL_GPIO_WritePin(ALARM_PORT, ALARM_PIN, alarm_pin_level);
        }
        break;
    }
}

/*==================== Cấp dữ liệu cho UI ====================*/

static void Fill_UI_Data(UI_Data_t *out)
{
    uint8_t i;

    out->temperature_c = last_temp;
    out->humidity_pct = last_humidity;
    out->bluetooth_connected = bluetooth_connected;
    out->heartbeat_led_on =
        (HAL_GPIO_ReadPin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN) == HEARTBEAT_LED_ON_STATE);

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        out->output_on[i] = output_on[i];
    }

    out->sensor_valid = sensor_valid;
    out->dht_health_ok = Health_Monitor_IsDHTHealthy(&health_mon);
    out->sensor_age_s = sensor_valid ? ((HAL_GetTick() - last_sensor_ok_ms) / 1000u) : 0u;
    out->auth_owner = Auth_GetOwner();
    out->alarm_state = current_alarm_state;
}

/*==================== Handler của từng lệnh Bluetooth ====================*/

static void Command_LOGIN(char *return_msg, const char *args)
{
    bool                kicked = false;
    Auth_Login_Result_t res;

    if (args == NULL) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "LOGIN_FAIL\r\n");
        return;
    }

    res = Auth_Login(AUTH_BLE, args, &kicked);
    if (res == AUTH_LOGIN_OK) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "LOGIN_OK\r\n");
    } else if (res == AUTH_LOGIN_BUSY_LOCAL) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "LOGIN_BUSY_LOCAL\r\n");
    } else {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "LOGIN_FAIL\r\n");
    }
}

static void Command_LOGOUT(char *return_msg, const char *args)
{
    (void)args;
    if (Auth_Logout(AUTH_BLE)) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "LOGOUT_OK\r\n");
    } else {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "ERR_NOT_OWNER\r\n");
    }
}

static void Command_SetOutputs(char *return_msg, const char *args, bool on)
{
    const char *state_text = on ? "ON" : "OFF";
    uint8_t     channel;

    /* Quyền: chỉ chủ sở hữu AUTH_BLE mới được điều khiển GPIO */
    if (!Auth_IsOwner(AUTH_BLE)) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "ERR_LOCKED\r\n");
        return;
    }

    Auth_NotifyActivity(AUTH_BLE);

    if (args == NULL) {
        Set_Output(0u, on);
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "OUT1_%s\r\n", state_text);
        return;
    }

    if (strcmp(args, "ALL") == 0) {
        for (channel = 0u; channel < (uint8_t)OUT_COUNT; channel++) {
            Set_Output(channel, on);
        }
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "ALL_%s\r\n", state_text);
        return;
    }

    /* 4 kênh: '1'..'4' */
    if ((args[0] < '1') || (args[0] > '0' + (char)OUT_COUNT) || (args[1] != '\0')) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "BAD_CHANNEL\r\n");
        return;
    }

    channel = (uint8_t)(args[0] - '1');
    Set_Output(channel, on);
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "OUT%u_%s\r\n", (unsigned)(channel + 1u),
                   state_text);
}

static void Command_ON(char *return_msg, const char *args)
{
    Command_SetOutputs(return_msg, args, true);
}

static void Command_OFF(char *return_msg, const char *args)
{
    Command_SetOutputs(return_msg, args, false);
}

static void Command_STATUS(char *return_msg, const char *args)
{
    (void)args;
    if (Auth_IsOwner(AUTH_BLE)) {
        Auth_NotifyActivity(AUTH_BLE);
    }
    Format_Status(return_msg, COMMAND_RETURN_MSG_SIZE);
}

static void Command_TEMP(char *return_msg, const char *args)
{
    (void)args;
    if (Auth_IsOwner(AUTH_BLE)) {
        Auth_NotifyActivity(AUTH_BLE);
    }
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "TEMP=%uC\r\n", (unsigned)last_temp);
}

static void Command_HUM(char *return_msg, const char *args)
{
    (void)args;
    if (Auth_IsOwner(AUTH_BLE)) {
        Auth_NotifyActivity(AUTH_BLE);
    }
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "HUM=%u%%\r\n", (unsigned)last_humidity);
}

static void Command_AUTO(char *return_msg, const char *args)
{
    (void)args;
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "AUTO_MODE_READY\r\n");
}

/*==================== Khởi tạo clock hệ thống ====================*/

/*
 * SYSCLK = 72 MHz: HSE 8 MHz (thạch anh trên Blue Pill) -> PLL x9.
 *
 *   HCLK  = 72 MHz (AHB  /1)
 *   PCLK2 = 72 MHz (APB2 /1)
 *   PCLK1 = 36 MHz (APB1 /2) — TRẦN CỨNG của APB1, không được để /1. USART2
 *                              lấy clock từ đây; HAL tự tính BRR nên baud vẫn đúng
 *   TIM2  = 72 MHz (APB1 prescaler != 1 nên clock timer = 2 x PCLK1)
 *
 * FLASH_LATENCY_2 là bắt buộc cho dải 48-72 MHz (RM0008 §3.3.3). Đặt sai
 * latency thì CPU đọc nhầm lệnh từ flash và chết ngay khi chuyển clock.
 *
 * PR đổi cây clock phải rà lại mọi tính toán baud và prescaler của timer.
 *
 * Không static: HAL gọi lại hàm này sau khi thoát chế độ low-power.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init.HSEState = RCC_HSE_ON;
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init.PLL.PLLMUL = RCC_PLL_MUL9; /* 8 MHz x 9 = 72 MHz */

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        Error_Handler(); /* Thạch anh HSE không dao động được */
    }

    rcc_clk_init.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/*==================== Khởi tạo ngoại vi ====================*/

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /* GPIOA: UART BT (PA2/PA3), DHT11 (PA4), 3 nút (PA5..PA7), OUT-1 (PA8).
     * GPIOB: 2 nút (PB0/PB1), I2C2 (PB10/PB11), OUT-2..OUT-5 (PB12..PB15).
     * GPIOC: LED heartbeat onboard (PC13).
     * Thiếu clock của port nào thì các chân port đó không phản hồi gì cả. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Ghi mức TẮT trước khi init để LED active-low không chớp lúc boot */
    HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN, HEARTBEAT_LED_OFF_STATE);

    /* Cùng lý do với LED, nhưng hậu quả nặng hơn: các chân này lái relay và
     * thiết bị thật. Sau reset chúng là input floating, và ODR có thể còn giữ
     * mức của lần chạy trước — cấu hình thành output trước khi ghi mức sẽ bật
     * thiết bị trong vài chu kỳ. Ghi mức TẮT ngay bây giờ để lúc
     * Digital_Out_Init() chuyển chân sang output thì nó đã sẵn ở mức an toàn.
     *
     * OUT-1 nằm trên GPIOA, bốn kênh còn lại trên GPIOB nên phải hai lệnh ghi.
     * Cả hai giả định mọi kênh có OUTn_ON_STATE = GPIO_PIN_SET; nếu sau này gắn
     * tầng ngoài tác động mức THẤP thì kênh đó phải ghi riêng ở đây. */
    HAL_GPIO_WritePin(GPIOA, OUT_GPIOA_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, OUT_GPIOB_PINS, GPIO_PIN_RESET);

    /* PC13 — LED heartbeat onboard (active LOW) */
    gpio_init.Pin = HEARTBEAT_LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HEARTBEAT_LED_PORT, &gpio_init);

    /* PA8 — Chân cảnh báo tự động ALARM (active HIGH) */
    gpio_init.Pin = ALARM_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ALARM_PORT, &gpio_init);

    /* PA5..PA7 và PB0/PB1 — 5 nút nhấn. Pull-up nội, nút kéo xuống GND nên mức
     * nghỉ là CAO. Không cần điện trở ngoài.
     *
     * Bắt CẢ HAI cạnh chứ không chỉ cạnh xuống: việc chống dội phím trong ui.c
     * cần nhìn thấy cả lúc nhả tay để biết cú bấm đã kết thúc. Chỉ bắt cạnh
     * xuống thì tiếng nảy lúc nhả — cũng toàn là cạnh xuống — không thể phân
     * biệt được với một cú bấm mới. */
    gpio_init.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Pin = BTN_GPIOA_PINS;
    HAL_GPIO_Init(GPIOA, &gpio_init);
    gpio_init.Pin = BTN_GPIOB_PINS;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* Ưu tiên ngắt: EXTI của DHT11 PHẢI cao hơn UART (số nhỏ hơn = ưu tiên cao).
     * DHT11 lấy timestamp ngay trong ISR, các bit chỉ cách nhau 77-124 us.
     * Ưu tiên của USART2 đặt trong HAL_UART_MspInit(). */
    HAL_NVIC_SetPriority(DHT11_EXTI_IRQn, DHT11_EXTI_PRIO, 0);

    /* Nút nhấn: ưu tiên thấp nhất trong các ngắt của app. Bật ngay ở đây vì
     * không driver nào "sở hữu" chúng như DHT11 sở hữu PA4.
     * PA5/PA6/PA7 dùng chung EXTI9_5; PB0 -> EXTI0; PB1 -> EXTI1. */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(BTN4_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN4_EXTI_IRQn);
    HAL_NVIC_SetPriority(BTN5_EXTI_IRQn, BTN_EXTI_PRIO, 0);
    HAL_NVIC_EnableIRQ(BTN5_EXTI_IRQn);

    /* 4 chân ngõ ra (PB12..PB15) và PA4 (DHT11) cố tình KHÔNG cấu hình ở
     * đây: Digital_Out_Init() và DHT11_Init() tự lo, vì chân DHT11 phải đổi
     * qua lại giữa output OD và input EXTI lúc chạy. Chân PA8 (ALARM) đã được
     * cấu hình output push-pull ở trên. */
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = BT_UART_INSTANCE;
    huart2.Init.BaudRate = BT_UART_BAUDRATE; /* 9600 — mặc định của MKE-M15 */
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = I2C2_CLOCK_SPEED; /* 400 kHz Fast-mode */
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

/*
 * TIM2 làm đồng hồ micro-giây cho DHT11: bộ đếm PHẢI chạy đúng 1 MHz
 * (1 tick = 1 us) vì DHT11.c đo độ rộng xung trực tiếp bằng giá trị đếm.
 * Prescaler được tính từ clock thực tế thay vì ghi cứng, để đổi SYSCLK
 * không làm sai toàn bộ timing của cảm biến.
 */
static void MX_TIM2_Init(void)
{
    uint32_t timer_clock_hz;

    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Trên STM32F1, nếu APB1 prescaler khác /1 thì clock timer = 2 x PCLK1 */
    timer_clock_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        timer_clock_hz *= 2u;
    }

    if ((timer_clock_hz % TIM2_COUNTER_FREQ_HZ) != 0u) {
        Error_Handler(); /* Không chia ra được 1 MHz chính xác */
    }

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (timer_clock_hz / TIM2_COUNTER_FREQ_HZ) - 1u;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 65535u;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

/*==================== MSP — cấu hình mức thấp của ngoại vi ====================*/

/* HAL tự gọi các hàm này từ bên trong HAL_xxx_Init()/DeInit(). Nơi duy nhất
 * được phép bật clock ngoại vi, cấu hình chân AF và NVIC của ngoại vi đó. */

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (huart->Instance == BT_UART_INSTANCE) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART2_CLK_ENABLE();

        gpio_init.Pin = BT_UART_TX_PIN;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(BT_UART_TX_PORT, &gpio_init);

        /* PA3 = RX — PULL-UP cố ý: module chưa cấp nguồn / đứt dây thì chân
         * thả nổi sẽ nhặt nhiễu và báo framing error liên tục. */
        gpio_init.Pin = BT_UART_RX_PIN;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(BT_UART_RX_PORT, &gpio_init);

        /* PHẢI thấp hơn (số lớn hơn) DHT11_EXTI_PRIO — xem pin_config.h */
        HAL_NVIC_SetPriority(BT_UART_IRQn, BT_UART_PRIO, 0);
        HAL_NVIC_EnableIRQ(BT_UART_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        __HAL_RCC_USART2_CLK_DISABLE();
        HAL_GPIO_DeInit(BT_UART_TX_PORT, BT_UART_TX_PIN);
        HAL_GPIO_DeInit(BT_UART_RX_PORT, BT_UART_RX_PIN);
        HAL_NVIC_DisableIRQ(BT_UART_IRQn);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C2) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init.Pin = I2C2_SCL_PIN | I2C2_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(I2C2_SCL_PORT, &gpio_init);

        __HAL_RCC_I2C2_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_DISABLE();
        HAL_GPIO_DeInit(I2C2_SCL_PORT, I2C2_SCL_PIN | I2C2_SDA_PIN);
    }
}

/*==================== Callback ngắt của HAL ====================*/

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        if (Ring_Buffer_Write_SingleData(&ring_buffer_handler, &uart_rx_byte) == DEV_SUCCESS) {
            bluetooth_connected = true;
            developer_uart_handler.last_received_tick = HAL_GetTick();
            developer_uart_handler.uart_connecting_status = DEVELOPER_UART_CONNECTED;
        }

        /* Mở lại phiên nhận kể cả khi bộ đệm đầy: bỏ một byte còn hơn ngừng nhận. */
        (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1u);
    }
}

/**
 * @brief Khởi động lại việc nhận sau khi USART2 gặp lỗi.
 *
 * Khi bị tràn (ORE) — dễ xảy ra vì ISR của DHT11 có thể chiếm CPU lâu hơn một
 * khung 9600 baud — HAL báo lỗi và HUỶ luôn phiên Receive_IT. Không bắt lại ở
 * đây thì RXNE sẽ không bao giờ nổi nữa và Bluetooth "chết câm" sau lần tràn
 * đầu tiên, dù mọi thứ khác vẫn chạy bình thường.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == BT_UART_INSTANCE) {
        /* Đọc SR rồi DR là trình tự xoá cờ ORE trên F1; HAL_UART_IRQHandler đã
         * đọc SR, đọc nốt DR để chắc chắn cờ được xoá và bỏ byte hỏng đi. */
        (void)huart2.Instance->DR;

        (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1u);
    }
}

/**
 * @brief Điểm đến chung của mọi ngắt EXTI (DHT11 trên PA4, 5 nút PA5..PA7 + PB0/PB1).
 *
 * Thử nút trước vì UI_HandleButtonIrq() trả về ngay khi chân không phải của nó;
 * chân nào không ai nhận thì bỏ qua.
 */
void HAL_GPIO_EXTI_Callback(uint16_t exti_pin)
{
    if (UI_HandleButtonIrq(exti_pin)) {
        return;
    }

    DHT11_CallbackEXTI(exti_pin);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        DHT11_CallbackTIM2();
    }
}

/*==================== Bẫy lỗi ====================*/

/**
 * @brief Lỗi không hồi phục được: tắt ngắt và treo vĩnh viễn.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
