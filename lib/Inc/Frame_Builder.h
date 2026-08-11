#ifndef FRAME_BUILDER_H_
#define FRAME_BUILDER_H_

#include "Framing_Base.h"
#include "Global_Enum.h"

Developer_Action_Result_t Frame_Builder_Init(Framing_Result_HandleTypeDef *framing_result_handler, void *framing_result_ptr, uint8_t framing_result_data_size_byte, uint8_t max_frame_size);
Developer_Action_Result_t Frame_Building(Framing_Result_HandleTypeDef *framing_result_handler, Filting_Result_t fliting_result, void *current_processed_data);

#endif