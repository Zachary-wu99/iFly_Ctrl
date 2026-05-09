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
