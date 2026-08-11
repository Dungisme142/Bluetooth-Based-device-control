#include "Text_Filter.h"
#include "Framing_Base.h"
#include <stddef.h>

Filting_Result_t Text_Filting(uint8_t *current_processed_data){
    if(current_processed_data != NULL){
        if(*current_processed_data == '\r'){
            return filting_result_finish;
        }
        else if(*current_processed_data == '\b'){
            return filting_result_backspace;
        }
        else if(*current_processed_data == 0x07){
            return filting_result_ignore;
        }
        else{
            return filting_result_save_and_continue;
        }
    }
    else{
        return filting_result_error;
    }
}