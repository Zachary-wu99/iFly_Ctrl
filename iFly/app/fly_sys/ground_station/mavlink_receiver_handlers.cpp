#include "mavlink_receiver.hpp"

#include "tick.hpp"

namespace iFly {

void MavlinkReceiver::HandleHeartbeatMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandlePingMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_ping_t request {};
  if (!link_->DecodePing(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  mavlink_ping_t response {};
  response.time_usec = request.time_usec;
  response.seq = request.seq;
  response.target_system = msg.sysid;
  response.target_component = msg.compid;

  mavlink_message_t response_msg {};
  (void)link_->PackPing(&response_msg, response);
  link_->SendMessage(response_msg);
}

void MavlinkReceiver::HandleTimesyncMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_timesync_t request {};
  if (!link_->DecodeTimesync(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component) ||
      (request.tc1 != 0)) {
    return;
  }

  mavlink_timesync_t response {};
  response.tc1 = static_cast<int64_t>(tick::NowNs());
  response.ts1 = request.ts1;
  response.target_system = msg.sysid;
  response.target_component = msg.compid;

  mavlink_message_t response_msg {};
  (void)link_->PackTimesync(&response_msg, response);
  link_->SendMessage(response_msg);
}

void MavlinkReceiver::HandleModeMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandRequestMessage(const mavlink_message_t &msg)
{
  CommandRequest state {};
  if (DecodeCommandRequest(msg, &state)) {
    if (!IsTargetMatched(state.target_system, state.target_component)) {
      return;
    }

    HandleCommandRequest(state);

    uint8_t result = MAV_RESULT_UNSUPPORTED;
    switch (state.command) {
      case MAV_CMD_REQUEST_MESSAGE: {
        if (state.argument[0] >= 0.0f) {
          const uint32_t message_id =
              static_cast<uint32_t>(state.argument[0]);
          result = SendRequestedMessage(message_id) ? MAV_RESULT_ACCEPTED
                                                    : MAV_RESULT_UNSUPPORTED;
        }
        break;
      }

      case MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES:
        result = (state.argument[0] > 0.0f) && SendAutopilotVersion()
                     ? MAV_RESULT_ACCEPTED
                     : MAV_RESULT_DENIED;
        break;

      case MAV_CMD_REQUEST_PROTOCOL_VERSION:
        result = SendProtocolVersion() ? MAV_RESULT_ACCEPTED
                                       : MAV_RESULT_FAILED;
        break;

      default:
        break;
    }

    SendCommandAck(msg, state.command, result);
  }
}

void MavlinkReceiver::HandleCommandIntMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_command_int_t request {};
  if (!link_->DecodeCommandInt(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  SendCommandAck(msg, request.command, MAV_RESULT_UNSUPPORTED);
}

void MavlinkReceiver::HandleCommandAckMessage(const mavlink_message_t &msg)
{
  (void)msg;
}

void MavlinkReceiver::HandleCommandCancelMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_command_cancel_t request {};
  if (!link_->DecodeCommandCancel(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  SendCommandAck(msg, request.command, MAV_RESULT_CANCELLED);
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
  if (link_ == nullptr) {
    return;
  }

  mavlink_param_request_read_t request {};
  if (!link_->DecodeParamRequestRead(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  MavlinkParameterValue parameter {};
  if (request.param_index >= 0) {
    if (parameter_service_.ReadByIndex(
            static_cast<uint16_t>(request.param_index), &parameter)) {
      (void)SendParameterValue(parameter);
    }
    return;
  }

  char name[kMavlinkParamIdLength + 1U] {};
  CopyParamId(request.param_id, name, sizeof(name));
  if (parameter_service_.ReadByName(name, &parameter)) {
    (void)SendParameterValue(parameter);
  }
}

void MavlinkReceiver::HandleParameterListMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_param_request_list_t request {};
  if (!link_->DecodeParamRequestList(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  const uint16_t count = parameter_service_.Count();
  for (uint16_t index = 0U; index < count; ++index) {
    MavlinkParameterValue parameter {};
    if (parameter_service_.ReadByIndex(index, &parameter)) {
      (void)SendParameterValue(parameter);
    }
  }
}

void MavlinkReceiver::HandleParameterSetMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_param_set_t request {};
  if (!link_->DecodeParamSet(msg, &request)) {
    return;
  }

  if (!IsTargetMatched(request.target_system, request.target_component)) {
    return;
  }

  char name[kMavlinkParamIdLength + 1U] {};
  CopyParamId(request.param_id, name, sizeof(name));

  (void)parameter_service_.WriteValue(name,
                                      request.param_value,
                                      request.param_type);

  MavlinkParameterValue parameter {};
  if (parameter_service_.ReadByName(name, &parameter)) {
    (void)SendParameterValue(parameter);
  }
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

bool MavlinkReceiver::SendParameterValue(
    const MavlinkParameterValue &parameter)
{
  if (link_ == nullptr) {
    return false;
  }

  mavlink_message_t msg {};
  if (link_->PackParameterValue(&msg,
                                parameter.name,
                                parameter.value,
                                parameter.type,
                                parameter.count,
                                parameter.index) == 0U) {
    return false;
  }

  link_->SendMessage(msg);
  return true;
}

void MavlinkReceiver::SendCommandAck(const mavlink_message_t &request,
                                     uint16_t command,
                                     uint8_t result)
{
  if (link_ == nullptr) {
    return;
  }

  mavlink_command_ack_t ack {};
  ack.command = command;
  ack.result = result;
  ack.progress = 0xFFU;
  ack.result_param2 = 0;
  ack.target_system = request.sysid;
  ack.target_component = request.compid;

  mavlink_message_t msg {};
  (void)link_->PackCommandAck(&msg, ack);
  link_->SendMessage(msg);
}

bool MavlinkReceiver::SendHeartbeat()
{
  if (link_ == nullptr) {
    return false;
  }

  MavlinkParameterValue vehicle_type {};
  MavlinkParameterValue autopilot_type {};

  mavlink_heartbeat_t heartbeat {};
  heartbeat.type = parameter_service_.ReadByName("MAV_TYPE", &vehicle_type)
                       ? static_cast<uint8_t>(vehicle_type.value)
                       : MAV_TYPE_GENERIC;
  heartbeat.autopilot =
      parameter_service_.ReadByName("MAV_AUTOPILOT", &autopilot_type)
          ? static_cast<uint8_t>(autopilot_type.value)
          : MAV_AUTOPILOT_GENERIC;
  heartbeat.base_mode = MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
  heartbeat.custom_mode = 0U;
  heartbeat.system_status = MAV_STATE_STANDBY;

  mavlink_message_t msg {};
  if (link_->PackHeartbeat(&msg, heartbeat) == 0U) {
    return false;
  }

  link_->SendMessage(msg);
  return true;
}

bool MavlinkReceiver::SendAutopilotVersion()
{
  if (link_ == nullptr) {
    return false;
  }

  mavlink_autopilot_version_t version {};
  version.capabilities =
      static_cast<uint64_t>(MAV_PROTOCOL_CAPABILITY_PARAM_FLOAT) |
      static_cast<uint64_t>(MAV_PROTOCOL_CAPABILITY_PARAM_ENCODE_C_CAST) |
      static_cast<uint64_t>(MAV_PROTOCOL_CAPABILITY_MAVLINK2);

  mavlink_message_t msg {};
  if (link_->PackAutopilotVersion(&msg, version) == 0U) {
    return false;
  }

  link_->SendMessage(msg);
  return true;
}

bool MavlinkReceiver::SendProtocolVersion()
{
  if (link_ == nullptr) {
    return false;
  }

  mavlink_protocol_version_t version {};
  version.version = 200U;
  version.min_version = 100U;
  version.max_version = 200U;

  mavlink_message_t msg {};
  if (link_->PackProtocolVersion(&msg, version) == 0U) {
    return false;
  }

  link_->SendMessage(msg);
  return true;
}

bool MavlinkReceiver::SendRequestedMessage(uint32_t message_id)
{
  switch (message_id) {
    case MAVLINK_MSG_ID_HEARTBEAT:
      return SendHeartbeat();

    case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
      return SendAutopilotVersion();

    case MAVLINK_MSG_ID_PROTOCOL_VERSION:
      return SendProtocolVersion();

    default:
      return false;
  }
}

} // namespace iFly
