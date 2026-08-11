#include "Ring_Buffer.h"
#include "Global_Enum.h"
#include <stdint.h>
#include <string.h>


Developer_Action_Result_t Ring_Buffer_Init(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *ring_buffer_ptr, uint8_t ring_buffer_size, uint8_t ring_buffer_data_size_byte){
    if((ring_buffer_handler != NULL) && (ring_buffer_ptr != NULL)){
        ring_buffer_handler->ring_buffer_ptr = ring_buffer_ptr;
        ring_buffer_handler->ring_buffer_size = ring_buffer_size;
        ring_buffer_handler->ring_buffer_data_size_byte = ring_buffer_data_size_byte;
        ring_buffer_handler->ring_buffer_write_index = 0;
        ring_buffer_handler->ring_buffer_read_index = 0;
        ring_buffer_handler->ring_buffer_status = ring_buffer_empty;
        return success;
    }
    else{
        return fail;
    }
}

static void Ring_Buffer_Status_SingleCheck(Ring_Buffer_HandleTypeDef *ring_buffer_handler){
    if(ring_buffer_handler->ring_buffer_read_index == ring_buffer_handler->ring_buffer_write_index){
        ring_buffer_handler->ring_buffer_status = ring_buffer_empty;
    }
    else if(((ring_buffer_handler->ring_buffer_write_index + 1) % ring_buffer_handler->ring_buffer_size) == ring_buffer_handler->ring_buffer_read_index){
        ring_buffer_handler->ring_buffer_status = ring_buffer_full;
    }
    else{
        ring_buffer_handler->ring_buffer_status = ring_buffer_available;
    }
}

Developer_Action_Result_t Ring_Buffer_Write_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *data_source){
    if((ring_buffer_handler != NULL) && (data_source != NULL)){
        Ring_Buffer_Status_SingleCheck(ring_buffer_handler);
        if(ring_buffer_handler->ring_buffer_status != ring_buffer_full){
            uint8_t *dest = (uint8_t*)ring_buffer_handler->ring_buffer_ptr + (ring_buffer_handler->ring_buffer_write_index) * (ring_buffer_handler->ring_buffer_data_size_byte);
            memcpy(dest, data_source, ring_buffer_handler->ring_buffer_data_size_byte);
            ring_buffer_handler->ring_buffer_write_index = ((ring_buffer_handler->ring_buffer_write_index + 1) % ring_buffer_handler->ring_buffer_size);
            return success;
        }
        else{
            return fail;
        }
    }
    else{
        return fail;
    }
    
}

Developer_Action_Result_t Ring_Buffer_Read_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *data_dest){
    if((ring_buffer_handler != NULL) && (data_dest != NULL)){
        Ring_Buffer_Status_SingleCheck(ring_buffer_handler);
        if(ring_buffer_handler->ring_buffer_status != ring_buffer_empty){
            uint8_t *src = (uint8_t*)ring_buffer_handler->ring_buffer_ptr + (ring_buffer_handler->ring_buffer_read_index) * (ring_buffer_handler->ring_buffer_data_size_byte);
            memcpy(data_dest, src, ring_buffer_handler->ring_buffer_data_size_byte);
            ring_buffer_handler->ring_buffer_read_index = ((ring_buffer_handler->ring_buffer_read_index + 1) % ring_buffer_handler->ring_buffer_size);
            return success;
        }   
        else{
            return fail;
        }
    }
    else{
        return fail;
    }
}


