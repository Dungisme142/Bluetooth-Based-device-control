#include "SENSOR_TASK.h"
#include "DHT11.h"

void Sensor_Task_ReadAndPublish(float *temperature, float *humidity)
{
    if (temperature == NULL || humidity == NULL)
    {
        return;
    }

    DHT11_ReadData(temperature, humidity);
}
