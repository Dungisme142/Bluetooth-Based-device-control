#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

/* Đọc dữ liệu cảm biến DHT11 và truyền kết quả ra các biến đầu ra */
void Sensor_Task_ReadAndPublish(float *temperature, float *humidity);

#endif
