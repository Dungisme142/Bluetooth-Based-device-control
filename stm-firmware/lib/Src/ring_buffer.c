/*
 * ring_buffer.c — Bộ đệm vòng một-người-ghi/một-người-đọc.
 *
 * Write và Read cố tình đối xứng nhau và không gộp lại: hai hàm giống nhau về
 * hình thức nhưng thay đổi vì hai lý do khác nhau (một chạy trong ISR, một
 * chạy ở vòng lặp chính), gộp lại sẽ ràng buộc chúng vào nhau vô cớ.
 */

#include "ring_buffer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Con trỏ tới ô thứ `index` của bộ đệm. */
static uint8_t *Ring_Buffer_Slot_Ptr(const Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                     uint8_t index)
{
    return (uint8_t *)ring_buffer_handler->ring_buffer_ptr +
           (uint32_t)index * ring_buffer_handler->ring_buffer_data_size_byte;
}

Developer_Action_Result_t Ring_Buffer_Init(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                           void *ring_buffer_ptr,
                                           uint8_t ring_buffer_size,
                                           uint8_t ring_buffer_data_size_byte)
{
    if ((ring_buffer_handler == NULL) || (ring_buffer_ptr == NULL) ||
        (ring_buffer_size < 2u) || (ring_buffer_data_size_byte == 0u)) {
        /* Cần ít nhất 2 ô: một ô luôn để trống mới phân biệt được đầy với rỗng. */
        return DEV_FAIL;
    }

    ring_buffer_handler->ring_buffer_ptr = ring_buffer_ptr;
    ring_buffer_handler->ring_buffer_size = ring_buffer_size;
    ring_buffer_handler->ring_buffer_data_size_byte = ring_buffer_data_size_byte;
    ring_buffer_handler->ring_buffer_write_index = 0u;
    ring_buffer_handler->ring_buffer_read_index = 0u;
    ring_buffer_handler->ring_buffer_status = RING_BUFFER_EMPTY;

    return DEV_SUCCESS;
}

static void Ring_Buffer_Status_SingleCheck(Ring_Buffer_HandleTypeDef *ring_buffer_handler)
{
    uint8_t next_write = (uint8_t)((ring_buffer_handler->ring_buffer_write_index + 1u) %
                                   ring_buffer_handler->ring_buffer_size);

    if (ring_buffer_handler->ring_buffer_read_index ==
        ring_buffer_handler->ring_buffer_write_index) {
        ring_buffer_handler->ring_buffer_status = RING_BUFFER_EMPTY;
    } else if (next_write == ring_buffer_handler->ring_buffer_read_index) {
        ring_buffer_handler->ring_buffer_status = RING_BUFFER_FULL;
    } else {
        ring_buffer_handler->ring_buffer_status = RING_BUFFER_AVAILABLE;
    }
}

Developer_Action_Result_t Ring_Buffer_Write_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                                       const void *data_source)
{
    if ((ring_buffer_handler == NULL) || (data_source == NULL)) {
        return DEV_FAIL;
    }

    Ring_Buffer_Status_SingleCheck(ring_buffer_handler);
    if (ring_buffer_handler->ring_buffer_status == RING_BUFFER_FULL) {
        return DEV_FAIL;
    }

    memcpy(Ring_Buffer_Slot_Ptr(ring_buffer_handler,
                                ring_buffer_handler->ring_buffer_write_index),
           data_source, ring_buffer_handler->ring_buffer_data_size_byte);
    ring_buffer_handler->ring_buffer_write_index =
        (uint8_t)((ring_buffer_handler->ring_buffer_write_index + 1u) %
                  ring_buffer_handler->ring_buffer_size);

    return DEV_SUCCESS;
}

Developer_Action_Result_t Ring_Buffer_Read_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                                      void *data_dest)
{
    if ((ring_buffer_handler == NULL) || (data_dest == NULL)) {
        return DEV_FAIL;
    }

    Ring_Buffer_Status_SingleCheck(ring_buffer_handler);
    if (ring_buffer_handler->ring_buffer_status == RING_BUFFER_EMPTY) {
        return DEV_FAIL;
    }

    memcpy(data_dest,
           Ring_Buffer_Slot_Ptr(ring_buffer_handler,
                                ring_buffer_handler->ring_buffer_read_index),
           ring_buffer_handler->ring_buffer_data_size_byte);
    ring_buffer_handler->ring_buffer_read_index =
        (uint8_t)((ring_buffer_handler->ring_buffer_read_index + 1u) %
                  ring_buffer_handler->ring_buffer_size);

    return DEV_SUCCESS;
}
