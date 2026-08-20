/* Digital_Out.c — Một ngõ ra số trên một chân GPIO push-pull. */
#include "Digital_Out.h"

#include <stddef.h>

Developer_Action_Result_t Digital_Out_Init(Digital_Out_HandleTypeDef *out)
{
    GPIO_InitTypeDef gpio_init = {0};

    if ((out == NULL) || (out->port == NULL)) {
        return DEV_FAIL;
    }

    gpio_init.Pin = out->pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    /* Bắt buộc set Speed cho chân output trên F1: để 0 là giá trị không hợp lệ
     * (MODE = 00 = input), chân sẽ không lái được mức logic. */
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(out->port, &gpio_init);

    return DEV_SUCCESS;
}

Developer_Action_Result_t Digital_Out_SetState(Digital_Out_HandleTypeDef *out,
                                               Digital_Out_State_t state)
{
    if ((out == NULL) || (out->port == NULL)) {
        return DEV_FAIL;
    }

    HAL_GPIO_WritePin(out->port, out->pin,
                      (state == DIGITAL_OUT_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    out->state = state;

    return DEV_SUCCESS;
}
