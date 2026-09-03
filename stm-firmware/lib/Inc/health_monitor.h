/*
 * health_monitor.h — Giám sát sức khỏe phần cứng và chẩn đoán lỗi hệ thống.
 *
 * Theo dõi trạng thái cảm biến DHT11 và kết nối Bluetooth. Cung cấp cờ lỗi
 * để điều khiển còi/LED cảnh báo PA8 và các chỉ báo trực quan trên OLED UI (!DHT, DHT BAD).
 */
#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#define DHT11_FAIL_THRESHOLD 1U /* Đọc lỗi 1 lần là kích hoạt trạng thái cảnh báo ngay */

typedef enum {
    HEALTH_OK        = 0x00,
    FAULT_DHT11_DEAD = (1 << 0),
    FAULT_BT_LOST    = (1 << 1)
} System_Fault_Mask_t;

typedef struct {
    uint32_t fault_mask;
    uint8_t  dht11_fail_count;
} Health_Monitor_t;

/**
 * @brief Khởi tạo hệ thống giám sát sức khỏe lúc boot.
 */
void Health_Monitor_Init(Health_Monitor_t *mon);

/**
 * @brief Cập nhật kết quả phiên đọc cảm biến DHT11.
 *
 * @param mon          Con trỏ handle Health_Monitor.
 * @param read_success true nếu đọc thành công và đúng checksum, false nếu lỗi/quá hạn.
 */
void Health_Monitor_ReportDHT11(Health_Monitor_t *mon, bool read_success);

/**
 * @brief Cập nhật trạng thái kết nối Bluetooth.
 *
 * @param mon          Con trỏ handle Health_Monitor.
 * @param bt_connected true nếu đang có kết nối và trao đổi byte, false nếu mất liên lạc.
 */
void Health_Monitor_ReportBT(Health_Monitor_t *mon, bool bt_connected);

/**
 * @brief Kiểm tra xem một cờ lỗi cụ thể có đang kích hoạt không.
 */
bool Health_Monitor_HasFault(const Health_Monitor_t *mon, uint32_t fault_flag);

/**
 * @brief Kiểm tra nhanh xem cảm biến DHT11 có đang hoạt động tốt không.
 */
bool Health_Monitor_IsDHTHealthy(const Health_Monitor_t *mon);

/**
 * @brief Lấy chuỗi biểu diễn trạng thái DHT ("OK" hoặc "BAD").
 */
const char *Health_Monitor_GetDHTStatusString(const Health_Monitor_t *mon);

#endif /* HEALTH_MONITOR_H */
