#include "DISPLAY_TASK.h"
#include "DHT11.h"
#include "SSD1306.h"

void Display_Task_Update(float temperature, float humidity, uint8_t relay_state, uint8_t bluetooth_connected)
{
    SSD1306_ShowData(temperature, humidity, relay_state, bluetooth_connected);
}

void Display_Task_Refresh(void)
{
    float temperature = 0.0f;
    float humidity = 0.0f;

    if (DHT11_ReadData(&temperature, &humidity) > 0.0f)
    {
        SSD1306_ShowData(temperature, humidity, 1u, 1u);
    }
}
