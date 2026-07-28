#include "ACTUATOR_TASK.h"
#include "MKE_M01_LED.h"
#include "MKE_M05_RELAY.h"

void Actuator_Task_Run(uint8_t relay_state, uint8_t led_state)
{
    Relay_SetState(relay_state);
    LED_SetState(led_state);
}

void Actuator_Task_HandleCommand(uint8_t relay_state)
{
    Relay_SetState(relay_state);
    LED_Toggle();
}
