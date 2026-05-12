/**
 * @file mavlink_receiver.hpp
 * @brief MAVLink 接收分发接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP

#include "mavlink_link.hpp"
#include "mavlink_send.hpp"
#include "project_parameter_manager.hpp"
#include "sys_state_type.hpp"

namespace iFly {

/**
 * @brief MAVLink 接收分发服务。
 */
class MavlinkReceiver final {
public:
  /**
   * @brief 构造 MAVLink 接收分发服务对象。
   *
   * @param link MAVLink 字节流链路。
   */
  explicit MavlinkReceiver(MavlinkLink *link = nullptr)
      : link_(link),
        send_(link)
  {
  }

  /**
   * @brief 绑定 MAVLink 字节流链路。
   *
   * @param link MAVLink 字节流链路。
   */
  void BindLink(MavlinkLink *link)
  {
    link_ = link;
    send_.BindLink(link);
  }

  /**
   * @brief 读取一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息输出。
   * @return 读取到有效消息返回 `true`。
   */
  bool ReceiveMessage(mavlink_message_t *msg)
  {
    if (link_ == nullptr) {
      return false;
    }

    return link_->ReceiveMessage(msg);
  }

  /**
   * @brief 轮询接收并分发一帧 MAVLink 消息。
   *
   * @return 接收到有效消息返回 `true`。
   */
  bool Poll()
  {
    mavlink_message_t msg {};
    if (!ReceiveMessage(&msg)) {
      return false;
    }

    DispatchMessage(msg);
    return true;
  }

  /**
   * @brief 分发 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void DispatchMessage(const mavlink_message_t &msg)
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

  /**
   * @brief 解码长命令请求。
   *
   * @param msg MAVLink 消息。
   * @param state 长命令请求输出。
   * @return 解码成功返回 `true`。
   */
  bool DecodeCommandRequest(const mavlink_message_t &msg,
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

  /**
   * @brief 解码手动控制输入。
   *
   * @param msg MAVLink 消息。
   * @param state 手动控制输入输出。
   * @return 解码成功返回 `true`。
   */
  bool DecodeManualControl(const mavlink_message_t &msg, ManualControl *state)
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

private:
  enum class DispatchState : uint8_t {
    kIdle = 0U, /**< 空闲。 */
    kHeartbeat, /**< 分发心跳消息。 */
    kPing, /**< 分发链路探测消息。 */
    kTimesync, /**< 分发时间同步消息。 */
    kMode, /**< 分发模式设置消息。 */
    kCommandRequest, /**< 分发长命令请求。 */
    kCommandIntRequest, /**< 分发整型命令请求。 */
    kCommandAck, /**< 分发命令响应。 */
    kCommandCancel, /**< 分发命令取消。 */
    kManualControl, /**< 分发手动控制输入。 */
    kRcOverride, /**< 分发 RC 覆盖输入。 */
    kGuidedControl, /**< 分发引导控制输入。 */
    kHomePosition, /**< 分发起飞点设置消息。 */
    kFollowTarget, /**< 分发跟随目标消息。 */
    kLandingTarget, /**< 分发降落目标消息。 */
    kDataStreamRequest, /**< 分发数据流请求。 */
    kParameter, /**< 分发参数消息。 */
    kParameterRead, /**< 分发参数读取消息。 */
    kParameterList, /**< 分发参数列表消息。 */
    kParameterSet, /**< 分发参数设置消息。 */
    kExtendedParameter, /**< 分发扩展参数消息。 */
    kMission, /**< 分发任务消息。 */
    kFileTransfer, /**< 分发文件传输消息。 */
    kLog, /**< 分发日志消息。 */
    kGpsInput, /**< 分发 GPS 输入消息。 */
    kHil, /**< 分发硬件在环消息。 */
    kConsole, /**< 分发控制台消息。 */
    kUnknown /**< 分发未知消息。 */
  };

  /**
   * @brief 处理心跳消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleHeartbeatMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理链路探测消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandlePingMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理时间同步消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleTimesyncMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理模式设置消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleModeMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 分发长命令请求消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleCommandRequestMessage(const mavlink_message_t &msg)
  {
    CommandRequest state {};
    if (DecodeCommandRequest(msg, &state)) {
      HandleCommandRequest(state);
    }
  }

  /**
   * @brief 处理整型命令请求消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleCommandIntMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理命令响应消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleCommandAckMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理命令取消消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleCommandCancelMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 分发手动控制输入消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleManualControlMessage(const mavlink_message_t &msg)
  {
    ManualControl state {};
    if (DecodeManualControl(msg, &state)) {
      HandleManualControl(state);
    }
  }

  /**
   * @brief 处理 RC 覆盖输入消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleRcOverrideMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理引导控制输入消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleGuidedControlMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理起飞点设置消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleHomePositionMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理跟随目标消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleFollowTargetMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理降落目标消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleLandingTargetMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理数据流请求消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleDataStreamRequestMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理参数读取消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleParameterReadMessage(const mavlink_message_t &msg)
  {
    mavlink_param_request_read_t request {};
    mavlink_msg_param_request_read_decode(&msg, &request);
    if (!IsTargetMatched(request.target_system, request.target_component)) {
      return;
    }

    if (request.param_index >= 0) {
      SendParameterByIndex(static_cast<uint16_t>(request.param_index));
      return;
    }

    char name[kMavlinkParamIdLength + 1U] {};
    CopyParamId(request.param_id, name, sizeof(name));
    SendParameterByName(name);
  }

  /**
   * @brief 处理参数列表消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleParameterListMessage(const mavlink_message_t &msg)
  {
    mavlink_param_request_list_t request {};
    mavlink_msg_param_request_list_decode(&msg, &request);
    if (!IsTargetMatched(request.target_system, request.target_component)) {
      return;
    }

    ProjectParameterManager &parameters = ProjectParameterManager::Instance();
    const uint16_t count = parameters.MavlinkCount();
    for (uint16_t index = 0U; index < count; ++index) {
      SendParameterByIndex(index);
    }
  }

  /**
   * @brief 处理参数设置消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleParameterSetMessage(const mavlink_message_t &msg)
  {
    mavlink_param_set_t request {};
    mavlink_msg_param_set_decode(&msg, &request);
    if (!IsTargetMatched(request.target_system, request.target_component)) {
      return;
    }

    char name[kMavlinkParamIdLength + 1U] {};
    CopyParamId(request.param_id, name, sizeof(name));

    ProjectParameterManager &parameters = ProjectParameterManager::Instance();
    (void)parameters.WriteMavlinkValue(name,
                                       request.param_value,
                                       ToProjectParameterType(request.param_type));
    SendParameterByName(name);
  }

  /**
   * @brief 处理参数消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleParameterMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理扩展参数消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleExtendedParameterMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理任务消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleMissionMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理文件传输消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleFileTransferMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理日志消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleLogMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理 GPS 输入消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleGpsInputMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理硬件在环消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleHilMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理长命令请求。
   *
   * @param state 长命令请求。
   */
  void HandleCommandRequest(const CommandRequest &state)
  {
    (void)state;
  }

  /**
   * @brief 处理手动控制输入。
   *
   * @param state 手动控制输入。
   */
  void HandleManualControl(const ManualControl &state)
  {
    (void)state;
  }

  /**
   * @brief 处理控制台消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleConsoleMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 处理未知消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleUnknownMessage(const mavlink_message_t &msg)
  {
    (void)msg;
  }

  /**
   * @brief 判断消息目标是否为本机。
   *
   * @param target_system 目标系统 ID。
   * @param target_component 目标组件 ID。
   * @return 目标匹配返回 `true`。
   */
  static bool IsTargetMatched(uint8_t target_system, uint8_t target_component)
  {
    return ((target_system == 0U) || (target_system == kSystemId)) &&
           ((target_component == 0U) || (target_component == kComponentId));
  }

  /**
   * @brief 拷贝 MAVLink 参数名。
   *
   * @param source MAVLink 参数名字段。
   * @param output 输出缓冲区。
   * @param output_size 输出缓冲区大小。
   */
  static void CopyParamId(const char *source, char *output, uint32_t output_size)
  {
    if ((source == nullptr) || (output == nullptr) || (output_size == 0U)) {
      return;
    }

    uint32_t index = 0U;
    while ((index < kMavlinkParamIdLength) &&
           ((index + 1U) < output_size) &&
           (source[index] != '\0')) {
      output[index] = source[index];
      ++index;
    }

    output[index] = '\0';
  }

  /**
   * @brief 转换 MAVLink 参数类型到工程参数类型。
   *
   * @param type MAVLink 参数类型。
   * @return 工程参数类型。
   */
  static ProjectParameterType ToProjectParameterType(uint8_t type)
  {
    switch (type) {
      case MAV_PARAM_TYPE_UINT8:
        return ProjectParameterType::kUint8;

      case MAV_PARAM_TYPE_UINT16:
        return ProjectParameterType::kUint16;

      case MAV_PARAM_TYPE_UINT32:
        return ProjectParameterType::kUint32;

      case MAV_PARAM_TYPE_INT32:
        return ProjectParameterType::kInt32;

      case MAV_PARAM_TYPE_REAL32:
        return ProjectParameterType::kFloat;

      default:
        return ProjectParameterType::kBytes;
    }
  }

  /**
   * @brief 按索引发送参数。
   *
   * @param index MAVLink 参数索引。
   */
  void SendParameterByIndex(uint16_t index)
  {
    ProjectParameterManager &parameters = ProjectParameterManager::Instance();
    const ProjectParameterManager::EntryView *parameter = parameters.MavlinkAt(index);
    if (parameter == nullptr) {
      return;
    }

    send_.SendParameterValue(*parameter, parameters.MavlinkCount(), index);
  }

  /**
   * @brief 按名称发送参数。
   *
   * @param name MAVLink 参数名。
   */
  void SendParameterByName(const char *name)
  {
    ProjectParameterManager &parameters = ProjectParameterManager::Instance();
    const int16_t index = parameters.MavlinkIndexOf(name);
    if (index < 0) {
      return;
    }

    SendParameterByIndex(static_cast<uint16_t>(index));
  }

  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */
  static constexpr uint8_t kMavlinkParamIdLength = 16U; /**< MAVLink 参数名长度。 */

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
  MavlinkSend send_ {}; /**< MAVLink 发送服务。 */
  DispatchState dispatch_state_ = DispatchState::kIdle; /**< 当前分发状态。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP */
