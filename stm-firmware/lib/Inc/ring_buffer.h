/*
 * ring_buffer.h — Bộ đệm vòng tổng quát cho phần tử có kích thước bất kỳ.
 *
 * Không dính gì tới ngữ cảnh dùng nên giữ là module riêng: hiện cả cụm UART
 * lẫn tầng ứng dụng đều dùng, và bê sang project khác được nguyên vẹn.
 *
 * Chú ý về đồng thời: ISR ghi (Write) còn vòng lặp chính đọc (Read). Mô hình
 * một-người-ghi/một-người-đọc này an toàn vì mỗi bên chỉ sửa chỉ số của riêng
 * mình; đừng gọi Write từ hai ngữ cảnh khác nhau nếu không có critical section.
 */
#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include "global_enum.h"
#include <stdint.h>

typedef enum {
    RING_BUFFER_EMPTY = 0,   /* Trạng thái lúc khởi tạo */
    RING_BUFFER_FULL,
    RING_BUFFER_AVAILABLE
} Ring_Buffer_Status_t;

typedef struct {
    void    *ring_buffer_ptr;
    uint8_t  ring_buffer_size;             /* Số phần tử; một ô luôn để trống làm mốc phân biệt đầy/rỗng */
    uint8_t  ring_buffer_data_size_byte;
    uint8_t  ring_buffer_write_index;      /* Chỉ ISR ghi */
    uint8_t  ring_buffer_read_index;       /* Chỉ vòng lặp chính ghi */
    Ring_Buffer_Status_t ring_buffer_status;
} Ring_Buffer_HandleTypeDef;

/**
 * @brief  Gắn bộ đệm do người gọi cấp vào handle và đặt chỉ số về 0.
 * @retval DEV_SUCCESS nếu tham số hợp lệ, DEV_FAIL nếu không.
 */
Developer_Action_Result_t Ring_Buffer_Init(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                           void *ring_buffer_ptr,
                                           uint8_t ring_buffer_size,
                                           uint8_t ring_buffer_data_size_byte);

/**
 * @brief  Chép một phần tử vào bộ đệm.
 * @retval DEV_FAIL nếu bộ đệm đầy (dữ liệu bị bỏ, không ghi đè).
 */
Developer_Action_Result_t Ring_Buffer_Write_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                                       const void *data_source);

/**
 * @brief  Lấy một phần tử ra khỏi bộ đệm.
 * @retval DEV_FAIL nếu bộ đệm rỗng (data_dest không bị chạm tới).
 */
Developer_Action_Result_t Ring_Buffer_Read_SingleData(Ring_Buffer_HandleTypeDef *ring_buffer_handler,
                                                      void *data_dest);

#endif /* RING_BUFFER_H_ */
