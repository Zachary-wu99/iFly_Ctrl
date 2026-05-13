#include "mavlink_receiver.hpp"

namespace iFly {

void MavlinkReceiver::HandleHeartbeatMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandlePingMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleTimesyncMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleModeMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandRequestMessage(const mavlink_message_t &msg)
{
  CommandRequest state {};
  if (DecodeCommandRequest(msg, &state)) {
    HandleCommandRequest(state);
  }
}

void MavlinkReceiver::HandleCommandIntMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandAckMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandCancelMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleManualControlMessage(const mavlink_message_t &msg)
{
  ManualControl state {};
  if (DecodeManualControl(msg, &state)) {
    HandleManualControl(state);
  }
}

void MavlinkReceiver::HandleRcOverrideMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleGuidedControlMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleHomePositionMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleFollowTargetMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleLandingTargetMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleDataStreamRequestMessage(
    const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleParameterReadMessage(const mavlink_message_t &msg)
{
  mavlink_param_request_read_t request {};
  mavlink_msg_param_request_read_decode(&msg, &request);
  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }
}

void MavlinkReceiver::HandleParameterListMessage(const mavlink_message_t &msg)
{
  mavlink_param_request_list_t request {};
  mavlink_msg_param_request_list_decode(&msg, &request);
  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }
}

void MavlinkReceiver::HandleParameterSetMessage(const mavlink_message_t &msg)
{
  mavlink_param_set_t request {};
  mavlink_msg_param_set_decode(&msg, &request);
  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  char name[kMavlinkParamIdLength + 1U] {};
  CopyParamId(request.param_id, name, sizeof(name));

  (void)parameter_service_.WriteValue(name,
                                      request.param_value,
                                      request.param_type);
}

void MavlinkReceiver::HandleParameterMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleExtendedParameterMessage(
    const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleMissionMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleFileTransferMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleLogMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleGpsInputMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleHilMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandRequest(const CommandRequest &state)
{
  (void)state;
}

void MavlinkReceiver::HandleManualControl(const ManualControl &state)
{
  (void)state;
}

void MavlinkReceiver::HandleConsoleMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleUnknownMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

} // namespace iFly
