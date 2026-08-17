#include "Command_Selector.h"
#include "Global_Enum.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



Developer_Action_Result_t Command_Selecting(const Command_HandleTypeDef *Command_Menu, uint8_t Command_Menu_Size, char *return_msg, const char *received_cmd){
    if(received_cmd == NULL){
        return DEV_FAIL;
    }
    
    for(uint8_t i = 0; i < Command_Menu_Size; i++){
        uint8_t cmd_len = strlen(Command_Menu[i].command);
        if(strncmp(received_cmd, Command_Menu[i].command, cmd_len) == 0){
            const char *args_ptr = NULL;
            if(received_cmd[cmd_len] == '\0'){
                args_ptr = NULL;
            }
            else if(received_cmd[cmd_len] == ' '){
                args_ptr = received_cmd + cmd_len + 1;
            }
            else{
                continue;
            }
            Command_Menu[i].command_executing(return_msg, args_ptr);
            return DEV_SUCCESS;
        }
    }
    snprintf(return_msg, 256, "Invalid Command\r\n");
    return DEV_FAIL;

}