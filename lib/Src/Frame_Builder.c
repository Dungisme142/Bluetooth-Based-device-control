#include "Frame_Builder.h"
#include "Framing_Base.h"
#include "Global_Enum.h"
#include <stdint.h>
#include <string.h>

Developer_Action_Result_t Frame_Builder_Init(Framing_Result_HandleTypeDef *framing_result_handler, void *framing_result_ptr, uint8_t framing_result_data_size_byte, uint8_t max_frame_size){
    if(framing_result_handler != NULL){
        framing_result_handler->framing_result_ptr = framing_result_ptr;
        framing_result_handler->framing_result_data_size_byte = framing_result_data_size_byte;
        framing_result_handler->max_frame_size = max_frame_size;
        framing_result_handler->framing_result_index = 0;
        framing_result_handler->framing_result_ready_to_read = DEV_FALSE;
        return DEV_SUCCESS;
    }
    else{
        return DEV_FAIL;
    }
}

Developer_Action_Result_t Frame_Building(Framing_Result_HandleTypeDef *framing_result_handler, Filting_Result_t fliting_result, void *current_processed_data){
    if(framing_result_handler != NULL){
        if(fliting_result == filting_result_ignore){
            return DEV_SUCCESS;
        }
        if(framing_result_handler->framing_result_index < framing_result_handler->max_frame_size - 1){
            if(fliting_result == filting_result_save_and_continue){
                uint8_t *dest = (uint8_t*)framing_result_handler->framing_result_ptr + (framing_result_handler->framing_result_index) * (framing_result_handler->framing_result_data_size_byte);
                memcpy(dest, current_processed_data, framing_result_handler->framing_result_data_size_byte);
                framing_result_handler->framing_result_index += 1;
            }
            else if(fliting_result == filting_result_finish){
                uint8_t *dest = (uint8_t*)framing_result_handler->framing_result_ptr + (framing_result_handler->framing_result_index) * (framing_result_handler->framing_result_data_size_byte);
                memset(dest, '\0', framing_result_handler->framing_result_data_size_byte);
                framing_result_handler->framing_result_ready_to_read = DEV_TRUE;
            }
            else if(fliting_result == filting_result_backspace){
                if(framing_result_handler->framing_result_index > 0){
                    framing_result_handler->framing_result_index -= 1;
                }
            }
            else if(fliting_result == filting_result_error){
                return DEV_FAIL;
            }
            return DEV_SUCCESS;
        }
        else if(framing_result_handler->framing_result_index == framing_result_handler->max_frame_size - 1){
            if(fliting_result == filting_result_finish){
                uint8_t *dest = (uint8_t*)framing_result_handler->framing_result_ptr + (framing_result_handler->framing_result_index) * (framing_result_handler->framing_result_data_size_byte);
                memset(dest, '\0', framing_result_handler->framing_result_data_size_byte);
                framing_result_handler->framing_result_ready_to_read = DEV_TRUE;
                return DEV_SUCCESS;
            }
            else if(fliting_result == filting_result_backspace){
                framing_result_handler->framing_result_index -= 1;
                return DEV_SUCCESS;
            }
            else{
                framing_result_handler->framing_result_index += 1;
                return DEV_SUCCESS;
            }
        }
        else{
            if(fliting_result == filting_result_backspace){
                framing_result_handler->framing_result_index -= 1;
                return DEV_SUCCESS;
            }
            else if (fliting_result == filting_result_finish){
                return DEV_FAIL;
            }
            else{
                framing_result_handler->framing_result_index += 1;
                return DEV_SUCCESS;
            }
        }
    }
    
    else{
        return DEV_FAIL;
    }
}