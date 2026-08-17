/* Relay module header */
#ifndef MKE_M05_RELAY_H

#define MKE_M05_RELAY_H

#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"

typedef enum {
	MKE_M05_RELAY_OFF = 0,
	MKE_M05_RELAY_ON  = 1,
} MKE_M05_RELAY_STATE;

typedef enum {
	MKE_M05_RELAY_OK = 0,
	MKE_M05_RELAY_ERROR,
} MKE_M05_RELAY_StatusTypeDef;

typedef struct {
	/* data */
	GPIO_TypeDef* port;
	uint16_t pin;
	MKE_M05_RELAY_STATE state;
} MKE_M05_RELAY_HandleTypeDef;


MKE_M05_RELAY_StatusTypeDef MKE_M05_RELAY_Init (MKE_M05_RELAY_HandleTypeDef* relay);
MKE_M05_RELAY_StatusTypeDef MKE_M05_RELAY_SetState (MKE_M05_RELAY_HandleTypeDef* relay,
MKE_M05_RELAY_STATE state);


#endif // MKE_M05_RELAY_H