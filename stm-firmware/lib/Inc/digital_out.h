/*
 * digital_out.h — Một ngõ ra số, điều khiển bằng một chân GPIO push-pull.
 *
 * Driver generic theo {port, pin, state}: dùng cho cả 5 kênh OUT-1..OUT-5.
 * Mạch không còn module relay nào — đây chỉ là mức logic đưa ra chân cắm,
 * tầng công suất (nếu có) nằm ngoài board.
 */
#ifndef DIGITAL_OUT_H
#define DIGITAL_OUT_H

#include "global_enum.h"
#include "stm32f1xx_hal.h"

typedef enum {
    DIGITAL_OUT_OFF = 0,
    DIGITAL_OUT_ON
} Digital_Out_State_t;

typedef struct {
    GPIO_TypeDef        *port;
    uint16_t             pin;
    Digital_Out_State_t  state;
} Digital_Out_HandleTypeDef;

/**
 * @brief  Cấu hình chân thành ngõ ra push-pull.
 * @retval DEV_SUCCESS nếu handle hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t Digital_Out_Init(Digital_Out_HandleTypeDef *out);

/**
 * @brief  Đặt mức ngõ ra và ghi lại trạng thái mới vào handle.
 * @retval DEV_SUCCESS nếu handle hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t Digital_Out_SetState(Digital_Out_HandleTypeDef *out,
                                               Digital_Out_State_t state);

#endif /* DIGITAL_OUT_H */
