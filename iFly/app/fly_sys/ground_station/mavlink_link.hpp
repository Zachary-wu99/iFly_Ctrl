/**
 * @file mavlink_link.hpp
 * @brief MAVLink 字节流链路接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_LINK_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_LINK_HPP

#include <stdint.h>
#include <string.h>

#include "common/mavlink.h"
#include "serial_io_base.hpp"

namespace iFly {

/**
 * @brief 运行在统一串行 IO 上的 MAVLink 链路。
 */
class MavlinkLink final {
public:
  /**
   * @brief 构造 MAVLink 链路对象。
   *
   * @param io 底层串行 IO 对象。
   */
  explicit MavlinkLink(SerialIoBase *io = nullptr);

  /**
   * @brief 绑定底层串行 IO 对象。
   *
   * @param io 底层串行 IO 对象。
   */
  void BindIo(SerialIoBase *io);

  /**
   * @brief 读取一帧已经解析完成的 MAVLink 消息。
   *
   * @param msg MAVLink 消息输出。
   * @return 读取到有效消息返回 `true`。
   */
  bool ReceiveMessage(mavlink_message_t *msg);

  /**
   * @brief 发送一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void SendMessage(const mavlink_message_t &msg);

  /**
   * @brief 打包 MAVLink HEARTBEAT 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param heartbeat HEARTBEAT 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackHeartbeat(mavlink_message_t *msg,
                         const mavlink_heartbeat_t &heartbeat) const;

  /**
   * @brief 打包 MAVLink SYS_STATUS 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param status SYS_STATUS 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackSystemStatus(mavlink_message_t *msg,
                            const mavlink_sys_status_t &status) const;

  /**
   * @brief 打包 MAVLink BATTERY_STATUS 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param status BATTERY_STATUS 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackBatteryStatus(mavlink_message_t *msg,
                             const mavlink_battery_status_t &status) const;

  /**
   * @brief 打包 MAVLink GPS_RAW_INT 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param status GPS_RAW_INT 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackGpsRawInt(mavlink_message_t *msg,
                         const mavlink_gps_raw_int_t &status) const;

  /**
   * @brief 打包 MAVLink GLOBAL_POSITION_INT 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param position GLOBAL_POSITION_INT 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackGlobalPositionInt(
      mavlink_message_t *msg,
      const mavlink_global_position_int_t &position) const;

  /**
   * @brief 打包 MAVLink LOCAL_POSITION_NED 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param position LOCAL_POSITION_NED 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackLocalPositionNed(
      mavlink_message_t *msg,
      const mavlink_local_position_ned_t &position) const;

  /**
   * @brief 打包 MAVLink ATTITUDE 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param attitude ATTITUDE 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackAttitude(mavlink_message_t *msg,
                        const mavlink_attitude_t &attitude) const;

  /**
   * @brief 打包 MAVLink VFR_HUD 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param hud VFR_HUD 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackVfrHud(mavlink_message_t *msg,
                      const mavlink_vfr_hud_t &hud) const;

  /**
   * @brief 打包 MAVLink RC_CHANNELS 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param channels RC_CHANNELS 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackRcChannels(mavlink_message_t *msg,
                          const mavlink_rc_channels_t &channels) const;

  /**
   * @brief 打包 MAVLink SERVO_OUTPUT_RAW 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param output SERVO_OUTPUT_RAW 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackServoOutputRaw(
      mavlink_message_t *msg,
      const mavlink_servo_output_raw_t &output) const;

  /**
   * @brief 打包 MAVLink EXTENDED_SYS_STATE 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param state EXTENDED_SYS_STATE 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackExtendedSysState(
      mavlink_message_t *msg,
      const mavlink_extended_sys_state_t &state) const;

  /**
   * @brief 打包 MAVLink STATUSTEXT 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param text STATUSTEXT 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackStatusText(mavlink_message_t *msg,
                          const mavlink_statustext_t &text) const;

  /**
   * @brief 打包 MAVLink COMMAND_LONG 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param command COMMAND_LONG 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackCommandLong(mavlink_message_t *msg,
                           const mavlink_command_long_t &command) const;

  /**
   * @brief 打包 MAVLink COMMAND_ACK 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param ack COMMAND_ACK 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackCommandAck(mavlink_message_t *msg,
                          const mavlink_command_ack_t &ack) const;

  /**
   * @brief 打包 MAVLink MANUAL_CONTROL 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param control MANUAL_CONTROL 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackManualControl(mavlink_message_t *msg,
                             const mavlink_manual_control_t &control) const;

  /**
   * @brief 打包 MAVLink AUTOPILOT_VERSION 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param version AUTOPILOT_VERSION 消息负载。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackAutopilotVersion(
      mavlink_message_t *msg,
      const mavlink_autopilot_version_t &version) const;

  /**
   * @brief 打包 MAVLink PARAM_VALUE 消息。
   *
   * @param msg MAVLink 消息输出。
   * @param param_id MAVLink 参数名。
   * @param param_value MAVLink 参数值。
   * @param param_type MAVLink 参数类型。
   * @param param_count MAVLink 参数总数。
   * @param param_index 当前 MAVLink 参数索引。
   * @return MAVLink 消息负载长度。
   */
  uint16_t PackParameterValue(mavlink_message_t *msg,
                              const char *param_id,
                              float param_value,
                              uint8_t param_type,
                              uint16_t param_count,
                              uint16_t param_index) const;

  /**
   * @brief 解码 MAVLink 控制台消息。
   *
   * @param msg MAVLink 消息。
   * @param control 控制台消息输出。
   * @return 控制台消息返回 `true`。
   */
  bool DecodeConsoleMessage(const mavlink_message_t &msg,
                            mavlink_serial_control_t *control);

  /**
   * @brief 发送 MAVLink 控制台输出。
   *
   * @param data 控制台输出数据。
   * @param length 控制台输出字节数。
   * @return 实际发送字节数。
   */
  uint32_t SendConsoleOutput(const uint8_t *data, uint32_t length);

  /**
   * @brief 发送一帧 MAVLink 控制台回包。
   *
   * @param data 控制台输出数据。
   * @param len 控制台输出字节数。
   * @param flags SERIAL_CONTROL 标志位。
   */
  void SendConsoleReply(const uint8_t *data, uint8_t len, uint8_t flags);

private:
  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */

  SerialIoBase *io_ = nullptr; /**< 底层串行 IO 对象。 */
  mavlink_status_t rx_status_ {}; /**< MAVLink 接收解析状态。 */
  uint8_t console_target_system_ = 0U; /**< 控制台对端系统 ID。 */
  uint8_t console_target_component_ = 0U; /**< 控制台对端组件 ID。 */
  bool console_target_valid_ = false; /**< 控制台对端是否有效。 */
};

inline MavlinkLink::MavlinkLink(SerialIoBase *io)
    : io_(io)
{
}

inline void MavlinkLink::BindIo(SerialIoBase *io)
{
  io_ = io;
}

inline bool MavlinkLink::ReceiveMessage(mavlink_message_t *msg)
{
  if ((io_ == nullptr) || !io_->IsConnected() || (msg == nullptr)) {
    return false;
  }

  uint8_t rx_byte = 0U;
  while (io_->Read(&rx_byte, 1U) == 1U) {
    if (mavlink_parse_char(MAVLINK_COMM_0, rx_byte, msg, &rx_status_) != 0U) {
      return true;
    }
  }

  return false;
}

inline void MavlinkLink::SendMessage(const mavlink_message_t &msg)
{
  if (io_ == nullptr) {
    return;
  }

  uint8_t tx_buffer[MAVLINK_MAX_PACKET_LEN] {};
  const uint16_t tx_length = mavlink_msg_to_send_buffer(tx_buffer, &msg);
  (void)io_->Write(tx_buffer, tx_length);
}

inline uint16_t MavlinkLink::PackHeartbeat(
    mavlink_message_t *msg,
    const mavlink_heartbeat_t &heartbeat) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_heartbeat_encode(kSystemId, kComponentId, msg, &heartbeat);
}

inline uint16_t MavlinkLink::PackSystemStatus(
    mavlink_message_t *msg,
    const mavlink_sys_status_t &status) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_sys_status_encode(kSystemId, kComponentId, msg, &status);
}

inline uint16_t MavlinkLink::PackBatteryStatus(
    mavlink_message_t *msg,
    const mavlink_battery_status_t &status) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_battery_status_encode(kSystemId,
                                           kComponentId,
                                           msg,
                                           &status);
}

inline uint16_t MavlinkLink::PackGpsRawInt(
    mavlink_message_t *msg,
    const mavlink_gps_raw_int_t &status) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_gps_raw_int_encode(kSystemId, kComponentId, msg, &status);
}

inline uint16_t MavlinkLink::PackGlobalPositionInt(
    mavlink_message_t *msg,
    const mavlink_global_position_int_t &position) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_global_position_int_encode(kSystemId,
                                                kComponentId,
                                                msg,
                                                &position);
}

inline uint16_t MavlinkLink::PackLocalPositionNed(
    mavlink_message_t *msg,
    const mavlink_local_position_ned_t &position) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_local_position_ned_encode(kSystemId,
                                               kComponentId,
                                               msg,
                                               &position);
}

inline uint16_t MavlinkLink::PackAttitude(
    mavlink_message_t *msg,
    const mavlink_attitude_t &attitude) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_attitude_encode(kSystemId, kComponentId, msg, &attitude);
}

inline uint16_t MavlinkLink::PackVfrHud(mavlink_message_t *msg,
                                        const mavlink_vfr_hud_t &hud) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_vfr_hud_encode(kSystemId, kComponentId, msg, &hud);
}

inline uint16_t MavlinkLink::PackRcChannels(
    mavlink_message_t *msg,
    const mavlink_rc_channels_t &channels) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_rc_channels_encode(kSystemId,
                                        kComponentId,
                                        msg,
                                        &channels);
}

inline uint16_t MavlinkLink::PackServoOutputRaw(
    mavlink_message_t *msg,
    const mavlink_servo_output_raw_t &output) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_servo_output_raw_encode(kSystemId,
                                             kComponentId,
                                             msg,
                                             &output);
}

inline uint16_t MavlinkLink::PackExtendedSysState(
    mavlink_message_t *msg,
    const mavlink_extended_sys_state_t &state) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_extended_sys_state_encode(kSystemId,
                                               kComponentId,
                                               msg,
                                               &state);
}

inline uint16_t MavlinkLink::PackStatusText(
    mavlink_message_t *msg,
    const mavlink_statustext_t &text) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_statustext_encode(kSystemId, kComponentId, msg, &text);
}

inline uint16_t MavlinkLink::PackCommandLong(
    mavlink_message_t *msg,
    const mavlink_command_long_t &command) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_command_long_encode(kSystemId,
                                         kComponentId,
                                         msg,
                                         &command);
}

inline uint16_t MavlinkLink::PackCommandAck(
    mavlink_message_t *msg,
    const mavlink_command_ack_t &ack) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_command_ack_encode(kSystemId, kComponentId, msg, &ack);
}

inline uint16_t MavlinkLink::PackManualControl(
    mavlink_message_t *msg,
    const mavlink_manual_control_t &control) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_manual_control_encode(kSystemId,
                                           kComponentId,
                                           msg,
                                           &control);
}

inline uint16_t MavlinkLink::PackAutopilotVersion(
    mavlink_message_t *msg,
    const mavlink_autopilot_version_t &version) const
{
  if (msg == nullptr) {
    return 0U;
  }

  return mavlink_msg_autopilot_version_encode(kSystemId,
                                              kComponentId,
                                              msg,
                                              &version);
}

inline uint16_t MavlinkLink::PackParameterValue(mavlink_message_t *msg,
                                                const char *param_id,
                                                float param_value,
                                                uint8_t param_type,
                                                uint16_t param_count,
                                                uint16_t param_index) const
{
  if ((msg == nullptr) || (param_id == nullptr)) {
    return 0U;
  }

  return mavlink_msg_param_value_pack(kSystemId,
                                      kComponentId,
                                      msg,
                                      param_id,
                                      param_value,
                                      param_type,
                                      param_count,
                                      param_index);
}

inline bool MavlinkLink::DecodeConsoleMessage(
    const mavlink_message_t &msg,
    mavlink_serial_control_t *control)
{
  if ((msg.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL) || (control == nullptr)) {
    return false;
  }

  mavlink_msg_serial_control_decode(&msg, control);

  if (control->device != SERIAL_CONTROL_DEV_SHELL) {
    return false;
  }

  if ((control->target_system != 0U) &&
      (control->target_system != kSystemId)) {
    return false;
  }

  if ((control->target_component != 0U) &&
      (control->target_component != kComponentId)) {
    return false;
  }

  console_target_system_ = msg.sysid;
  console_target_component_ = msg.compid;
  console_target_valid_ = true;

  if (control->count > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
    control->count = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
  }

  return true;
}

inline uint32_t MavlinkLink::SendConsoleOutput(const uint8_t *data,
                                               uint32_t length)
{
  if ((data == nullptr) || (length == 0U) || !console_target_valid_) {
    return 0U;
  }

  uint32_t offset = 0U;
  while (offset < length) {
    uint32_t chunk_length = length - offset;
    if (chunk_length > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
      chunk_length = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
    }

    uint8_t flags = SERIAL_CONTROL_FLAG_REPLY;
    if ((offset + chunk_length) < length) {
      flags |= SERIAL_CONTROL_FLAG_MULTI;
    }

    SendConsoleReply(data + offset, static_cast<uint8_t>(chunk_length), flags);
    offset += chunk_length;
  }

  return offset;
}

inline void MavlinkLink::SendConsoleReply(const uint8_t *data,
                                          uint8_t len,
                                          uint8_t flags)
{
  if (!console_target_valid_) {
    return;
  }

  if (len > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
    len = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
  }

  uint8_t payload[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN] {};
  if ((data != nullptr) && (len > 0U)) {
    memcpy(payload, data, len);
  }

  mavlink_message_t msg {};
  (void)mavlink_msg_serial_control_pack(kSystemId,
                                        kComponentId,
                                        &msg,
                                        SERIAL_CONTROL_DEV_SHELL,
                                        flags,
                                        0U,
                                        0U,
                                        len,
                                        payload,
                                        console_target_system_,
                                        console_target_component_);
  SendMessage(msg);
}

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_LINK_HPP */

