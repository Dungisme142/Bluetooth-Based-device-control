/*
 * ui.c — Vẽ 5 trang giao diện lên OLED SSD1306 và xử lý 5 nút điều khiển.
 *
 * Khung chung của mọi trang (màn 128x64, font 5x7 -> mỗi ký tự chiếm 6 px,
 * một dòng chứa tối đa 21 ký tự):
 *
 *      y=0   +--------------------------------+  thanh tiêu đề đảo màu:
 *            | HOME                   BT 1/5  |  nền trắng, chữ đen
 *      y=11  +--------------------------------+
 *            | phần thân, mỗi trang tự lo      |
 *            +--------------------------------+  y=63
 *
 * Luồng xử lý nút: ISR -> hàng đợi sự kiện -> UI_Task() ở vòng lặp chính.
 * ISR không vẽ (I2C blocking ~25 ms) và không đổi trạng thái thiết bị (việc
 * đó cần ghi nhật ký, mà nhật ký lại là bộ đệm vòng UI_Task() đang đọc).
 */
#include "ui.h"

#include "SSD1306.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Định nghĩa trong main.c — khai ở đây để ui.c không phải kéo theo header nào. */
void Error_Handler(void);

/*==================== Hằng số bố cục ====================*/

#define UI_HEADER_HEIGHT     11u   /* Chiều cao thanh tiêu đề */
#define UI_TEXT_PAD_X         2u   /* Lề trái của chữ */
#define UI_HEADER_TEXT_Y      2u   /* Chữ trong thanh tiêu đề */
#define UI_RIGHT_EDGE_X     126u   /* Mép phải dùng khi căn lề phải */

/* Danh sách 5 dòng (trang OUTPUTS, LOG, HƯỚNG DẪN) */
#define UI_LIST_ROW0_Y       13u
#define UI_LIST_ROW_SPACING  10u
#define UI_LIST_ROWS          5u

#define UI_TEXT_LEN          24u   /* Đủ cho 21 ký tự vừa bề ngang màn hình */

/* Ô trạng thái của từng kênh trên trang HOME (4 kênh) */
#define UI_CELL_SIZE         10u
#define UI_CELL_FIRST_X      24u
#define UI_CELL_PITCH_X      24u
#define UI_CELL_Y            43u
#define UI_CELL_INSET         2u   /* Viền chừa lại khi ô được tô đặc */
#define UI_CELL_FOCUS_PAD     2u   /* Khoảng cách từ ô tới khung chọn */

/* Chu kỳ vẽ lại màn hình khi không ai bấm nút (ms) */
#define UI_REFRESH_PERIOD_MS 500u

/* Thang đo của thanh mức nhiệt độ. Độ ẩm dùng thẳng phần trăm nên không cần. */
#define UI_TEMP_SCALE_MAX_C  50u

/*==================== Trạng thái module ====================*/

typedef enum {
    UI_VIEW_LOCKED_DASHBOARD = 0,
    UI_VIEW_KEYPAD,
    UI_VIEW_STANDARD_PAGES
} UI_ViewMode_t;

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_OUTPUTS,
    UI_PAGE_SENSOR,
    UI_PAGE_LOG,
    UI_PAGE_HELP,
    UI_PAGE_COUNT
} UI_Page_t;

/* Sự kiện nút, do ISR đẩy vào hàng đợi */
typedef enum {
    UI_EVENT_NEXT = 0,
    UI_EVENT_PREV,
    UI_EVENT_UP,
    UI_EVENT_DOWN,
    UI_EVENT_OK
} UI_Event_t;

/* Số nút vật lý — bằng số mục của button_map[] trong UI_HandleButtonIrq() */
#define UI_BUTTON_COUNT       5u

/* Hàng đợi sự kiện: một người ghi (ISR) và một người đọc (vòng lặp chính), mỗi
 * bên chỉ sửa chỉ số của riêng mình và cả hai chỉ số đều là 1 byte — trên
 * Cortex-M3 đọc/ghi 1 byte là thao tác đơn, không cần khoá ngắt.
 * 8 chỗ là quá thừa: người dùng bấm nhanh nhất cũng chỉ vài lần mỗi giây, còn
 * vòng lặp chính rút hàng đợi ít nhất 2 lần mỗi giây. */
#define UI_EVENT_QUEUE_SIZE   8u

/* Nhật ký hiện trên trang LOG */
#define UI_LOG_LINES         12u
#define UI_LOG_VISIBLE       UI_LIST_ROWS
#define UI_LOG_TEXT_LEN      16u

typedef struct {
    uint32_t tick_ms;                 /* Mốc HAL_GetTick() lúc ghi */
    char     text[UI_LOG_TEXT_LEN];
} UI_LogEntry_t;

/* Bảng nhãn của 4 kênh ngõ ra PB15..PB12 */
static const struct {
    const char *name;
    const char *pin_name;
} ui_outputs[OUT_COUNT] = {
    { OUT1_NAME, OUT1_PIN_NAME },
    { OUT2_NAME, OUT2_PIN_NAME },
    { OUT3_NAME, OUT3_PIN_NAME },
    { OUT4_NAME, OUT4_PIN_NAME },
};

static ssd1306_t          ui_display;
static I2C_HandleTypeDef *ui_hi2c;

/* Quản lý chế độ xem (Locked Dashboard vs Keypad vs 5 trang chuẩn) */
static UI_ViewMode_t ui_view_mode = UI_VIEW_LOCKED_DASHBOARD;

/* Bàn phím số ảo 3x4: 4 hàng x 3 cột */
static uint8_t  ui_keypad_row = 0u;
static uint8_t  ui_keypad_col = 0u;
static char     ui_keypad_pin[5] = {0};
static uint8_t  ui_keypad_pin_len = 0u;
static bool     ui_keypad_wrong_pin = false;
static uint32_t ui_keypad_wrong_pin_until_ms = 0u;
static uint32_t ui_keypad_last_action_ms = 0u;

/* Cờ trạng thái sức khỏe cảm biến DHT */
static bool     ui_dht_health_ok = true;

/* ISR ghi các biến này, vòng lặp chính đọc chúng. Đều là 1 byte nên trên
 * Cortex-M3 mỗi lần đọc/ghi là một lệnh đơn, không thể bị cắt đôi. */
static volatile uint8_t ui_event_queue[UI_EVENT_QUEUE_SIZE];
static volatile uint8_t ui_event_head;   /* ISR ghi vào đây */
static volatile uint8_t ui_event_tail;   /* Vòng lặp chính đọc từ đây */

static uint8_t  ui_current_page;
static uint8_t  ui_selected_output;  /* Kênh đang được con trỏ trỏ tới */

/* Tình trạng Bluetooth của khung hình đang vẽ */
static bool     ui_bluetooth_connected;
static uint8_t  ui_redraw_pending;
static uint32_t ui_last_draw_ms;

/* Bảng nút: chân, cổng và sự kiện tương ứng. Cần cả cổng vì việc chống dội
 * phím phải ĐỌC LẠI mức của chân chứ không thể chỉ tin vào cạnh ngắt. */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    UI_Event_t    event;
} ui_buttons[UI_BUTTON_COUNT] = {
    { BTN_NEXT_PORT, BTN_NEXT_PIN, UI_EVENT_NEXT },
    { BTN_PREV_PORT, BTN_PREV_PIN, UI_EVENT_PREV },
    { BTN_UP_PORT,   BTN_UP_PIN,   UI_EVENT_UP },
    { BTN_DOWN_PORT, BTN_DOWN_PIN, UI_EVENT_DOWN },
    { BTN_OK_PORT,   BTN_OK_PIN,   UI_EVENT_OK },
};

/* Mức đã chốt của từng nút: true = đang bị nhấn giữ. Sự kiện chỉ sinh ra ở lần
 * chuyển NHẢ -> NHẤN, nên tiếng nảy lúc nhả tay không thể hoá thành cú bấm mới. */
static volatile uint8_t ui_button_pressed[UI_BUTTON_COUNT];

/* Mốc chống dội phím RIÊNG cho từng nút. Một mốc dùng chung sẽ nuốt mất thao
 * tác hai nút liên tiếp — chọn kênh bằng UP rồi bấm OK ngay sau đó. */
static volatile uint32_t ui_last_button_ms[UI_BUTTON_COUNT];

static UI_LogEntry_t ui_log[UI_LOG_LINES];
static uint8_t       ui_log_count;   /* Số dòng đang có, tối đa UI_LOG_LINES */
static uint8_t       ui_log_head;    /* Vị trí sẽ ghi đè tiếp theo */
static uint8_t       ui_log_scroll;  /* Số dòng đã cuộn lên khỏi đáy */

/*==================== Nguyên mẫu hàm nội bộ ====================*/

static void UI_QueueEvent(UI_Event_t event);
static void UI_SampleButton(uint8_t index, uint32_t now_ms);
static void UI_ReleaseStaleButtons(uint32_t now_ms);
static void UI_HandleEvent(UI_Event_t event, UI_Request_t *req, uint32_t now_ms);
static void UI_DrawHeader(const char *title);
static void UI_DrawTextRight(uint16_t right_x, uint16_t y, const char *text,
                             SSD1306_COLOR color);
static void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               uint8_t percent);
static void UI_DrawCell(uint16_t x, uint16_t y, bool filled, bool focused);
static void UI_FormatUptime(char *out, size_t out_size, uint32_t now_ms);
static void UI_DrawLockedPage(const UI_Data_t *data, uint32_t now_ms);
static void UI_DrawKeypadPage(uint32_t now_ms);
static void UI_DrawHomePage(const UI_Data_t *data, uint32_t now_ms);
static void UI_DrawOutputsPage(const UI_Data_t *data);
static void UI_DrawSensorPage(const UI_Data_t *data);
static void UI_DrawLogPage(void);
static void UI_DrawHelpPage(void);
static void UI_Render(const UI_Data_t *data, uint32_t now_ms);

/*==================== API công khai ====================*/

void UI_Init(I2C_HandleTypeDef *hi2c)
{
    ui_hi2c = hi2c;

    if (SSD1306_Init(hi2c) != HAL_OK) {
        Error_Handler();
    }

    ui_display.width = SSD1306_WIDTH;
    ui_display.height = SSD1306_HEIGHT;

    ui_view_mode = UI_VIEW_LOCKED_DASHBOARD;
    ui_current_page = (uint8_t)UI_PAGE_HOME;
    ui_selected_output = 0u;
    ui_keypad_row = 0u;
    ui_keypad_col = 0u;
    ui_keypad_pin_len = 0u;
    ui_keypad_pin[0] = '\0';
    ui_keypad_wrong_pin = false;
    ui_redraw_pending = 1u;

    SSD1306_Clear(&ui_display);
    UI_DrawHeader("BOOTING");
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 18u,
                        "STM32 BT NODE", SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 32u,
                        "4 OUTPUTS  5 BUTTONS", SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 46u,
                        "PA6/PB1 = DOI TRANG", SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(hi2c, &ui_display);
}

void UI_Task(uint32_t now_ms, const UI_Data_t *data, UI_Request_t *req)
{
    uint8_t due;

    req->toggle_output = false;
    req->channel = 0u;
    req->local_login = false;
    req->user_activity = false;

    UI_ReleaseStaleButtons(now_ms);

    /* Đồng bộ chế độ hiển thị với quyền sở hữu phiên */
    if ((data->auth_owner != AUTH_LOCAL) && (ui_view_mode == UI_VIEW_STANDARD_PAGES)) {
        ui_view_mode = UI_VIEW_LOCKED_DASHBOARD;
        ui_keypad_pin_len = 0u;
        ui_keypad_pin[0] = '\0';
        ui_keypad_wrong_pin = false;
        ui_redraw_pending = 1u;
    } else if ((data->auth_owner == AUTH_LOCAL) && (ui_view_mode != UI_VIEW_STANDARD_PAGES)) {
        ui_view_mode = UI_VIEW_STANDARD_PAGES;
        ui_redraw_pending = 1u;
    }

    /* Kiểm tra timeout 30s của bàn phím ảo */
    if (ui_view_mode == UI_VIEW_KEYPAD) {
        if ((now_ms - ui_keypad_last_action_ms) >= AUTH_KEYPAD_TIMEOUT_MS) {
            ui_view_mode = UI_VIEW_LOCKED_DASHBOARD;
            ui_keypad_pin_len = 0u;
            ui_keypad_pin[0] = '\0';
            ui_keypad_wrong_pin = false;
            ui_redraw_pending = 1u;
        } else if (ui_keypad_wrong_pin && (now_ms >= ui_keypad_wrong_pin_until_ms)) {
            ui_keypad_wrong_pin = false;
            ui_redraw_pending = 1u;
        }
    }

    /* Rút hàng đợi cho tới khi hết, hoặc cho tới khi đã có một đề nghị đổi ngõ ra / đăng nhập */
    while ((ui_event_tail != ui_event_head) && !req->toggle_output && !req->local_login) {
        UI_Event_t event = (UI_Event_t)ui_event_queue[ui_event_tail];

        ui_event_tail = (uint8_t)((ui_event_tail + 1u) % UI_EVENT_QUEUE_SIZE);
        UI_HandleEvent(event, req, now_ms);
    }

    due = (uint8_t)((now_ms - ui_last_draw_ms) >= UI_REFRESH_PERIOD_MS);

    if (ui_redraw_pending == 0u && due == 0u) {
        return;
    }

    ui_redraw_pending = 0u;

    UI_Render(data, now_ms);
    ui_last_draw_ms = now_ms;
}

bool UI_HandleButtonIrq(uint16_t exti_pin)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)UI_BUTTON_COUNT; i++) {
        if (ui_buttons[i].pin == exti_pin) {
            UI_SampleButton(i, HAL_GetTick());
            return true;
        }
    }

    return false;
}

void UI_Log(const char *text)
{
    UI_LogEntry_t *entry = &ui_log[ui_log_head];

    entry->tick_ms = HAL_GetTick();
    strncpy(entry->text, text, UI_LOG_TEXT_LEN - 1u);
    entry->text[UI_LOG_TEXT_LEN - 1u] = '\0';

    ui_log_head = (uint8_t)((ui_log_head + 1u) % UI_LOG_LINES);
    if (ui_log_count < UI_LOG_LINES) {
        ui_log_count++;
    }

    if (ui_current_page == (uint8_t)UI_PAGE_LOG) {
        ui_redraw_pending = 1u;
    }
}

/*==================== Chống dội phím ====================*/

/**
 * @brief Đưa một sự kiện vào hàng đợi cho vòng lặp chính.
 *
 * Hàng đợi đầy thì bỏ sự kiện này. Thà mất một cú bấm còn hơn ghi đè lên sự
 * kiện chưa xử lý và làm lệch chỉ số của bên đọc.
 */
static void UI_QueueEvent(UI_Event_t event)
{
    uint8_t next_head = (uint8_t)((ui_event_head + 1u) % UI_EVENT_QUEUE_SIZE);

    if (next_head != ui_event_tail) {
        ui_event_queue[ui_event_head] = (uint8_t)event;
        ui_event_head = next_head;
    }
}

/**
 * @brief Lấy mẫu một nút và sinh sự kiện nếu nó vừa CHUYỂN từ nhả sang nhấn.
 *
 * Vì sao không chỉ đếm cạnh xuống: tiếp điểm cơ khí nảy cả lúc nhấn LẪN lúc
 * nhả. Cách khoá-một-khoảng-sau-cạnh-đầu chỉ dập được tiếng nảy lúc nhấn; giữ
 * nút lâu hơn khoảng khoá rồi nhả ra thì tiếng nảy lúc nhả lại sinh đúng loại
 * cạnh đó và bị tính thành cú bấm thứ hai.
 *
 * Ở đây mỗi nút được coi là một máy trạng thái hai mức:
 *   - Trong BTN_DEBOUNCE_MS kể từ lần chuyển mức gần nhất, mọi cạnh bị bỏ qua
 *     (đó chính là lúc tiếp điểm đang nảy).
 *   - Hết khoảng đó thì ĐỌC LẠI mức thật của chân. Cạnh chỉ là lời mời đi kiểm
 *     tra, mức của chân mới là sự thật.
 *   - Chỉ lần chuyển NHẢ -> NHẤN mới sinh sự kiện. Nhả tay chỉ mở khoá cho cú
 *     bấm sau.
 *
 * Gọi được từ cả ISR lẫn vòng lặp chính (xem UI_ReleaseStaleButtons).
 */
static void UI_SampleButton(uint8_t index, uint32_t now_ms)
{
    uint8_t pressed;

    if ((now_ms - ui_last_button_ms[index]) < BTN_DEBOUNCE_MS) {
        return;
    }

    /* Nút nối xuống GND với pull-up nội: mức THẤP nghĩa là đang bị nhấn. */
    pressed = (uint8_t)(HAL_GPIO_ReadPin(ui_buttons[index].port,
                                         ui_buttons[index].pin) == GPIO_PIN_RESET);

    if (pressed == ui_button_pressed[index]) {
        return;     /* Cạnh giả hoặc mức đã trở về chỗ cũ — không có gì đổi */
    }

    ui_last_button_ms[index] = now_ms;
    ui_button_pressed[index] = pressed;

    if (pressed != 0u) {
        UI_QueueEvent(ui_buttons[index].event);
    }
}

/**
 * @brief Gỡ trạng thái "đang nhấn" bị kẹt của các nút đã nhả từ lâu.
 *
 * Cạnh lúc nhả tay có thể rơi đúng vào lúc chân đang nảy và bị đọc nhầm thành
 * vẫn-đang-nhấn; nếu tiếng nảy tắt ngay sau đó thì không còn cạnh nào tới nữa
 * và nút kẹt ở trạng thái nhấn vĩnh viễn — nghĩa là mất luôn cú bấm kế tiếp.
 * Vòng lặp chính đọc lại mức thật để tự gỡ.
 *
 * Hàm này CỐ Ý chỉ đi theo chiều nhả, không bao giờ sinh sự kiện: nếu cả ISR
 * lẫn vòng lặp chính cùng sinh được sự kiện thì hai bên có thể chen nhau và
 * đếm một cú bấm thành hai.
 */
static void UI_ReleaseStaleButtons(uint32_t now_ms)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)UI_BUTTON_COUNT; i++) {
        if (ui_button_pressed[i] == 0u) {
            continue;
        }
        if ((now_ms - ui_last_button_ms[i]) < BTN_DEBOUNCE_MS) {
            continue;
        }
        if (HAL_GPIO_ReadPin(ui_buttons[i].port, ui_buttons[i].pin) == GPIO_PIN_RESET) {
            continue;   /* Vẫn đang giữ thật */
        }

        ui_button_pressed[i] = 0u;
    }
}

/*==================== Xử lý nút ====================*/

/*==================== Xử lý nút ====================*/

/**
 * @brief Áp một sự kiện nút vào trạng thái UI.
 *
 * - Khi ở LOCKED DASHBOARD: bấm bất kỳ nút nào sẽ mở Bàn phím ảo 3x4 (cú bấm đầu không chọn ô nào).
 * - Khi ở KEYPAD: NEXT/PREV đổi cột (0..2), UP/DOWN đổi hàng (0..3), OK kích hoạt ô đang trỏ.
 * - Khi ở 5 TRANG TIÊU CHUẨN: chuyển trang, chọn kênh, cuộn nhật ký, bật/tắt kênh (chỉ khi AUTH_LOCAL).
 */
static void UI_HandleEvent(UI_Event_t event, UI_Request_t *req, uint32_t now_ms)
{
    ui_redraw_pending = 1u;

    if (ui_view_mode == UI_VIEW_LOCKED_DASHBOARD) {
        /* Cú bấm đầu tiên khi đang ở màn hình khóa chỉ có tác dụng mở bàn phím ảo */
        ui_view_mode = UI_VIEW_KEYPAD;
        ui_keypad_row = 0u;
        ui_keypad_col = 0u;
        ui_keypad_pin_len = 0u;
        ui_keypad_pin[0] = '\0';
        ui_keypad_wrong_pin = false;
        ui_keypad_last_action_ms = now_ms;
        req->user_activity = true;
        return;
    }

    if (ui_view_mode == UI_VIEW_KEYPAD) {
        req->user_activity = true;
        ui_keypad_last_action_ms = now_ms;
        if (ui_keypad_wrong_pin) {
            ui_keypad_wrong_pin = false;
        }

        switch (event) {
        case UI_EVENT_NEXT:
            ui_keypad_col = (uint8_t)((ui_keypad_col + 1u) % 3u);
            break;

        case UI_EVENT_PREV:
            ui_keypad_col = (uint8_t)((ui_keypad_col + 3u - 1u) % 3u);
            break;

        case UI_EVENT_UP:
            ui_keypad_row = (uint8_t)((ui_keypad_row + 4u - 1u) % 4u);
            break;

        case UI_EVENT_DOWN:
            ui_keypad_row = (uint8_t)((ui_keypad_row + 1u) % 4u);
            break;

        case UI_EVENT_OK:
        default:
            if (ui_keypad_row < 3u) {
                /* Hàng 0..2: Chữ số 1..9 */
                char digit = (char)('1' + ui_keypad_row * 3u + ui_keypad_col);
                if (ui_keypad_pin_len < 4u) {
                    ui_keypad_pin[ui_keypad_pin_len++] = digit;
                    ui_keypad_pin[ui_keypad_pin_len] = '\0';
                }
            } else {
                /* Hàng 3: col 0 = '<', col 1 = '0', col 2 = 'OK' */
                if (ui_keypad_col == 0u) {
                    /* Backspace: xóa lùi 1 ký tự */
                    if (ui_keypad_pin_len > 0u) {
                        ui_keypad_pin[--ui_keypad_pin_len] = '\0';
                    }
                } else if (ui_keypad_col == 1u) {
                    /* Số 0 */
                    if (ui_keypad_pin_len < 4u) {
                        ui_keypad_pin[ui_keypad_pin_len++] = '0';
                        ui_keypad_pin[ui_keypad_pin_len] = '\0';
                    }
                } else {
                    /* Ô OK: Xác nhận đăng nhập */
                    if (ui_keypad_pin_len == 4u) {
                        if (Auth_VerifyPIN(ui_keypad_pin)) {
                            req->local_login = true;
                            ui_keypad_pin_len = 0u;
                            ui_keypad_pin[0] = '\0';
                            ui_view_mode = UI_VIEW_STANDARD_PAGES;
                            ui_current_page = (uint8_t)UI_PAGE_HOME;
                        } else {
                            ui_keypad_wrong_pin = true;
                            ui_keypad_wrong_pin_until_ms = now_ms + 1500u;
                            ui_keypad_pin_len = 0u;
                            ui_keypad_pin[0] = '\0';
                        }
                    }
                }
            }
            break;
        }
        return;
    }

    /* Đang ở 5 trang tiêu chuẩn sau khi đăng nhập Local thành công */
    req->user_activity = true;

    switch (event) {
    case UI_EVENT_NEXT:
        ui_current_page = (uint8_t)((ui_current_page + 1u) % (uint8_t)UI_PAGE_COUNT);
        break;

    case UI_EVENT_PREV:
        ui_current_page = (uint8_t)((ui_current_page + (uint8_t)UI_PAGE_COUNT - 1u) %
                                    (uint8_t)UI_PAGE_COUNT);
        break;

    case UI_EVENT_UP:
        if (ui_current_page == (uint8_t)UI_PAGE_LOG) {
            /* Cuộn ngược về quá khứ, dừng lại khi dòng cũ nhất đã lên đỉnh. */
            if ((ui_log_count > UI_LOG_VISIBLE) &&
                (ui_log_scroll < (uint8_t)(ui_log_count - UI_LOG_VISIBLE))) {
                ui_log_scroll++;
            }
        } else if (ui_selected_output > 0u) {
            ui_selected_output--;
        } else {
            ui_selected_output = (uint8_t)(OUT_COUNT - 1u);
        }
        break;

    case UI_EVENT_DOWN:
        if (ui_current_page == (uint8_t)UI_PAGE_LOG) {
            if (ui_log_scroll > 0u) {
                ui_log_scroll--;
            }
        } else {
            ui_selected_output = (uint8_t)((ui_selected_output + 1u) % OUT_COUNT);
        }
        break;

    case UI_EVENT_OK:
    default:
        /* Trang LOG không có gì để bật/tắt; ở đó OK nhảy về bám đáy nhật ký. */
        if (ui_current_page == (uint8_t)UI_PAGE_LOG) {
            ui_log_scroll = 0u;
        } else {
            req->toggle_output = true;
            req->channel = ui_selected_output;
        }
        break;
    }
}

/*==================== Vẽ khung hình ====================*/

static void UI_Render(const UI_Data_t *data, uint32_t now_ms)
{
    SSD1306_Clear(&ui_display);

    ui_bluetooth_connected = data->bluetooth_connected;
    ui_dht_health_ok = data->dht_health_ok;

    if (ui_view_mode == UI_VIEW_LOCKED_DASHBOARD) {
        UI_DrawLockedPage(data, now_ms);
    } else if (ui_view_mode == UI_VIEW_KEYPAD) {
        UI_DrawKeypadPage(now_ms);
    } else {
        switch ((UI_Page_t)ui_current_page) {
        case UI_PAGE_HOME:
            UI_DrawHomePage(data, now_ms);
            break;

        case UI_PAGE_OUTPUTS:
            UI_DrawOutputsPage(data);
            break;

        case UI_PAGE_SENSOR:
            UI_DrawSensorPage(data);
            break;

        case UI_PAGE_LOG:
            UI_DrawLogPage();
            break;

        case UI_PAGE_HELP:
        case UI_PAGE_COUNT:
        default:
            UI_DrawHelpPage();
            break;
        }
    }

    SSD1306_UpdateScreen(ui_hi2c, &ui_display);
}

/**
 * @brief Thanh tiêu đề đảo màu: tên trang bên trái, "!DHT" ở giữa nếu lỗi, "BT" bên phải.
 */
static void UI_DrawHeader(const char *title)
{
    char right_text[12];

    SSD1306_FillRect(&ui_display, 0u, 0u, SSD1306_WIDTH, UI_HEADER_HEIGHT,
                     SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_HEADER_TEXT_Y, title,
                        SSD1306_COLOR_BLACK);

    /* Cảnh báo sức khỏe cảm biến trên tiêu đề mọi trang */
    if (!ui_dht_health_ok) {
        SSD1306_WriteString(&ui_display, 52u, UI_HEADER_TEXT_Y, "!DHT",
                            SSD1306_COLOR_BLACK);
    }

    /* "BT" chỉ hiện khi đã có liên lạc — sự vắng mặt của nó chính là dấu hiệu
     * mất kết nối, không cần thêm chữ "NO LINK" chiếm chỗ. */
    snprintf(right_text, sizeof(right_text), "%s%u/%u",
             ui_bluetooth_connected ? "BT " : "",
             (unsigned)(ui_current_page + 1u), (unsigned)UI_PAGE_COUNT);

    UI_DrawTextRight(UI_RIGHT_EDGE_X, UI_HEADER_TEXT_Y, right_text,
                     SSD1306_COLOR_BLACK);
}

/*==================== Màn hình Khóa & Bàn phím số ảo ====================*/

static void UI_DrawLockedPage(const UI_Data_t *data, uint32_t now_ms)
{
    char        line[UI_TEXT_LEN];
    const char *owner_str = Auth_OwnerToString(data->auth_owner);
    const char *alarm_str = "NORMAL";

    if (data->alarm_state == ALARM_STATE_HIGH_HUMIDITY) {
        alarm_str = "HIGH";
    } else if (data->alarm_state == ALARM_STATE_DHT_FAULT) {
        alarm_str = "FAULT";
    }

    /* Thanh tiêu đề đảo màu */
    SSD1306_FillRect(&ui_display, 0u, 0u, SSD1306_WIDTH, UI_HEADER_HEIGHT,
                     SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_HEADER_TEXT_Y, "LOCKED",
                        SSD1306_COLOR_BLACK);
    if (!data->dht_health_ok) {
        SSD1306_WriteString(&ui_display, 48u, UI_HEADER_TEXT_Y, "!DHT",
                            SSD1306_COLOR_BLACK);
    }
    snprintf(line, sizeof(line), "AUTH:%s", owner_str);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, UI_HEADER_TEXT_Y, line, SSD1306_COLOR_BLACK);

    /* Hàng 1: Nhiệt độ & Độ ẩm */
    snprintf(line, sizeof(line), "T:%u%cC  H:%u%%",
             (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR,
             (unsigned)data->humidity_pct);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 13u, line, SSD1306_COLOR_WHITE);

    /* Hàng 2: Sức khỏe DHT & Kết nối Bluetooth */
    snprintf(line, sizeof(line), "DHT:%s  BLE:%s",
             data->dht_health_ok ? "OK" : "BAD",
             data->bluetooth_connected ? "LINK" : "NO");
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 23u, line, SSD1306_COLOR_WHITE);

    /* Hàng 3: Trạng thái cảnh báo PA8 */
    snprintf(line, sizeof(line), "ALARM: %s", alarm_str);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 33u, line, SSD1306_COLOR_WHITE);

    /* Hàng 4: 4 ngõ ra OUT1..OUT4 & Chủ phiên */
    snprintf(line, sizeof(line), "OUT:%d%d%d%d  OWN:%s",
             data->output_on[0] ? 1 : 0,
             data->output_on[1] ? 1 : 0,
             data->output_on[2] ? 1 : 0,
             data->output_on[3] ? 1 : 0,
             owner_str);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 43u, line, SSD1306_COLOR_WHITE);

    /* Hàng 5: Hướng dẫn bấm phím (nhấp nháy chu kỳ 500 ms) */
    if (((now_ms / 500u) % 2u) == 0u) {
        SSD1306_WriteString(&ui_display, 8u, 54u, ">> PRESS ANY KEY <<", SSD1306_COLOR_WHITE);
    }
}

static void UI_DrawKeypadPage(uint32_t now_ms)
{
    static const char *const key_labels[4][3] = {
        { "1", "2", "3" },
        { "4", "5", "6" },
        { "7", "8", "9" },
        { "<", "0", "OK" }
    };
    uint8_t r, c;

    /* Thanh tiêu đề hiển thị mã PIN */
    SSD1306_FillRect(&ui_display, 0u, 0u, SSD1306_WIDTH, UI_HEADER_HEIGHT,
                     SSD1306_COLOR_WHITE);

    if (ui_keypad_wrong_pin && (now_ms < ui_keypad_wrong_pin_until_ms)) {
        SSD1306_WriteString(&ui_display, 36u, UI_HEADER_TEXT_Y, "WRONG PIN",
                            SSD1306_COLOR_BLACK);
    } else {
        char    pin_str[16];
        char    stars[5] = {0};
        uint8_t s;

        for (s = 0u; s < ui_keypad_pin_len; s++) {
            stars[s] = '*';
        }
        snprintf(pin_str, sizeof(pin_str), "PIN: %-4s", stars);
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_HEADER_TEXT_Y, pin_str,
                            SSD1306_COLOR_BLACK);
        UI_DrawTextRight(UI_RIGHT_EDGE_X, UI_HEADER_TEXT_Y, "ENTER PIN",
                         SSD1306_COLOR_BLACK);
    }

    /* Vẽ ma trận 4 hàng x 3 cột */
    for (r = 0u; r < 4u; r++) {
        uint16_t row_y = (uint16_t)(14u + r * 12u);

        for (c = 0u; c < 3u; c++) {
            uint16_t    col_x = (uint16_t)(12u + c * 37u);
            uint16_t    btn_w = 30u;
            uint16_t    btn_h = 10u;
            bool        focused = (r == ui_keypad_row && c == ui_keypad_col);
            const char *label = key_labels[r][c];
            uint16_t    text_x;

            if (focused) {
                SSD1306_FillRect(&ui_display, col_x, row_y, btn_w, btn_h,
                                 SSD1306_COLOR_WHITE);
            } else {
                SSD1306_DrawRect(&ui_display, col_x, row_y, btn_w, btn_h,
                                 SSD1306_COLOR_WHITE);
            }

            text_x = (uint16_t)(col_x + ((strlen(label) == 1u) ? 12u : 9u));
            SSD1306_WriteString(&ui_display, text_x, (uint16_t)(row_y + 2u), label,
                                focused ? SSD1306_COLOR_BLACK : SSD1306_COLOR_WHITE);
        }
    }
}

/**
 * @brief Viết chuỗi sao cho ký tự cuối kết thúc tại right_x.
 *
 * Mỗi ký tự chiếm SSD1306_CHAR_ADVANCE px, ký tự cuối không cần cột ngăn cách
 * nên trừ bớt 1.
 */
static void UI_DrawTextRight(uint16_t right_x, uint16_t y, const char *text,
                             SSD1306_COLOR color)
{
    uint16_t width = (uint16_t)(strlen(text) * SSD1306_CHAR_ADVANCE - 1u);

    if (width > right_x) {
        return;     /* Không đủ chỗ: thà bỏ qua còn hơn vẽ tràn sang trái */
    }

    SSD1306_WriteString(&ui_display, (uint16_t)(right_x - width), y, text, color);
}

/**
 * @brief Thanh mức: khung viền 1 px, bên trong tô đặc theo tỉ lệ percent (0..100).
 */
static void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               uint8_t percent)
{
    uint16_t inner_w;
    uint16_t fill_w;

    SSD1306_DrawRect(&ui_display, x, y, w, h, SSD1306_COLOR_WHITE);

    if (percent > 100u) {
        percent = 100u;     /* Cảm biến hỏng cũng không được vẽ tràn khung */
    }

    /* Chừa 1 px viền mỗi bên, thêm 1 px khe cho phần tô không dính vào viền. */
    inner_w = (uint16_t)(w - 4u);
    fill_w = (uint16_t)((inner_w * percent) / 100u);

    if (fill_w > 0u) {
        SSD1306_FillRect(&ui_display, (uint16_t)(x + 2u), (uint16_t)(y + 2u),
                         fill_w, (uint16_t)(h - 4u), SSD1306_COLOR_WHITE);
    }
}

/**
 * @brief Ô trạng thái một kênh: rỗng = TẮT, tô đặc = BẬT, có khung bao = đang chọn.
 */
static void UI_DrawCell(uint16_t x, uint16_t y, bool filled, bool focused)
{
    SSD1306_DrawRect(&ui_display, x, y, UI_CELL_SIZE, UI_CELL_SIZE,
                     SSD1306_COLOR_WHITE);

    if (filled) {
        SSD1306_FillRect(&ui_display, (uint16_t)(x + UI_CELL_INSET),
                         (uint16_t)(y + UI_CELL_INSET),
                         (uint16_t)(UI_CELL_SIZE - 2u * UI_CELL_INSET),
                         (uint16_t)(UI_CELL_SIZE - 2u * UI_CELL_INSET),
                         SSD1306_COLOR_WHITE);
    }

    if (focused) {
        SSD1306_DrawRect(&ui_display, (uint16_t)(x - UI_CELL_FOCUS_PAD),
                         (uint16_t)(y - UI_CELL_FOCUS_PAD),
                         (uint16_t)(UI_CELL_SIZE + 2u * UI_CELL_FOCUS_PAD),
                         (uint16_t)(UI_CELL_SIZE + 2u * UI_CELL_FOCUS_PAD),
                         SSD1306_COLOR_WHITE);
    }
}

/**
 * @brief Định dạng uptime thành "hh:mm:ss" (giờ lấy dư 100 để không tràn ô).
 */
static void UI_FormatUptime(char *out, size_t out_size, uint32_t now_ms)
{
    uint32_t seconds = now_ms / 1000u;

    snprintf(out, out_size, "%02u:%02u:%02u",
             (unsigned)((seconds / 3600u) % 100u),
             (unsigned)((seconds / 60u) % 60u),
             (unsigned)(seconds % 60u));
}

/*==================== Trang 1: HOME ====================*/

static void UI_DrawHomePage(const UI_Data_t *data, uint32_t now_ms)
{
    char     line[UI_TEXT_LEN];
    uint8_t  i;
    uint16_t temp_pct;

    UI_DrawHeader("HOME");

    /* Hai cột số đo. %c với SSD1306_DEGREE_CHAR: ký hiệu độ nằm ở ô 0x7F của
     * bảng font, ngoài dải ASCII nên không viết thẳng trong chuỗi được. */
    snprintf(line, sizeof(line), "TEMP %u%cC",
             (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 12u, line, SSD1306_COLOR_WHITE);

    snprintf(line, sizeof(line), "HUMI %u%%", (unsigned)data->humidity_pct);
    SSD1306_WriteString(&ui_display, 70u, 12u, line, SSD1306_COLOR_WHITE);

    /* Nhiệt độ quy về phần trăm của thang 0..UI_TEMP_SCALE_MAX_C độ C. */
    temp_pct = (uint16_t)((data->temperature_c * 100u) / UI_TEMP_SCALE_MAX_C);
    UI_DrawProgressBar(UI_TEXT_PAD_X, 21u, 56u, 8u, (uint8_t)((temp_pct > 100u) ? 100u : temp_pct));
    UI_DrawProgressBar(70u, 21u, 56u, 8u, data->humidity_pct);

    /* Đường kẻ tách phần cảm biến với phần ngõ ra */
    SSD1306_FillRect(&ui_display, 0u, 32u, SSD1306_WIDTH, 1u, SSD1306_COLOR_WHITE);

    /* Hàng nhãn kết thúc ở y=40, ngay trên khung chọn bắt đầu ở y=41 — nhích
     * xuống nữa là chữ số dính vào khung. */
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 34u, "OUT", SSD1306_COLOR_WHITE);

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        uint16_t cell_x = (uint16_t)(UI_CELL_FIRST_X + i * UI_CELL_PITCH_X);
        char     number[2];

        /* Số kênh đặt ngay trên ô, lùi 2 px để glyph 5 px nằm giữa ô 10 px. */
        number[0] = (char)('1' + i);
        number[1] = '\0';
        SSD1306_WriteString(&ui_display, (uint16_t)(cell_x + 2u), 34u, number,
                            SSD1306_COLOR_WHITE);

        UI_DrawCell(cell_x, UI_CELL_Y, data->output_on[i],
                    (i == ui_selected_output));
    }

    UI_FormatUptime(line, sizeof(line), now_ms);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 56u, line, SSD1306_COLOR_WHITE);
    if (!data->dht_health_ok) {
        SSD1306_FillRect(&ui_display, 80u, 55u, 46u, 9u, SSD1306_COLOR_WHITE);
        SSD1306_WriteString(&ui_display, 82u, 56u, "DHT BAD", SSD1306_COLOR_BLACK);
    } else {
        UI_DrawTextRight(UI_RIGHT_EDGE_X, 56u, data->sensor_valid ? "DHT OK" : "DHT --",
                         SSD1306_COLOR_WHITE);
    }
}

/*==================== Trang 2: OUTPUTS ====================*/

static void UI_DrawOutputsPage(const UI_Data_t *data)
{
    uint8_t i;

    UI_DrawHeader("OUTPUTS");

    for (i = 0u; i < (uint8_t)OUT_COUNT; i++) {
        uint16_t      row_y = (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING);
        bool          selected = (i == ui_selected_output);
        SSD1306_COLOR text_color = selected ? SSD1306_COLOR_BLACK : SSD1306_COLOR_WHITE;
        char          line[UI_TEXT_LEN];

        /* Dòng đang chọn: tô trắng cả dải rồi ghi chữ đen đè lên — cùng kỹ
         * thuật với thanh tiêu đề, nhìn rõ hơn nhiều so với một dấu ">". */
        if (selected) {
            SSD1306_FillRect(&ui_display, 0u, (uint16_t)(row_y - 1u),
                             SSD1306_WIDTH, 9u, SSD1306_COLOR_WHITE);
        }

        /* %-5s và %-4s giữ các cột thẳng hàng dù tên kênh dài ngắn khác nhau;
         * %3s để chữ ON/OFF cùng kết thúc ở một cột. */
        snprintf(line, sizeof(line), "%u %-5s %-4s  %3s",
                 (unsigned)(i + 1u), ui_outputs[i].name, ui_outputs[i].pin_name,
                 data->output_on[i] ? "ON" : "OFF");
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, row_y, line, text_color);
    }
}

/*==================== Trang 3: DHT11 SENSOR ====================*/

static void UI_DrawSensorPage(const UI_Data_t *data)
{
    char     line[UI_TEXT_LEN];
    uint16_t temp_pct;

    UI_DrawHeader("DHT11 SENSOR");

    /* Trang này cố tình KHÔNG dùng Format_Status(): màn hình là một
     * phương tiện khác — mỗi trường một dòng, có thanh mức, có ký hiệu độ — ép
     * chung một hàm định dạng sẽ làm cả hai bên khó đọc hơn. Giá trị thì vẫn
     * chỉ có một nguồn duy nhất là system_state.
     *
     * Không in phần thập phân: DHT11 có độ phân giải 1 độ C / 1 %, byte thập
     * phân của nó luôn bằng 0 nên ".0" chỉ là số trang trí giả. */
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 13u, "TEMP", SSD1306_COLOR_WHITE);
    snprintf(line, sizeof(line), "%u%cC",
             (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, 13u, line, SSD1306_COLOR_WHITE);

    temp_pct = (uint16_t)((data->temperature_c * 100u) / UI_TEMP_SCALE_MAX_C);
    UI_DrawProgressBar(UI_TEXT_PAD_X, 22u, 124u, 8u,
                       (uint8_t)((temp_pct > 100u) ? 100u : temp_pct));

    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 34u, "HUMI", SSD1306_COLOR_WHITE);
    snprintf(line, sizeof(line), "%u%%", (unsigned)data->humidity_pct);
    UI_DrawTextRight(UI_RIGHT_EDGE_X, 34u, line, SSD1306_COLOR_WHITE);

    UI_DrawProgressBar(UI_TEXT_PAD_X, 43u, 124u, 8u, data->humidity_pct);

    if (!data->dht_health_ok) {
        snprintf(line, sizeof(line), "DHT: BAD (%lus)", (unsigned long)data->sensor_age_s);
    } else if (data->sensor_valid) {
        snprintf(line, sizeof(line), "LAST OK %lus", (unsigned long)data->sensor_age_s);
    } else {
        snprintf(line, sizeof(line), "NO DATA");
    }
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, 55u, line, SSD1306_COLOR_WHITE);

    UI_DrawTextRight(UI_RIGHT_EDGE_X, 55u,
                     data->bluetooth_connected ? "BT PAIR" : "BT ----",
                     SSD1306_COLOR_WHITE);
}

/*==================== Trang 4: UART LOG ====================*/

static void UI_DrawLogPage(void)
{
    char    header_range[12];   /* "12-12/12" là chuỗi dài nhất có thể */
    uint8_t visible;
    uint8_t first;      /* Chỉ số (tính từ dòng cũ nhất) của dòng trên cùng */
    uint8_t i;

    UI_DrawHeader("LOG");

    if (ui_log_count == 0u) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_LIST_ROW0_Y,
                            "(EMPTY)", SSD1306_COLOR_WHITE);
        return;
    }

    visible = (ui_log_count < UI_LOG_VISIBLE) ? ui_log_count : (uint8_t)UI_LOG_VISIBLE;

    /* Mặc định bám đáy (ui_log_scroll = 0 -> dòng mới nhất nằm ở cuối màn hình).
     * Bấm UP đẩy cửa sổ lùi về quá khứ; UI_HandleEvent() đã chặn không cho lùi
     * quá dòng cũ nhất nên phép trừ dưới đây không thể âm. */
    first = (uint8_t)(ui_log_count - visible - ui_log_scroll);

    /* Cho biết đang xem đoạn nào của nhật ký — không có nó thì lúc cuộn giữa
     * chừng, màn hình trông y hệt lúc đang bám đáy. */
    snprintf(header_range, sizeof(header_range), "%u-%u/%u",
             (unsigned)(first + 1u), (unsigned)(first + visible),
             (unsigned)ui_log_count);
    SSD1306_WriteString(&ui_display, 34u, UI_HEADER_TEXT_Y, header_range,
                        SSD1306_COLOR_BLACK);

    for (i = 0u; i < visible; i++) {
        /* Đọc vòng tròn từ dòng cũ nhất tới dòng mới nhất. Khi bộ đệm chưa
         * đầy, dòng cũ nhất nằm ở chỉ số 0; khi đã đầy, nó nằm ngay sau head. */
        uint8_t index = (uint8_t)(first + i);
        uint8_t slot = (ui_log_count < UI_LOG_LINES)
                           ? index
                           : (uint8_t)((ui_log_head + index) % UI_LOG_LINES);
        const UI_LogEntry_t *entry = &ui_log[slot];
        uint32_t seconds = entry->tick_ms / 1000u;
        char     line[UI_TEXT_LEN];

        snprintf(line, sizeof(line), "%02u:%02u %s",
                 (unsigned)((seconds / 60u) % 100u), (unsigned)(seconds % 60u),
                 entry->text);
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                            (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING),
                            line, SSD1306_COLOR_WHITE);
    }
}

/*==================== Trang 5: HƯỚNG DẪN ====================*/

/**
 * @brief Nhắc nhanh tập lệnh Bluetooth và vai trò 5 nút.
 *
 * Nội dung tĩnh nên không cần UI_Data_t. Đặt cuối vòng xoay trang để bấm PREV
 * một cái từ trang HOME là tới ngay.
 *
 * Danh sách lệnh ở đây phải khớp với Command_Menu[] trong src/main.c.
 */
static void UI_DrawHelpPage(void)
{
    /* Mỗi dòng tối đa 21 ký tự thì vừa bề ngang màn hình (21 x 6 = 126 px).
     * Dài hơn thì SSD1306_WriteString() tự xuống dòng và đè lên dòng kế tiếp. */
    static const char *const help_lines[UI_LIST_ROWS] = {
        "BTN: NEXT PREV UP DN",
        "     OK = BAT/TAT",
        "CMD: LOGIN 1234/LOGOUT",
        "     ON 1-4 / OFF 1-4",
        "     STATUS TEMP HUM",
    };

    uint8_t i;

    UI_DrawHeader("HUONG DAN");

    for (i = 0u; i < (uint8_t)UI_LIST_ROWS; i++) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                            (uint16_t)(UI_LIST_ROW0_Y + i * UI_LIST_ROW_SPACING),
                            help_lines[i], SSD1306_COLOR_WHITE);
    }
}
