/*
 * MKE_M05_RELAY.h — Module relay một kênh, điều khiển bằng một chân GPIO.
 */
#ifndef MKE_M05_RELAY_H
#define MKE_M05_RELAY_H

#include "Global_Enum.h"
#include "stm32f1xx_hal.h"

typedef enum {
    MKE_M05_RELAY_OFF = 0,
    MKE_M05_RELAY_ON
} MKE_M05_RELAY_State_t;

typedef struct {
    GPIO_TypeDef        *port;
    uint16_t             pin;
    MKE_M05_RELAY_State_t state;
} MKE_M05_RELAY_HandleTypeDef;

/**
 * @brief  Cấu hình chân điều khiển relay thành ngõ ra push-pull.
 * @retval DEV_SUCCESS nếu handle hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t MKE_M05_RELAY_Init(MKE_M05_RELAY_HandleTypeDef *relay);

/**
 * @brief  Đóng/mở relay và ghi lại trạng thái mới vào handle.
 * @retval DEV_SUCCESS nếu handle hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t MKE_M05_RELAY_SetState(MKE_M05_RELAY_HandleTypeDef *relay,
                                                 MKE_M05_RELAY_State_t state);

#endif /* MKE_M05_RELAY_H */
