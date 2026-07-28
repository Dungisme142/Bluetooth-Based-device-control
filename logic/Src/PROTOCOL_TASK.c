#include "PROTOCOL_TASK.h"
#include "ACTUATOR_TASK.h"
#include "HC05.h"

#include <stdio.h>
#include <string.h>

void Protocol_Task_HandleCommand(const char *command, char *response, size_t response_size)
{
    if (command == NULL || response == NULL)
    {
        return;
    }

    if (strcmp(command, "RELAY_ON") == 0)
    {
        Actuator_Task_HandleCommand(1u);
        snprintf(response, response_size, "RELAY_ON");
    }
    else if (strcmp(command, "RELAY_OFF") == 0)
    {
        Actuator_Task_HandleCommand(0u);
        snprintf(response, response_size, "RELAY_OFF");
    }
    else if (strcmp(command, "GET_DATA") == 0)
    {
        snprintf(response, response_size, "DATA_OK");
    }
    else
    {
        snprintf(response, response_size, "UNKNOWN_CMD");
    }

    HC05_SendString(response);
}
