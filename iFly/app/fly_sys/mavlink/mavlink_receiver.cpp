#include "mavlink_receiver.hpp"

namespace iFly {

MavlinkReceiver::MavlinkReceiver(MavlinkLink *link)
    : link_(link)
{
}

void MavlinkReceiver::BindLink(MavlinkLink *link)
{
  link_ = link;
}

bool MavlinkReceiver::ReceiveMessage(mavlink_message_t *msg)
{
  if (link_ == nullptr) {
    return false;
  }

  return link_->ReceiveMessage(msg);
}

bool MavlinkReceiver::Poll()
{
  mavlink_message_t msg {};
  if (!ReceiveMessage(&msg)) {
    return false;
  }

  DispatchMessage(msg);
  return true;
}

void MavlinkReceiver::DispatchMessage(const mavlink_message_t &msg)
{
  switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:
      dispatch_state_ = DispatchState::kHeartbeat;
      HandleHeartbeatMessage(msg);
      break;

    case MAVLINK_MSG_ID_PING:
      dispatch_state_ = DispatchState::kPing;
      HandlePingMessage(msg);
      break;

    case MAVLINK_MSG_ID_TIMESYNC:
      dispatch_state_ = DispatchState::kTimesync;
      HandleTimesyncMessage(msg);
      break;

    case MAVLINK_MSG_ID_SET_MODE:
      dispatch_state_ = DispatchState::kMode;
      HandleModeMessage(msg);
      break;

    case MAVLINK_MSG_ID_COMMAND_LONG:
      dispatch_state_ = DispatchState::kCommandRequest;
      HandleCommandRequestMessage(msg);
      break;

    case MAVLINK_MSG_ID_COMMAND_INT:
      dispatch_state_ = DispatchState::kCommandIntRequest;
      HandleCommandIntMessage(msg);
      break;

    case MAVLINK_MSG_ID_COMMAND_ACK:
      dispatch_state_ = DispatchState::kCommandAck;
      HandleCommandAckMessage(msg);
      break;

    case MAVLINK_MSG_ID_COMMAND_CANCEL:
      dispatch_state_ = DispatchState::kCommandCancel;
      HandleCommandCancelMessage(msg);
      break;

    case MAVLINK_MSG_ID_MANUAL_CONTROL:
      dispatch_state_ = DispatchState::kManualControl;
      HandleManualControlMessage(msg);
      break;

    case MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE:
      dispatch_state_ = DispatchState::kRcOverride;
      HandleRcOverrideMessage(msg);
      break;

    case MAVLINK_MSG_ID_SET_ATTITUDE_TARGET:
    case MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED:
    case MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT:
      dispatch_state_ = DispatchState::kGuidedControl;
      HandleGuidedControlMessage(msg);
      break;

    case MAVLINK_MSG_ID_SET_GPS_GLOBAL_ORIGIN:
    case MAVLINK_MSG_ID_SET_HOME_POSITION:
      dispatch_state_ = DispatchState::kHomePosition;
      HandleHomePositionMessage(msg);
      break;

    case MAVLINK_MSG_ID_FOLLOW_TARGET:
      dispatch_state_ = DispatchState::kFollowTarget;
      HandleFollowTargetMessage(msg);
      break;

    case MAVLINK_MSG_ID_LANDING_TARGET:
      dispatch_state_ = DispatchState::kLandingTarget;
      HandleLandingTargetMessage(msg);
      break;

    case MAVLINK_MSG_ID_REQUEST_DATA_STREAM:
      dispatch_state_ = DispatchState::kDataStreamRequest;
      HandleDataStreamRequestMessage(msg);
      break;

    case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
      dispatch_state_ = DispatchState::kParameterRead;
      HandleParameterReadMessage(msg);
      break;

    case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
      dispatch_state_ = DispatchState::kParameterList;
      HandleParameterListMessage(msg);
      break;

    case MAVLINK_MSG_ID_PARAM_SET:
      dispatch_state_ = DispatchState::kParameterSet;
      HandleParameterSetMessage(msg);
      break;

    case MAVLINK_MSG_ID_PARAM_MAP_RC:
      dispatch_state_ = DispatchState::kParameter;
      HandleParameterMessage(msg);
      break;

    case MAVLINK_MSG_ID_PARAM_EXT_REQUEST_READ:
    case MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST:
    case MAVLINK_MSG_ID_PARAM_EXT_SET:
    case MAVLINK_MSG_ID_PARAM_EXT_ACK:
      dispatch_state_ = DispatchState::kExtendedParameter;
      HandleExtendedParameterMessage(msg);
      break;

    case MAVLINK_MSG_ID_MISSION_REQUEST_PARTIAL_LIST:
    case MAVLINK_MSG_ID_MISSION_WRITE_PARTIAL_LIST:
    case MAVLINK_MSG_ID_MISSION_ITEM:
    case MAVLINK_MSG_ID_MISSION_REQUEST:
    case MAVLINK_MSG_ID_MISSION_SET_CURRENT:
    case MAVLINK_MSG_ID_MISSION_REQUEST_LIST:
    case MAVLINK_MSG_ID_MISSION_COUNT:
    case MAVLINK_MSG_ID_MISSION_CLEAR_ALL:
    case MAVLINK_MSG_ID_MISSION_ACK:
    case MAVLINK_MSG_ID_MISSION_ITEM_INT:
    case MAVLINK_MSG_ID_MISSION_REQUEST_INT:
      dispatch_state_ = DispatchState::kMission;
      HandleMissionMessage(msg);
      break;

    case MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL:
      dispatch_state_ = DispatchState::kFileTransfer;
      HandleFileTransferMessage(msg);
      break;

    case MAVLINK_MSG_ID_LOG_REQUEST_LIST:
    case MAVLINK_MSG_ID_LOG_REQUEST_DATA:
    case MAVLINK_MSG_ID_LOG_ERASE:
    case MAVLINK_MSG_ID_LOG_REQUEST_END:
      dispatch_state_ = DispatchState::kLog;
      HandleLogMessage(msg);
      break;

    case MAVLINK_MSG_ID_GPS_INPUT:
      dispatch_state_ = DispatchState::kGpsInput;
      HandleGpsInputMessage(msg);
      break;

    case MAVLINK_MSG_ID_HIL_STATE:
    case MAVLINK_MSG_ID_HIL_CONTROLS:
    case MAVLINK_MSG_ID_HIL_RC_INPUTS_RAW:
    case MAVLINK_MSG_ID_HIL_SENSOR:
    case MAVLINK_MSG_ID_HIL_GPS:
    case MAVLINK_MSG_ID_HIL_OPTICAL_FLOW:
    case MAVLINK_MSG_ID_HIL_STATE_QUATERNION:
    case MAVLINK_MSG_ID_HIL_ACTUATOR_CONTROLS:
      dispatch_state_ = DispatchState::kHil;
      HandleHilMessage(msg);
      break;

    case MAVLINK_MSG_ID_SERIAL_CONTROL:
      dispatch_state_ = DispatchState::kConsole;
      HandleConsoleMessage(msg);
      break;

    default:
      dispatch_state_ = DispatchState::kUnknown;
      HandleUnknownMessage(msg);
      break;
  }

  dispatch_state_ = DispatchState::kIdle;
}

} // namespace iFly
