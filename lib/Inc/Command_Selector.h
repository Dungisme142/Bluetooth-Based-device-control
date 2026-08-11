#ifndef COMMAND_SELECTOR_H_
#define COMMAND_SELECTOR_H_

#include "Global_Enum.h"
#include <stdint.h>

typedef void (*Command_Executing_t)(char *return_msg, const char *args);

typedef struct{
    const char *command;
    Command_Executing_t command_executing; 
}Command_HandleTypeDef;

Developer_Action_Result_t Command_Selecting(const Command_HandleTypeDef *Command_Menu, uint8_t Command_Menu_Size, char *return_msg, const char *received_cmd);

#endif