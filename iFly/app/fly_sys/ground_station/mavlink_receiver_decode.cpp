#include "mavlink_receiver.hpp"

namespace iFly {

bool MavlinkReceiver::DecodeCommandRequest(const mavlink_message_t &msg,
                                           CommandRequest *state)
{
  if ((msg.msgid != MAVLINK_MSG_ID_COMMAND_LONG) || (state == nullptr)) {
    return false;
  }

  mavlink_command_long_t command {};
  mavlink_msg_command_long_decode(&msg, &command);
  state->target_system = command.target_system;
  state->target_component = command.target_component;
  state->command = command.command;
  state->confirmation = command.confirmation;
  state->argument[0] = command.param1;
  state->argument[1] = command.param2;
  state->argument[2] = command.param3;
  state->argument[3] = command.param4;
  state->argument[4] = command.param5;
  state->argument[5] = command.param6;
  state->argument[6] = command.param7;

  return true;
}

bool MavlinkReceiver::DecodeManualControl(const mavlink_message_t &msg,
                                          ManualControl *state)
{
  if ((msg.msgid != MAVLINK_MSG_ID_MANUAL_CONTROL) || (state == nullptr)) {
    return false;
  }

  mavlink_manual_control_t control {};
  mavlink_msg_manual_control_decode(&msg, &control);
  state->target = control.target;
  state->x = control.x;
  state->y = control.y;
  state->z = control.z;
  state->r = control.r;
  state->buttons = control.buttons;
  state->buttons2 = control.buttons2;

  return true;
}

bool MavlinkReceiver::IsTargetMatched(uint8_t target_system,
                                      uint8_t target_component)
{
  return ((target_system == 0U) || (target_system == kSystemId)) &&
         ((target_component == 0U) || (target_component == kComponentId));
}

void MavlinkReceiver::CopyParamId(const char *source,
                                  char *output,
                                  uint32_t output_size)
{
  if ((source == nullptr) || (output == nullptr) || (output_size == 0U)) {
    return;
  }

  uint32_t index = 0U;
  while ((index < kMavlinkParamIdLength) && ((index + 1U) < output_size) &&
         (source[index] != '\0')) {
    output[index] = source[index];
    ++index;
  }

  output[index] = '\0';
}

} // namespace iFly
