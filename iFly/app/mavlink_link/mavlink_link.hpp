/**
 * @file mavlink_link.hpp
 * @brief MAVLink 字节流链路接口。
 */
#ifndef IFLY_APP_MAVLINK_LINK_HPP
#define IFLY_APP_MAVLINK_LINK_HPP

#include <stdint.h>

#include "common/mavlink.h"
#include "serial_io_base.hpp"

namespace iFly {

class FlightCtrlCli;

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
   * @brief 绑定 MAVLink 控制台 CLI。
   *
   * @param console 控制台 CLI 对象。
   */
  void BindConsole(FlightCtrlCli *console);

  /**
   * @brief 轮询驱动 MAVLink 收发。
   */
  void Poll();

private:
  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */
  static constexpr uint32_t kHeartbeatPeriodMs = 1000U; /**< 心跳发送周期。 */

  /**
   * @brief 处理已经解析完成的 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleMessage(const mavlink_message_t &msg);

  /**
   * @brief 处理 MAVLink 控制台收发消息。
   *
   * @param msg MAVLink 消息。
   */
  void HandleSerialControl(const mavlink_message_t &msg);

  /**
   * @brief 按固定周期发送 MAVLink 心跳。
   *
   * @param now_ms 当前毫秒时间戳。
   */
  void SendHeartbeat(uint32_t now_ms);

  /**
   * @brief 发送一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void SendMessage(const mavlink_message_t &msg);

  /**
   * @brief 发送 MAVLink 控制台输出。
   *
   * @param data 控制台输出数据。
   * @param length 控制台输出字节数。
   * @return 实际发送字节数。
   */
  uint32_t SendConsoleOutput(const uint8_t *data, uint32_t length);

  /**
   * @brief MAVLink 控制台输出回调。
   *
   * @param context 回调上下文。
   * @param data 控制台输出数据。
   * @param length 控制台输出字节数。
   * @return 实际发送字节数。
   */
  static uint32_t ConsoleOutput(void *context, const uint8_t *data,
                                uint32_t length);

  /**
   * @brief 发送一帧 MAVLink 控制台回包。
   *
   * @param data 控制台输出数据。
   * @param len 控制台输出字节数。
   * @param flags SERIAL_CONTROL 标志位。
   */
  void SendSerialControlReply(const uint8_t *data, uint8_t len, uint8_t flags);

  SerialIoBase *io_ = nullptr; /**< 底层串行 IO 对象。 */
  FlightCtrlCli *console_ = nullptr; /**< MAVLink 控制台 CLI 对象。 */
  mavlink_status_t rx_status_ {}; /**< MAVLink 接收解析状态。 */
  uint32_t last_heartbeat_ms_ = 0U; /**< 上一次心跳发送时间。 */
  uint8_t console_target_system_ = 0U; /**< 控制台对端系统 ID。 */
  uint8_t console_target_component_ = 0U; /**< 控制台对端组件 ID。 */
  bool console_target_valid_ = false; /**< 控制台对端是否有效。 */
  bool console_output_sent_ = false; /**< 本轮控制台是否已经输出。 */
};

} // namespace iFly

#endif /* IFLY_APP_MAVLINK_LINK_HPP */
