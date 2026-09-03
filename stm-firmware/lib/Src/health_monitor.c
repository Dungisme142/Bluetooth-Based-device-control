/*
 * health_monitor.c — Cài đặt giám sát sức khỏe phần cứng và chẩn đoán lỗi.
 */
#include "health_monitor.h"
#include <stddef.h>

void Health_Monitor_Init(Health_Monitor_t *mon)
{
    if (mon == NULL) {
        return;
    }
    /* Lúc vừa boot, chưa có mẫu DHT hợp lệ nên đặt cờ FAULT_DHT11_DEAD */
    mon->fault_mask = FAULT_DHT11_DEAD;
    mon->dht11_fail_count = 1U;
}

void Health_Monitor_ReportDHT11(Health_Monitor_t *mon, bool read_success)
{
    if (mon == NULL) {
        return;
    }

    if (read_success) {
        mon->dht11_fail_count = 0u;
        mon->fault_mask &= ~FAULT_DHT11_DEAD;
    } else {
        if (mon->dht11_fail_count < 255u) {
            mon->dht11_fail_count++;
        }
        if (mon->dht11_fail_count >= DHT11_FAIL_THRESHOLD) {
            mon->fault_mask |= FAULT_DHT11_DEAD;
        }
    }
}

void Health_Monitor_ReportBT(Health_Monitor_t *mon, bool bt_connected)
{
    if (mon == NULL) {
        return;
    }

    if (bt_connected) {
        mon->fault_mask &= ~FAULT_BT_LOST;
    } else {
        mon->fault_mask |= FAULT_BT_LOST;
    }
}

bool Health_Monitor_HasFault(const Health_Monitor_t *mon, uint32_t fault_flag)
{
    if (mon == NULL) {
        return false;
    }
    return ((mon->fault_mask & fault_flag) != 0u);
}

bool Health_Monitor_IsDHTHealthy(const Health_Monitor_t *mon)
{
    if (mon == NULL) {
        return false;
    }
    return ((mon->fault_mask & FAULT_DHT11_DEAD) == 0u);
}

const char *Health_Monitor_GetDHTStatusString(const Health_Monitor_t *mon)
{
    if ((mon == NULL) || ((mon->fault_mask & FAULT_DHT11_DEAD) != 0u)) {
        return "BAD";
    }
    return "OK";
}
