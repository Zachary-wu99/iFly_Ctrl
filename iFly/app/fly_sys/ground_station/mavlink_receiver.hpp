/**
 * @file mavlink_receiver.hpp
 * @brief MAVLink 接收分发接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP

#include "mavlink_link.hpp"
#include "mavlink_parameter_service.hpp"
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
  explicit MavlinkReceiver(MavlinkLink *link = nullptr);

  /**
   * @brief 绑定 MAVLink 字节流链路。
   *
   * @param link MAVLink 字节流链路。
   */
  void BindLink(MavlinkLink *link);

  /**
   * @brief 读取一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息输出。
   * @return 读取到有效消息返回 `true`。
   */
  bool ReceiveMessage(mavlink_message_t *msg);

  /**
   * @brief 轮询接收并分发一帧 MAVLink 消息。
   *
   * @return 接收到有效消息返回 `true`。
   */
  bool Poll();

  /**
   * @brief 分发 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void DispatchMessage(const mavlink_message_t &msg);

  /**
   * @brief 解码长命令请求。
   *
   * @param msg MAVLink 消息。
   * @param state 长命令请求输出。
   * @return 解码成功返回 `true`。
   */
  bool DecodeCommandRequest(const mavlink_message_t &msg,
                            CommandRequest *state);

  /**
   * @brief 解码手动控制输入。
   *
   * @param msg MAVLink 消息。
   * @param state 手动控制输入输出。
   * @return 解码成功返回 `true`。
   */
  bool DecodeManualControl(const mavlink_message_t &msg, ManualControl *state);

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

  void HandleHeartbeatMessage(const mavlink_message_t &msg);
  void HandlePingMessage(const mavlink_message_t &msg);
  void HandleTimesyncMessage(const mavlink_message_t &msg);
  void HandleModeMessage(const mavlink_message_t &msg);
  void HandleCommandRequestMessage(const mavlink_message_t &msg);
  void HandleCommandIntMessage(const mavlink_message_t &msg);
  void HandleCommandAckMessage(const mavlink_message_t &msg);
  void HandleCommandCancelMessage(const mavlink_message_t &msg);
  void HandleManualControlMessage(const mavlink_message_t &msg);
  void HandleRcOverrideMessage(const mavlink_message_t &msg);
  void HandleGuidedControlMessage(const mavlink_message_t &msg);
  void HandleHomePositionMessage(const mavlink_message_t &msg);
  void HandleFollowTargetMessage(const mavlink_message_t &msg);
  void HandleLandingTargetMessage(const mavlink_message_t &msg);
  void HandleDataStreamRequestMessage(const mavlink_message_t &msg);
  void HandleParameterReadMessage(const mavlink_message_t &msg);
  void HandleParameterListMessage(const mavlink_message_t &msg);
  void HandleParameterSetMessage(const mavlink_message_t &msg);
  void HandleParameterMessage(const mavlink_message_t &msg);
  void HandleExtendedParameterMessage(const mavlink_message_t &msg);
  void HandleMissionMessage(const mavlink_message_t &msg);
  void HandleFileTransferMessage(const mavlink_message_t &msg);
  void HandleLogMessage(const mavlink_message_t &msg);
  void HandleGpsInputMessage(const mavlink_message_t &msg);
  void HandleHilMessage(const mavlink_message_t &msg);
  void HandleCommandRequest(const CommandRequest &state);
  void HandleManualControl(const ManualControl &state);
  void HandleConsoleMessage(const mavlink_message_t &msg);
  void HandleUnknownMessage(const mavlink_message_t &msg);

  /**
   * @brief 判断消息目标是否为本机。
   *
   * @param target_system 目标系统 ID。
   * @param target_component 目标组件 ID。
   * @return 目标匹配返回 `true`。
   */
  static bool IsTargetMatched(uint8_t target_system, uint8_t target_component);

  /**
   * @brief 拷贝 MAVLink 参数名。
   *
   * @param source MAVLink 参数名字段。
   * @param output 输出缓冲区。
   * @param output_size 输出缓冲区大小。
   */
  static void CopyParamId(const char *source,
                          char *output,
                          uint32_t output_size);

  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */
  static constexpr uint8_t kMavlinkParamIdLength = 16U; /**< MAVLink 参数名长度。 */

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
  MavlinkParameterService parameter_service_ {}; /**< MAVLink 参数适配服务。 */
  DispatchState dispatch_state_ = DispatchState::kIdle; /**< 当前分发状态。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_RECEIVER_HPP */
