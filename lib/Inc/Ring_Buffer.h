#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>
#include "Global_Enum.h"

typedef enum{
    ring_buffer_full,
    ring_buffer_empty,
    ring_buffer_available
}Ring_Buffer_Status_t;

typedef struct{
    void *ring_buffer_ptr;
    uint8_t ring_buffer_size;
    uint8_t ring_buffer_data_size_byte;
    uint8_t ring_buffer_write_index;
    uint8_t ring_buffer_read_index;
    Ring_Buffer_Status_t ring_buffer_status;
}Ring_Buffer_HandleTypeDef;

Developer_Action_Result_t Ring_Buffer_Init(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *ring_buffer_ptr, uint8_t ring_buffer_size, uint8_t ring_buffer_data_size_byte);
Developer_Action_Result_t Ring_Buffer_Write_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *data_source);
Developer_Action_Result_t Ring_Buffer_Read_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler, void *data_dest);


#endif