#ifndef PROTOCOL_TASK_H
#define PROTOCOL_TASK_H

#include <stddef.h>

void Protocol_Task_HandleCommand(const char *command, char *response, size_t response_size);

#endif
