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
#include "uart.h"
#include "ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*==================== Hằng số cấu hình ====================*/

/* Chu kỳ các tác vụ nền (ms) */
#define SENSOR_PERIOD_MS       2000u   /* Đọc DHT11 */
#define STATUS_PERIOD_MS       3000u   /* Báo trạng thái về điện thoại */
#define HEARTBEAT_PERIOD_MS    1000u   /* Nhấp nháy LED PC13 báo còn sống */

/* Chờ DHT11 hoàn tất một phép đo: poll mỗi 5 ms, bỏ cuộc sau 500 ms */
#define DHT11_POLL_INTERVAL_MS   5u
#define DHT11_POLL_TIMEOUT_MS  500u

#define UART_RX_BUFFER_SIZE    128u
#define UART_FRAME_BUFFER_SIZE 128u

/* Đủ rộng cho chuỗi trạng thái dài nhất (~45 ký tự) kèm CRLF */
#define STATUS_TEXT_SIZE        64u

/* Tần số bộ đếm TIM2 — 1 MHz để mỗi tick bằng đúng 1 us */
#define TIM2_COUNTER_FREQ_HZ 1000000u

/*==================== Handle ngoại vi ====================*/

/* Không static: stm32f1xx_it.c đẩy ngắt vào huart2 và htim2 qua extern. */
UART_HandleTypeDef huart2;
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim2;

/*==================== Trạng thái hệ thống ====================*/

static uint8_t  last_temp;                 /* Nhiệt độ đọc lần cuối (độ C) */
static uint8_t  last_humidity;             /* Độ ẩm đọc lần cuối (%) */
static bool     output_on[OUT_COUNT];      /* true = kênh đang đóng */
static bool     bluetooth_connected;       /* true = đã nhận được byte từ module BT */
static bool     sensor_valid;              /* true = đã từng đọc DHT11 thành công */
static uint32_t last_sensor_ok_ms;         /* Mốc tick của lần đọc thành công cuối */

/* Cả 5 kênh dùng chung driver Digital_Out: nó generic theo {port, pin, state}
 * nên một con MOSFET, một opto hay một chân tín hiệu thường đều lái được.
 *
 * Thứ tự PHẢI khớp với ui_outputs[] trong src/ui.c — cùng đánh số 0..N-1. */
static Digital_Out_HandleTypeDef outputs[OUT_COUNT] = {
    { OUT1_PORT, OUT1_PIN, DIGITAL_OUT_OFF },
    { OUT2_PORT, OUT2_PIN, DIGITAL_OUT_OFF },
    { OUT3_PORT, OUT3_PIN, DIGITAL_OUT_OFF },
    { OUT4_PORT, OUT4_PIN, DIGITAL_OUT_OFF },
    { OUT5_PORT, OUT5_PIN, DIGITAL_OUT_OFF },
};

/* Mức logic ứng với trạng thái BẬT của từng kênh. Tách khỏi bảng handle vì
 * Digital_Out_SetState() luôn coi ON = mức CAO; kênh nào tác động mức THẤP
 * thì đảo tại đây (xem OUTn_ON_STATE trong pin_config.h). */
static const GPIO_PinState output_on_state[OUT_COUNT] = {
    OUT1_ON_STATE, OUT2_ON_STATE, OUT3_ON_STATE, OUT4_ON_STATE, OUT5_ON_STATE,
};

/*==================== Cụm nhận lệnh qua UART ====================*/

/* Buffer cấp phát tĩnh, kích thước bằng macro — không có malloc sau init. */
static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t uart_frame_buffer[UART_FRAME_BUFFER_SIZE];

/* Ô nhận một byte mà HAL_UART_Receive_IT() ghi vào. ISR đọc rồi đẩy ngay sang
 * bộ đệm vòng, nên không có ai khác chạm vào nó cùng lúc. */
static uint8_t uart_rx_byte;

static Ring_Buffer_HandleTypeDef    ring_buffer_handler;
static Developer_UART_HandleTypeDef developer_uart_handler;

/*==================== Cấu hình DHT11 ====================*/

static DHT11_Config_t dht11_cfg = { DHT11_PORT, DHT11_PIN, DHT11_EXTI_IRQn, &htim2 };

/*==================== Nguyên mẫu hàm ====================*/

/* Hai hàm này không static: SystemClock_Config() được HAL gọi lại sau khi
 * thoát low-power, còn Error_Handler() cũng được ui.c dùng. */
void SystemClock_Config(void);
void Error_Handler(void);

static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);

static void Set_Output(uint8_t channel, bool on);
static void Format_Status(char *out, size_t out_size);
static bool DHT11_ReadOnce(void);
static void Send_Status(void);
static void Fill_UI_Data(UI_Data_t *out);

static void Command_SetOutputs(char *return_msg, const char *args, bool on);
static void Command_ON(char *return_msg, const char *args);
static void Command_OFF(char *return_msg, const char *args);
static void Command_STATUS(char *return_msg, const char *args);
static void Command_TEMP(char *return_msg, const char *args);
static void Command_HUM(char *return_msg, const char *args);
static void Command_AUTO(char *return_msg, const char *args);

/*==================== Bảng lệnh Bluetooth ====================*/

/* UART_Task() tra cứu bảng này qua khai báo extern trong Command_Selector.h.
 * Cố tình KHÔNG static: đây là phần "nội dung" mà tầng lib mong đợi app cấp.
 *
 * Thêm một lệnh mới = thêm một handler ở dưới và một dòng ở đây. Nhớ cập nhật
 * trang HƯỚNG DẪN trong ui.c cho khớp. */
Command_HandleTypeDef Command_Menu[] = {
    { "ON",     Command_ON     },   /* Đóng một kênh, hoặc tất cả */
    { "OFF",    Command_OFF    },   /* Mở một kênh, hoặc tất cả */
    { "STATUS", Command_STATUS },   /* Nhiệt độ, độ ẩm, ngõ ra, kết nối */
    { "TEMP",   Command_TEMP   },   /* Chỉ nhiệt độ */
    { "HUM",    Command_HUM    },   /* Chỉ độ ẩm */
    { "AUTO",   Command_AUTO   }    /* Chế độ tự động — chưa triển khai */
};

uint8_t Command_Menu_Size = (uint8_t)(sizeof(Command_Menu) / sizeof(Command_Menu[0]));

/*==================== Vòng lặp chính ====================*/

int main(void)
{
    uint32_t     now_ms;
    uint32_t     last_sensor_tick_ms;
    uint32_t     last_status_tick_ms;
    uint32_t     last_heartbeat_tick_ms;
    bool         bluetooth_logged = false;
    uint8_t      i;
    UI_Data_t    ui_data;
    UI_Request_t ui_request;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();

    /* 5 ngõ ra. MX_GPIO_Init() đã ghi sẵn mức TẮT lên các chân này nên bước
     * chuyển sang output dưới đây không làm thiết bị nháy. */
    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        if (Digital_Out_Init(&outputs[i]) != DEV_SUCCESS) {
            Error_Handler();
        }
        Set_Output(i, false);
    }

    if (DHT11_Init(&dht11_cfg) != DEV_SUCCESS) {
        Error_Handler();
    }

    UI_Init(&hi2c1);

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

    UART_Print(&developer_uart_handler, "HC-05 ready\r\n");
    UI_Log("BOOT OK");

    last_sensor_tick_ms = HAL_GetTick();
    last_status_tick_ms = last_sensor_tick_ms;
    last_heartbeat_tick_ms = last_sensor_tick_ms;

    while (1) {
        now_ms = HAL_GetTick();

        /* Mọi mốc thời gian đều so bằng hiệu (now - last): HAL_GetTick() là
         * uint32_t và tràn sau ~49,7 ngày, phép trừ unsigned vẫn đúng khi tràn
         * còn phép cộng (now >= last + T) thì không. */
        if ((now_ms - last_sensor_tick_ms) >= SENSOR_PERIOD_MS) {
            UI_Log(DHT11_ReadOnce() ? "DHT OK" : "DHT FAIL");
            last_sensor_tick_ms = now_ms;
        }

        if ((now_ms - last_status_tick_ms) >= STATUS_PERIOD_MS) {
            Send_Status();
            last_status_tick_ms = now_ms;
        }

        /* Nhấp nháy PC13 mỗi giây: dấu hiệu nhìn-là-biết vòng lặp chính còn
         * chạy. Đứng yên = treo ở Error_Handler hoặc một ISR nào đó. */
        if ((now_ms - last_heartbeat_tick_ms) >= HEARTBEAT_PERIOD_MS) {
            HAL_GPIO_TogglePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN);
            last_heartbeat_tick_ms = now_ms;
        }

        /* Ghi nhật ký lần bắt tay đầu tiên với module BT ở đây chứ không ở
         * trong HAL_UART_RxCpltCallback(): UI_Log() dùng chung bộ đệm vòng với
         * UI_Task(), gọi từ ISR sẽ tranh chấp với lúc đang vẽ. */
        if (bluetooth_connected && !bluetooth_logged) {
            bluetooth_logged = true;
            UI_Log("BT LINK UP");
        }

        UART_Task(&developer_uart_handler);

        Fill_UI_Data(&ui_data);
        UI_Task(now_ms, &ui_data, &ui_request);

        /* Nút OK trên OLED chỉ *đề nghị* đổi ngõ ra; việc thi hành nằm ở đây.
         * Nhờ vậy ui.c không cần biết gì về phần còn lại của main.c, và mọi lối
         * vào của việc bật/tắt thiết bị — nút bấm lẫn lệnh Bluetooth — đều đi
         * qua đúng một hàm Set_Output(). */
        if (ui_request.toggle_output && (ui_request.channel < (uint8_t)OUT_COUNT)) {
            Set_Output(ui_request.channel, !output_on[ui_request.channel]);
        }
    }
}

/*==================== Điều khiển ngõ ra ====================*/

/**
 * @brief Đóng/mở một kênh ngõ ra và ghi nhật ký UI.
 *
 * Kênh 0 (relay PB12) kéo theo LED chỉ báo PB13 — LED là đèn báo của relay chứ
 * không phải một thiết bị độc lập.
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
    Digital_Out_SetState(&outputs[channel],
                         (output_on_state[channel] == GPIO_PIN_SET)
                             ? (on ? DIGITAL_OUT_ON : DIGITAL_OUT_OFF)
                             : (on ? DIGITAL_OUT_OFF : DIGITAL_OUT_ON));

    (void)snprintf(label, sizeof(label), "OUT%u %s",
                   (unsigned)(channel + 1u), on ? "ON" : "OFF");
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
    char    map[OUT_COUNT + 1u];
    uint8_t i;

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        map[i] = output_on[i] ? '1' : '0';
    }
    map[OUT_COUNT] = '\0';

    (void)snprintf(out, out_size, "TEMP=%uC HUM=%u%% OUT1=%s BT=%s OUT=%s\r\n",
                   (unsigned)last_temp,
                   (unsigned)last_humidity,
                   output_on[0] ? "ON" : "OFF",
                   bluetooth_connected ? "OK" : "NO",
                   map);
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

    DHT11_StartRequest();

    start_ms = HAL_GetTick();
    /* Lùi một chu kỳ để lần poll đầu tiên chạy ngay, không chờ 5 ms. */
    last_poll_ms = start_ms - DHT11_POLL_INTERVAL_MS;

    while ((HAL_GetTick() - start_ms) < DHT11_POLL_TIMEOUT_MS) {
        DHT11_State_t state;

        /* Giữ nhịp poll bằng hiệu tick thay vì HAL_Delay: không khoá CPU trong
         * SysTick handler và không phụ thuộc độ phân giải của nó. */
        if ((HAL_GetTick() - last_poll_ms) < DHT11_POLL_INTERVAL_MS) {
            continue;
        }
        last_poll_ms += DHT11_POLL_INTERVAL_MS;

        state = DHT11_ReadData(&dht_data);

        if (state == DHT11_STATE_COMPLETE) {
            if (!dht_data.is_valid) {
                return false;   /* Checksum sai */
            }
            /* temp_dec / humidity_dec bị bỏ qua có ý: DHT11 luôn trả về 0 ở
             * hai trường này. */
            last_temp = dht_data.temp_int;
            last_humidity = dht_data.humidity_int;

            /* Ghi lại mốc đọc được để trang SENSOR nói được "LAST OK 4s": số
             * đo cũ vẫn hiện trên màn hình sau khi cảm biến hỏng, không có mốc
             * này thì không cách nào phân biệt số tươi với số đã chết. */
            last_sensor_ok_ms = HAL_GetTick();
            sensor_valid = true;
            return true;
        }

        if (state == DHT11_STATE_ERROR) {
            return false;
        }
    }

    return false;   /* Quá hạn: cảm biến không hoàn tất phiên đo */
}

/*==================== Cấp dữ liệu cho UI ====================*/

/**
 * @brief Sao trạng thái sang dạng mà ui.c cần để vẽ một khung hình.
 *
 * UI giữ struct riêng để đổi bố cục màn hình không kéo theo sửa phần logic và
 * ngược lại.
 */
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

    /* UI chỉ cần "cách đây bao lâu", không cần mốc tuyệt đối. Quy đổi ở đây để
     * UI khỏi phải tự trừ tick và tự lo chuyện tràn uint32_t. */
    out->sensor_valid = sensor_valid;
    out->sensor_age_s = sensor_valid ? ((HAL_GetTick() - last_sensor_ok_ms) / 1000u) : 0u;
}

/*==================== Handler của từng lệnh ====================*/

/* ON và OFF dùng chung Command_SetOutputs(): từ khi biết tách số kênh và "ALL",
 * phần thân đã dài đủ để chép đôi thành nợ. TEMP/HUM thì vẫn để rời — mỗi hàm
 * chỉ có một dòng, gộp lại không lợi gì. */

/**
 * @brief Thân chung của ON và OFF — chỉ khác nhau đúng một tham số `on`.
 *
 * Cú pháp tham số:
 *      (không có)  kênh 1, đúng như hành vi cũ khi mới có mỗi một ngõ ra. Giữ
 *                  nguyên để các app điện thoại đã cấu hình sẵn không phải sửa.
 *      "ALL"       cả OUT_COUNT kênh
 *      "1".."5"    một kênh
 */
static void Command_SetOutputs(char *return_msg, const char *args, bool on)
{
    const char *state_text = on ? "ON" : "OFF";
    uint8_t     channel;

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

    /* Số kênh người dùng gõ là 1..N; bên trong đánh số từ 0. Chỉ nhận đúng một
     * chữ số rồi hết chuỗi — "1X" hay "12" đều là gõ nhầm chứ không phải kênh 1. */
    if ((args[0] < '1') || (args[0] > '0' + (char)OUT_COUNT) || (args[1] != '\0')) {
        (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "BAD_CHANNEL\r\n");
        return;
    }

    channel = (uint8_t)(args[0] - '1');
    Set_Output(channel, on);
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "OUT%u_%s\r\n",
                   (unsigned)(channel + 1u), state_text);
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
    Format_Status(return_msg, COMMAND_RETURN_MSG_SIZE);
}

static void Command_TEMP(char *return_msg, const char *args)
{
    (void)args;
    (void)snprintf(return_msg, COMMAND_RETURN_MSG_SIZE, "TEMP=%uC\r\n", (unsigned)last_temp);
}

static void Command_HUM(char *return_msg, const char *args)
{
    (void)args;
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
    rcc_osc_init.PLL.PLLMUL = RCC_PLL_MUL9;     /* 8 MHz x 9 = 72 MHz */

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK) {
        Error_Handler();    /* Thạch anh HSE không dao động được */
    }

    rcc_clk_init.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                             RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
     * GPIOB: 2 nút (PB0/PB1), I2C1 (PB6/PB7), OUT-2..OUT-5 (PB12..PB15).
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

    /* 5 chân ngõ ra (PA8/PB12..PB15) và PA4 (DHT11) cố tình KHÔNG cấu hình ở
     * đây: Digital_Out_Init() và DHT11_Init() tự lo, vì chân DHT11 phải đổi
     * qua lại giữa output OD và input EXTI lúc chạy. */
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = BT_UART_INSTANCE;
    huart2.Init.BaudRate = BT_UART_BAUDRATE;    /* 9600 — mặc định của MKE-M15 */
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

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = I2C1_CLOCK_SPEED;   /* 400 kHz Fast-mode */
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
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
        Error_Handler();    /* Không chia ra được 1 MHz chính xác */
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

    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init.Pin = I2C1_SCL_PIN | I2C1_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(I2C1_SCL_PORT, &gpio_init);

        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(I2C1_SCL_PORT, I2C1_SCL_PIN | I2C1_SDA_PIN);
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
