/*
 * ui.c — Vẽ 4 trang giao diện lên OLED SSD1306 và xử lý 2 nút điều hướng.
 *
 * Bố cục chung của cả 4 trang (màn 128x64, font 5x7 -> mỗi ký tự chiếm 6 px):
 *
 *      y=0   +--------------------------------+  thanh tiêu đề đảo màu:
 *            | GPIO STATUS               1/4  |  nền trắng, chữ đen
 *      y=11  +--------------------------------+
 *      y=16  | nội dung dòng 1                |
 *      y=31  | nội dung dòng 2                |
 *      y=46  | nội dung dòng 3                |
 *            +--------------------------------+  y=63
 */
#include "ui.h"

#include "board.h"      /* Error_Handler() */
#include "SSD1306.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*==================== Hằng số bố cục ====================*/

#define UI_HEADER_HEIGHT     11u   /* Chiều cao thanh tiêu đề */
#define UI_TEXT_PAD_X         2u   /* Lề trái của chữ */
#define UI_HEADER_TEXT_Y      2u   /* Chữ trong thanh tiêu đề */

/* Ba dòng nội dung cách đều nhau cho trang GPIO và SENSOR */
#define UI_ROW0_Y            16u
#define UI_ROW_SPACING       15u

/* Trang UART LOG và HƯỚNG DẪN xếp 4 dòng sát hơn để chứa được nhiều chữ hơn */
#define UI_DENSE_ROW0_Y      15u
#define UI_DENSE_ROW_SPACING 11u
#define UI_DENSE_ROWS         4u

/* Ô tick vuông bên phải mỗi dòng của trang GPIO */
#define UI_CHECKBOX_SIZE     10u
#define UI_CHECKBOX_X       112u
#define UI_CHECKBOX_INSET     2u   /* Độ dày viền khi ô được tô đặc */

/* Chu kỳ vẽ lại màn hình khi không ai bấm nút (ms) */
#define UI_REFRESH_PERIOD_MS 500u

#define UI_TEXT_LEN          24u   /* Đủ cho 21 ký tự vừa bề ngang màn hình */

/*==================== Trạng thái module ====================*/

typedef enum {
    UI_PAGE_GPIO = 0,
    UI_PAGE_SENSOR,
    UI_PAGE_LOG,
    UI_PAGE_HELP,
    UI_PAGE_COUNT
} UI_Page_t;

/* Nhật ký hiện trên trang UART LOG */
#define UI_LOG_LINES          UI_DENSE_ROWS
#define UI_LOG_TEXT_LEN      16u

typedef struct {
    uint32_t tick_ms;                 /* Mốc HAL_GetTick() lúc ghi */
    char     text[UI_LOG_TEXT_LEN];
} UI_LogEntry_t;

static ssd1306_t          ui_display;
static I2C_HandleTypeDef *ui_hi2c;

/* ISR ghi hai biến này, vòng lặp chính đọc chúng. Đều là 1 byte nên trên
 * Cortex-M3 mỗi lần đọc/ghi là một lệnh đơn, không thể bị cắt đôi. */
static volatile uint8_t ui_current_page;
static volatile uint8_t ui_redraw_pending;

static uint32_t ui_last_draw_ms;
static uint32_t ui_last_button_ms;   /* Chỉ ISR dùng — chống dội phím */

static UI_LogEntry_t ui_log[UI_LOG_LINES];
static uint8_t       ui_log_count;   /* Số dòng đang có, tối đa UI_LOG_LINES */
static uint8_t       ui_log_head;    /* Vị trí sẽ ghi đè tiếp theo */

/*==================== Nguyên mẫu hàm nội bộ ====================*/

static void UI_DrawHeader(const char *title);
static void UI_DrawCheckbox(uint16_t y, bool checked);
static void UI_DrawGpioPage(const UI_Data_t *data);
static void UI_DrawSensorPage(const UI_Data_t *data);
static void UI_DrawLogPage(void);
static void UI_DrawHelpPage(void);
static void UI_Render(const UI_Data_t *data);

/*==================== API công khai ====================*/

void UI_Init(I2C_HandleTypeDef *hi2c)
{
    ui_hi2c = hi2c;

    if (SSD1306_Init(hi2c) != HAL_OK) {
        Error_Handler();
    }

    ui_display.width = SSD1306_WIDTH;
    ui_display.height = SSD1306_HEIGHT;

    ui_current_page = (uint8_t)UI_PAGE_GPIO;
    ui_redraw_pending = 1u;

    SSD1306_Clear(&ui_display);
    UI_DrawHeader("BOOTING");
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_ROW0_Y,
                        "STM32 BT NODE", SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_ROW0_Y + UI_ROW_SPACING,
                        "PA0 NEXT PA1 PREV", SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen(hi2c, &ui_display);
}

void UI_Task(uint32_t now_ms, const UI_Data_t *data)
{
    uint8_t due;

    due = (uint8_t)((now_ms - ui_last_draw_ms) >= UI_REFRESH_PERIOD_MS);

    if (ui_redraw_pending == 0u && due == 0u) {
        return;
    }

    /* Xoá cờ TRƯỚC khi vẽ: nếu người dùng bấm nút ngay giữa lúc đang đẩy dữ
     * liệu qua I2C, cờ được đặt lại và vòng lặp sau sẽ vẽ thêm một khung nữa.
     * Xoá sau khi vẽ thì cú bấm đó bị nuốt mất. */
    ui_redraw_pending = 0u;

    UI_Render(data);
    ui_last_draw_ms = now_ms;
}

bool UI_HandleButtonIrq(uint16_t exti_pin)
{
    uint32_t now_ms;
    uint8_t  page;

    if ((exti_pin != BTN_NEXT_PIN) && (exti_pin != BTN_PREV_PIN)) {
        return false;
    }

    /* Tiếp điểm cơ khí nảy từng chục lần mỗi lần bấm. Chỉ nhận cạnh đầu tiên
     * rồi làm ngơ phần còn lại trong BTN_DEBOUNCE_MS. */
    now_ms = HAL_GetTick();
    if ((now_ms - ui_last_button_ms) < BTN_DEBOUNCE_MS) {
        return true;
    }
    ui_last_button_ms = now_ms;

    page = ui_current_page;
    if (exti_pin == BTN_NEXT_PIN) {
        page = (uint8_t)((page + 1u) % (uint8_t)UI_PAGE_COUNT);
    } else {
        page = (uint8_t)((page + (uint8_t)UI_PAGE_COUNT - 1u) % (uint8_t)UI_PAGE_COUNT);
    }
    ui_current_page = page;

    /* Chỉ báo "phải vẽ lại"; việc vẽ thật sự do UI_Task() làm ở vòng lặp chính
     * vì SSD1306_UpdateScreen() blocking khoảng 25 ms trên I2C. */
    ui_redraw_pending = 1u;
    return true;
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

/*==================== Vẽ khung hình ====================*/

static void UI_Render(const UI_Data_t *data)
{
    SSD1306_Clear(&ui_display);

    switch ((UI_Page_t)ui_current_page) {
    case UI_PAGE_GPIO:
        UI_DrawGpioPage(data);
        break;

    case UI_PAGE_SENSOR:
        UI_DrawSensorPage(data);
        break;

    case UI_PAGE_LOG:
        UI_DrawLogPage();
        break;

    /* HELP gộp chung với default là CỐ Ý: ui_current_page khai kiểu uint8_t
     * (ISR ghi nó, 1 byte mới là thao tác nguyên tử trên Cortex-M3) nên về
     * nguyên tắc nó có thể mang giá trị ngoài dải enum. Trang HELP là nơi rơi
     * vào an toàn nhất vì nó không đọc UI_Data_t. */
    case UI_PAGE_HELP:
    case UI_PAGE_COUNT:
    default:
        UI_DrawHelpPage();
        break;
    }

    SSD1306_UpdateScreen(ui_hi2c, &ui_display);
}

/**
 * @brief Thanh tiêu đề đảo màu kèm chỉ số trang ở góc phải.
 *
 * Tô đặc cả dải bằng màu trắng rồi viết chữ màu ĐEN đè lên: SSD1306_WriteChar()
 * chỉ chạm vào đúng các pixel của glyph, nên chữ sẽ "khoét" ra khỏi nền trắng.
 */
static void UI_DrawHeader(const char *title)
{
    char page_text[8];
    uint16_t page_text_x;

    SSD1306_FillRect(&ui_display, 0u, 0u, SSD1306_WIDTH, UI_HEADER_HEIGHT,
                     SSD1306_COLOR_WHITE);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_HEADER_TEXT_Y, title,
                        SSD1306_COLOR_BLACK);

    snprintf(page_text, sizeof(page_text), "%u/%u",
             (unsigned)(ui_current_page + 1u), (unsigned)UI_PAGE_COUNT);

    /* Căn lề phải: mỗi ký tự chiếm SSD1306_CHAR_ADVANCE px, ký tự cuối không
     * cần cột ngăn cách nên trừ bớt 1. */
    page_text_x = (uint16_t)(SSD1306_WIDTH - UI_TEXT_PAD_X -
                             (strlen(page_text) * SSD1306_CHAR_ADVANCE - 1u));
    SSD1306_WriteString(&ui_display, page_text_x, UI_HEADER_TEXT_Y, page_text,
                        SSD1306_COLOR_BLACK);
}

/**
 * @brief Ô vuông trạng thái bên phải một dòng: rỗng = TẮT, tô đặc = BẬT.
 */
static void UI_DrawCheckbox(uint16_t y, bool checked)
{
    SSD1306_DrawRect(&ui_display, UI_CHECKBOX_X, y, UI_CHECKBOX_SIZE,
                     UI_CHECKBOX_SIZE, SSD1306_COLOR_WHITE);

    if (checked) {
        SSD1306_FillRect(&ui_display,
                         (uint16_t)(UI_CHECKBOX_X + UI_CHECKBOX_INSET),
                         (uint16_t)(y + UI_CHECKBOX_INSET),
                         (uint16_t)(UI_CHECKBOX_SIZE - 2u * UI_CHECKBOX_INSET),
                         (uint16_t)(UI_CHECKBOX_SIZE - 2u * UI_CHECKBOX_INSET),
                         SSD1306_COLOR_WHITE);
    }
}

/*==================== Trang 1: GPIO STATUS ====================*/

static void UI_DrawGpioPage(const UI_Data_t *data)
{
    /* Bảng mô tả các đầu ra: nhãn hiển thị + trạng thái tương ứng lấy từ
     * UI_Data_t. Thêm một đầu ra mới = thêm một dòng ở đây. */
    const struct {
        const char *label;
        bool        state;
    } rows[] = {
        { "PB12 RELAY", data->relay_on },
        { "PA8  LED",   data->status_led_on },
        { "PC13 BEAT",  data->heartbeat_led_on },
    };

    uint8_t i;

    UI_DrawHeader("GPIO STATUS");

    for (i = 0u; i < (uint8_t)(sizeof(rows) / sizeof(rows[0])); i++) {
        uint16_t row_y = (uint16_t)(UI_ROW0_Y + i * UI_ROW_SPACING);

        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, row_y, rows[i].label,
                            SSD1306_COLOR_WHITE);

        /* Ô vuông cao 10 px, chữ cao 7 px — lùi lên 2 px để hai thứ căn giữa
         * nhau theo chiều dọc. */
        UI_DrawCheckbox((uint16_t)(row_y - 2u), rows[i].state);
    }
}

/*==================== Trang 2: DHT11 SENSOR ====================*/

static void UI_DrawSensorPage(const UI_Data_t *data)
{
    char line[UI_TEXT_LEN];

    UI_DrawHeader("DHT11 SENSOR");

    /* Trang này cố tình KHÔNG dùng App_State_FormatStatus(): màn hình là một
     * phương tiện khác — mỗi trường một dòng, nhãn riêng, có ký hiệu độ — ép
     * chung một hàm định dạng sẽ làm cả hai bên khó đọc hơn. Giá trị thì vẫn
     * chỉ có một nguồn duy nhất là system_state.
     *
     * Không in phần thập phân: DHT11 có độ phân giải 1 độ C / 1 %, byte thập
     * phân của nó luôn bằng 0 nên ".0" chỉ là số trang trí giả.
     *
     * %c với SSD1306_DEGREE_CHAR: ký hiệu độ nằm ở ô 0x7F của bảng font,
     * ngoài dải ASCII nên không viết thẳng trong chuỗi được. */
    snprintf(line, sizeof(line), "TEMP:%u%cC",
             (unsigned)data->temperature_c, SSD1306_DEGREE_CHAR);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_ROW0_Y, line,
                        SSD1306_COLOR_WHITE);

    snprintf(line, sizeof(line), "HUMI:%u%%", (unsigned)data->humidity_pct);
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                        UI_ROW0_Y + UI_ROW_SPACING, line, SSD1306_COLOR_WHITE);

    snprintf(line, sizeof(line), "BT:%s",
             data->bluetooth_connected ? "PAIRED" : "NO LINK");
    SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                        UI_ROW0_Y + 2u * UI_ROW_SPACING, line,
                        SSD1306_COLOR_WHITE);
}

/*==================== Trang 3: UART LOG ====================*/

static void UI_DrawLogPage(void)
{
    uint8_t i;

    UI_DrawHeader("UART LOG");

    if (ui_log_count == 0u) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X, UI_DENSE_ROW0_Y,
                            "(EMPTY)", SSD1306_COLOR_WHITE);
        return;
    }

    for (i = 0u; i < ui_log_count; i++) {
        /* Đọc vòng tròn từ dòng cũ nhất tới dòng mới nhất. Khi bộ đệm chưa
         * đầy, dòng cũ nhất nằm ở chỉ số 0; khi đã đầy, nó nằm ngay sau head. */
        uint8_t slot = (ui_log_count < UI_LOG_LINES)
                           ? i
                           : (uint8_t)((ui_log_head + i) % UI_LOG_LINES);
        const UI_LogEntry_t *entry = &ui_log[slot];
        uint32_t seconds = entry->tick_ms / 1000u;
        char line[UI_TEXT_LEN];

        snprintf(line, sizeof(line), "%02u:%02u %s",
                 (unsigned)((seconds / 60u) % 100u), (unsigned)(seconds % 60u),
                 entry->text);
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                            (uint16_t)(UI_DENSE_ROW0_Y + i * UI_DENSE_ROW_SPACING),
                            line, SSD1306_COLOR_WHITE);
    }
}

/*==================== Trang 4: HƯỚNG DẪN ====================*/

/**
 * @brief Nhắc nhanh tập lệnh Bluetooth và cách dùng hai nút.
 *
 * Nội dung tĩnh nên không nhận UI_Data_t. Đặt cuối vòng xoay trang để người
 * dùng bấm NEXT thêm một cái từ trang LOG là thấy, hoặc bấm PREV một cái từ
 * trang GPIO là tới ngay.
 *
 * Danh sách lệnh ở đây phải khớp với Command_Menu[] trong src/app_command.c.
 */
static void UI_DrawHelpPage(void)
{
    /* Mỗi dòng tối đa 21 ký tự thì vừa bề ngang màn hình (21 x 6 = 126 px).
     * Dài hơn thì SSD1306_WriteString() tự xuống dòng và đè lên dòng kế tiếp. */
    static const char *const help_lines[UI_DENSE_ROWS] = {
        "CMD:ON OFF STATUS",
        "    TEMP HUM AUTO",
        "SEND VIA BT 9600 8N1",
        "PA0 NEXT  PA1 PREV",
    };

    uint8_t i;

    UI_DrawHeader("HUONG DAN");

    for (i = 0u; i < UI_DENSE_ROWS; i++) {
        SSD1306_WriteString(&ui_display, UI_TEXT_PAD_X,
                            (uint16_t)(UI_DENSE_ROW0_Y + i * UI_DENSE_ROW_SPACING),
                            help_lines[i], SSD1306_COLOR_WHITE);
    }
}
