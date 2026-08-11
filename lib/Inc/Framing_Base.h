#ifndef FRAMING_BASE_H_
#define FRAMING_BASE_H_

#include "Global_Enum.h"
#include <stdint.h>

typedef enum{
    filting_result_save_and_continue,
    filting_result_backspace,
    filting_result_ignore,
    filting_result_finish,
    filting_result_error
}Filting_Result_t;

typedef struct{
    void *framing_result_ptr;
    uint8_t framing_result_data_size_byte;
    uint8_t max_frame_size;
    uint8_t framing_result_index;
    Developer_Logic_t framing_result_ready_to_read;
}Framing_Result_HandleTypeDef;



#endif