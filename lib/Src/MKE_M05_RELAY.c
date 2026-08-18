/* MKE_M05_RELAY.c — Module relay một kênh. */
#include "MKE_M05_RELAY.h"

#include <stddef.h>

Developer_Action_Result_t MKE_M05_RELAY_Init(MKE_M05_RELAY_HandleTypeDef *relay)
{
    GPIO_InitTypeDef gpio_init = {0};

    if ((relay == NULL) || (relay->port == NULL)) {
        return DEV_FAIL;
    }

    gpio_init.Pin = relay->pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    /* Bắt buộc set Speed cho chân output trên F1: để 0 là giá trị không hợp lệ
     * (MODE = 00 = input), chân sẽ không lái được mức logic. */
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(relay->port, &gpio_init);

    return DEV_SUCCESS;
}

Developer_Action_Result_t MKE_M05_RELAY_SetState(MKE_M05_RELAY_HandleTypeDef *relay,
                                                 MKE_M05_RELAY_State_t state)
{
    if ((relay == NULL) || (relay->port == NULL)) {
        return DEV_FAIL;
    }

    HAL_GPIO_WritePin(relay->port, relay->pin,
                      (state == MKE_M05_RELAY_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    relay->state = state;

    return DEV_SUCCESS;
}
