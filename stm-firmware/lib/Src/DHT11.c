/*
 * DHT11.c — Driver DHT11 cho STM32F103.
 *
 * Toàn bộ việc bắt nhịp nằm trong ngắt: EXTI đo độ rộng xung giữa hai sườn
 * xuống, TIM2 vừa là đồng hồ micro-giây vừa là watchdog. Vòng lặp chính chỉ
 * gọi DHT11_StartRequest() rồi hỏi DHT11_ReadData() cho tới khi FSM báo xong.
 */

#include "DHT11.h"

/* Ngưỡng phân loại độ rộng xung (micro-giây), đo từ sườn xuống trước đó.
 * Theo datasheet: bit 0 ~ 76 us, bit 1 ~ 120 us, nhịp mồi ~ 160 us. */
#define DHT11_PULSE_BIT0_MIN_US   60u
#define DHT11_PULSE_BIT0_MAX_US   100u
#define DHT11_PULSE_BIT1_MAX_US   140u
#define DHT11_PULSE_ACK_MAX_US    180u

/* Watchdog cho giai đoạn chờ phản hồi và giai đoạn đọc bit (micro-giây) */
#define DHT11_WATCHDOG_US         500u

#define DHT11_RAW_BYTES           5u
#define DHT11_BITS_PER_BYTE       8u

/* Cấu hình chân + timer, ghi đúng một lần trong DHT11_Init() rồi chỉ đọc.
 * Vì write-once trước khi ngắt đầu tiên xảy ra nên không cần volatile: không
 * có luồng nào ghi vào nó trong lúc ISR đang chạy. */
static DHT11_Config_t dht_cfg;

/* ISR ghi, vòng lặp chính đọc. Enum trên Cortex-M3 nằm gọn trong một từ nên
 * mỗi lần đọc/ghi là một lệnh đơn — volatile là đủ, không cần critical section. */
static volatile DHT11_State_t dht_current_state = DHT11_STATE_IDLE;

/* 40 bit thô do ISR EXTI dựng dần. Vòng lặp chính chỉ đọc mảng này khi FSM đã
 * sang COMPLETE — lúc đó ISR rơi vào nhánh default và không còn chạm vào nữa,
 * nên không cần khoá ngắt để đọc cả cụm 5 byte. */
static volatile uint8_t dht_raw_data[DHT11_RAW_BYTES];
static volatile uint8_t dht_bit_index;
static volatile uint8_t dht_byte_index;

static void DHT11_SetInput(void);
static void DHT11_SetOutput(void);
static void DHT11_ParseData(DHT11_Data_t *data);
static void DHT11_ClearRaw(void);

/**
 * @brief Khởi tạo các biến, trạng thái ban đầu cho DHT11 và bật bộ đếm.
 */
Developer_Action_Result_t DHT11_Init(const DHT11_Config_t *dht)
{
    if ((dht == NULL) || (dht->Port == NULL) || (dht->htim == NULL)) {
        return DEV_FAIL;
    }

    /* Lưu cấu hình do người dùng cung cấp (pin_config.h). Không có bước này
     * thì dht_cfg còn nguyên số 0 -> cấu hình nhầm Pin 0 / Port NULL. */
    dht_cfg = *dht;

    DHT11_ClearRaw();
    dht_current_state = DHT11_STATE_IDLE;

    HAL_TIM_Base_Start(dht_cfg.htim);

    /* Chân giao tiếp để ở Input làm mặc định: bus nhả cho điện trở kéo lên. */
    DHT11_SetInput();

    return DEV_SUCCESS;
}

/**
 * @brief Xoá bộ đệm 40 bit và hai chỉ số đếm về 0.
 */
static void DHT11_ClearRaw(void)
{
    uint8_t i;

    for (i = 0u; i < DHT11_RAW_BYTES; i++) {
        dht_raw_data[i] = 0u;
    }
    dht_bit_index = 0u;
    dht_byte_index = 0u;
}

/**
 * @brief Cấu hình chân GPIO của DHT11 thành ngõ vào, kích hoạt ngắt EXTI sườn xuống.
 */
static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = dht_cfg.Pin;
    gpio_init.Mode = GPIO_MODE_IT_FALLING;
    gpio_init.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(dht_cfg.Port, &gpio_init);

    /* Xoá cờ ngắt cũ rồi mới cho phép ngắt trên NVIC.
     * IRQn lấy từ cấu hình, không hardcode: PB1 dùng EXTI1_IRQn chứ
     * không phải EXTI2. */
    __HAL_GPIO_EXTI_CLEAR_IT(dht_cfg.Pin);
    HAL_NVIC_EnableIRQ(dht_cfg.IRQn);
}

/**
 * @brief Cấu hình chân GPIO của DHT11 thành ngõ ra Open-Drain để MCU kéo bus.
 */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /* Tắt ngắt EXTI trước khi đổi mode. HAL không tự gỡ EXTI mask khi chuyển
     * IT_FALLING -> OUTPUT, nên phải che tay: không làm thì chính MCU sẽ tự
     * kích ngắt lúc nó kéo bus xuống LOW để phát lệnh Start. */
    HAL_NVIC_DisableIRQ(dht_cfg.IRQn);
    EXTI->IMR &= ~((uint32_t)dht_cfg.Pin);  /* che ngắt của riêng chân này */
    EXTI->PR = dht_cfg.Pin;                 /* xoá cờ pending (ghi 1 để xoá) */

    /* Open-drain vì bus DHT11 là một dây hai chiều: cả MCU lẫn cảm biến đều
     * chỉ được phép kéo xuống, mức cao do điện trở kéo lên tạo ra. */
    gpio_init.Pin = dht_cfg.Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(dht_cfg.Port, &gpio_init);
}

/**
 * @brief Bắt đầu chu kỳ giao tiếp bằng cách kéo bus xuống LOW tối thiểu 18 ms.
 */
void DHT11_StartRequest(void)
{
    /* Chỉ nhận yêu cầu mới khi chu kỳ trước đã kết thúc (xong, lỗi, hoặc rảnh).
     * Cắt ngang một phiên đang đọc sẽ làm hỏng cả khung 40 bit. */
    if ((dht_current_state != DHT11_STATE_IDLE) &&
        (dht_current_state != DHT11_STATE_COMPLETE) &&
        (dht_current_state != DHT11_STATE_ERROR)) {
        return;
    }

    DHT11_ClearRaw();

    /* Chuyển chân sang Output và kéo LOW để phát lệnh Start */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(dht_cfg.Port, dht_cfg.Pin, GPIO_PIN_RESET);
    dht_current_state = DHT11_STATE_START_LOW;

    /* Hẹn giờ 18 ms cho giai đoạn kéo LOW */
    HAL_TIM_Base_Stop_IT(dht_cfg.htim);
    __HAL_TIM_SET_COUNTER(dht_cfg.htim, 0u);
    __HAL_TIM_SET_AUTORELOAD(dht_cfg.htim, DHT11_START_LOW_TIME_US);
    __HAL_TIM_CLEAR_FLAG(dht_cfg.htim, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(dht_cfg.htim);
}

/**
 * @brief Xử lý ngắt TIM2: chuyển trạng thái FSM và làm watchdog giám sát timeout.
 */
void DHT11_CallbackTIM2(void)
{
    switch (dht_current_state) {
    case DHT11_STATE_START_LOW:
        /* Đủ 18 ms kéo LOW: nhả bus, chuyển chân thành Input để nghe cảm biến */
        HAL_TIM_Base_Stop_IT(dht_cfg.htim);
        DHT11_SetInput();
        dht_current_state = DHT11_STATE_START_HIGH;

        /* Timer đổi vai thành watchdog: quá hạn nghĩa là cảm biến không đáp */
        __HAL_TIM_SET_COUNTER(dht_cfg.htim, 0u);
        __HAL_TIM_SET_AUTORELOAD(dht_cfg.htim, DHT11_WATCHDOG_US);
        __HAL_TIM_CLEAR_FLAG(dht_cfg.htim, TIM_FLAG_UPDATE);
        HAL_TIM_Base_Start_IT(dht_cfg.htim);
        break;

    case DHT11_STATE_START_HIGH:
    case DHT11_STATE_READING:
        /* Watchdog nổ trong lúc chờ phản hồi hoặc đang đọc dở: coi như đứt bus */
        HAL_TIM_Base_Stop_IT(dht_cfg.htim);
        dht_current_state = DHT11_STATE_ERROR;
        break;

    /* IDLE/COMPLETE/ERROR: không còn phiên nào đang chạy nên bỏ qua. Liệt kê
     * đủ để -Wswitch báo ngay nếu sau này thêm state mới. */
    case DHT11_STATE_IDLE:
    case DHT11_STATE_COMPLETE:
    case DHT11_STATE_ERROR:
    default:
        break;
    }
}

/**
 * @brief Xử lý ngắt EXTI: đo khoảng cách giữa hai sườn xuống và giải mã bit.
 */
void DHT11_CallbackEXTI(uint16_t exti_pin)
{
    uint16_t time_elapsed;

    if (exti_pin != dht_cfg.Pin) {
        return;
    }

    /* Đọc rồi reset bộ đếm ngay: mọi phép đo sau đây tính từ sườn xuống này */
    time_elapsed = (uint16_t)__HAL_TIM_GET_COUNTER(dht_cfg.htim);
    __HAL_TIM_SET_COUNTER(dht_cfg.htim, 0u);

    switch (dht_current_state) {
    case DHT11_STATE_START_HIGH:
        /* Sườn xuống đầu tiên là tín hiệu DHT11 báo sẵn sàng truyền dữ liệu */
        dht_current_state = DHT11_STATE_READING;
        dht_bit_index = 0u;
        dht_byte_index = 0u;
        break;

    case DHT11_STATE_READING:
        if ((time_elapsed > DHT11_PULSE_BIT1_MAX_US) &&
            (time_elapsed <= DHT11_PULSE_ACK_MAX_US)) {
            /* Nhịp mồi (ACK) — không phải bit dữ liệu, không ghi gì cả */
            break;
        }

        if ((time_elapsed >= DHT11_PULSE_BIT0_MIN_US) &&
            (time_elapsed <= DHT11_PULSE_BIT0_MAX_US)) {
            dht_raw_data[dht_byte_index] &=
                (uint8_t)~(1u << (DHT11_BITS_PER_BYTE - 1u - dht_bit_index));
            dht_bit_index++;
        } else if ((time_elapsed > DHT11_PULSE_BIT0_MAX_US) &&
                   (time_elapsed <= DHT11_PULSE_BIT1_MAX_US)) {
            dht_raw_data[dht_byte_index] |=
                (uint8_t)(1u << (DHT11_BITS_PER_BYTE - 1u - dht_bit_index));
            dht_bit_index++;
        } else {
            /* Xung có độ dài phi lý -> báo lỗi và tắt watchdog */
            dht_current_state = DHT11_STATE_ERROR;
            HAL_TIM_Base_Stop_IT(dht_cfg.htim);
            break;
        }

        /* Đủ 8 bit thì sang byte kế tiếp */
        if (dht_bit_index >= DHT11_BITS_PER_BYTE) {
            dht_bit_index = 0u;
            dht_byte_index++;
        }

        /* Đủ 40 bit thì chốt phiên; từ đây ISR không chạm dht_raw_data nữa
         * nên vòng lặp chính đọc cả mảng được mà không cần khoá ngắt. */
        if (dht_byte_index >= DHT11_RAW_BYTES) {
            dht_current_state = DHT11_STATE_COMPLETE;
            HAL_TIM_Base_Stop_IT(dht_cfg.htim);
        }
        break;

    case DHT11_STATE_IDLE:
    case DHT11_STATE_START_LOW:
    case DHT11_STATE_COMPLETE:
    case DHT11_STATE_ERROR:
    default:
        break;
    }
}

/**
 * @brief Phân tích 40 bit thô, đối chiếu checksum và gán số liệu cho người gọi.
 */
static void DHT11_ParseData(DHT11_Data_t *data)
{
    uint8_t sum = (uint8_t)(dht_raw_data[0] + dht_raw_data[1] +
                            dht_raw_data[2] + dht_raw_data[3]);

    if (sum == dht_raw_data[4]) {
        data->humidity_int = dht_raw_data[0];
        data->humidity_dec = dht_raw_data[1];
        data->temp_int = dht_raw_data[2];
        data->temp_dec = dht_raw_data[3];
        data->checksum = dht_raw_data[4];
        data->is_valid = true;
    } else {
        data->is_valid = false;
    }
}

/**
 * @brief Đọc kết quả từ máy trạng thái; gọi lặp lại từ vòng lặp chính.
 */
DHT11_State_t DHT11_ReadData(DHT11_Data_t *data)
{
    DHT11_State_t state_to_return;

    if (data == NULL) {
        return DHT11_STATE_ERROR;
    }

    /* Chụp một lần rồi làm việc trên bản chụp: dht_current_state là volatile,
     * ISR có thể đổi nó ngay giữa hai lần đọc. */
    state_to_return = dht_current_state;

    if (state_to_return == DHT11_STATE_COMPLETE) {
        DHT11_ParseData(data);
        dht_current_state = DHT11_STATE_IDLE;
    } else if (state_to_return == DHT11_STATE_ERROR) {
        data->is_valid = false;
        dht_current_state = DHT11_STATE_IDLE;
    } else {
        /* Đang dở phiên: chưa có gì để trả, người gọi cứ hỏi lại */
    }

    return state_to_return;
}
