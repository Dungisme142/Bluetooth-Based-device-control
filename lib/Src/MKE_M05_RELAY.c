/* Relay module implementation */
#include "MKE_M05_RELAY.h"

MKE_M05_RELAY_StatusTypeDef MKE_M05_RELAY_Init (MKE_M05_RELAY_HandleTypeDef* relay) {
	// Init pin
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin				 = relay->pin;
	GPIO_InitStruct.Mode			 = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull			 = GPIO_NOPULL;
	HAL_GPIO_Init (relay->port, &GPIO_InitStruct);

	return MKE_M05_RELAY_OK;
}
MKE_M05_RELAY_StatusTypeDef MKE_M05_RELAY_SetState (MKE_M05_RELAY_HandleTypeDef* relay,
MKE_M05_RELAY_STATE state) {
	HAL_GPIO_WritePin (relay->port, relay->pin,
	(state == MKE_M05_RELAY_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	return MKE_M05_RELAY_OK;
}