/**
 * @file mavlink_send.hpp
 * @brief MAVLink 状态发送接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP

#include "mavlink_link.hpp"
#include "sys_state_type.hpp"

namespace iFly {

/**
 * @brief MAVLink 状态发送服务。
 */
class MavlinkSend final {
public:
  /**
   * @brief 构造 MAVLink 发送服务对象。
   *
   * @param link MAVLink 字节流链路。
   */
  explicit MavlinkSend(MavlinkLink *link = nullptr)
      : link_(link)
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
  }

  /**
   * @brief 发送心跳状态。
   *
   * @param state 心跳状态。
   */
  void SendHeartbeat(const Heartbeat &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_heartbeat_pack(kSystemId,
                                     kComponentId,
                                     &msg,
                                     static_cast<uint8_t>(state.type),
                                     static_cast<uint8_t>(state.autopilot),
                                     state.base_mode,
                                     state.custom_mode,
                                     static_cast<uint8_t>(state.state));
    SendMessage(msg);
  }

  /**
   * @brief 发送系统健康状态。
   *
   * @param state 系统健康状态。
   */
  void SendSystemStatus(const SystemStatus &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_sys_status_pack(kSystemId,
                                      kComponentId,
                                      &msg,
                                      state.sensors_present,
                                      state.sensors_enabled,
                                      state.sensors_health,
                                      state.load,
                                      state.battery_voltage,
                                      state.battery_current,
                                      state.battery_remaining,
                                      state.comm_drop_rate,
                                      state.comm_errors,
                                      0U,
                                      0U,
                                      0U,
                                      0U,
                                      0U,
                                      0U,
                                      0U);
    SendMessage(msg);
  }

  /**
   * @brief 发送电池状态。
   *
   * @param state 电池状态。
   */
  void SendBatteryStatus(const BatteryStatus &state)
  {
    uint16_t voltage_ext[4] {};
    mavlink_message_t msg {};
    (void)mavlink_msg_battery_status_pack(kSystemId,
                                          kComponentId,
                                          &msg,
                                          state.id,
                                          0U,
                                          0U,
                                          state.temperature,
                                          state.cell_voltage,
                                          state.current,
                                          -1,
                                          -1,
                                          state.remaining,
                                          0,
                                          0U,
                                          voltage_ext,
                                          0U,
                                          0U);
    SendMessage(msg);
  }

  /**
   * @brief 发送 GPS 状态。
   *
   * @param state GPS 状态。
   */
  void SendGpsStatus(const GpsStatus &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_gps_raw_int_pack(kSystemId,
                                       kComponentId,
                                       &msg,
                                       state.time_usec,
                                       static_cast<uint8_t>(state.fix_type),
                                       state.latitude,
                                       state.longitude,
                                       state.altitude,
                                       state.horizontal_dop,
                                       state.vertical_dop,
                                       state.ground_speed,
                                       state.course_over_ground,
                                       state.satellites_visible,
                                       0,
                                       0U,
                                       0U,
                                       0U,
                                       0U,
                                       0U);
    SendMessage(msg);
  }

  /**
   * @brief 发送全局位置状态。
   *
   * @param state 全局位置状态。
   */
  void SendGlobalPosition(const GlobalPosition &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_global_position_int_pack(kSystemId,
                                               kComponentId,
                                               &msg,
                                               state.time_boot_ms,
                                               state.latitude,
                                               state.longitude,
                                               state.altitude,
                                               state.relative_altitude,
                                               state.velocity_north,
                                               state.velocity_east,
                                               state.velocity_down,
                                               state.heading);
    SendMessage(msg);
  }

  /**
   * @brief 发送本地 NED 位置状态。
   *
   * @param state 本地 NED 位置状态。
   */
  void SendLocalPosition(const LocalPosition &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_local_position_ned_pack(kSystemId,
                                              kComponentId,
                                              &msg,
                                              state.time_boot_ms,
                                              state.north,
                                              state.east,
                                              state.down,
                                              state.velocity_north,
                                              state.velocity_east,
                                              state.velocity_down);
    SendMessage(msg);
  }

  /**
   * @brief 发送姿态状态。
   *
   * @param state 姿态状态。
   */
  void SendAttitude(const Attitude &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_attitude_pack(kSystemId,
                                    kComponentId,
                                    &msg,
                                    state.time_boot_ms,
                                    state.roll,
                                    state.pitch,
                                    state.yaw,
                                    state.roll_rate,
                                    state.pitch_rate,
                                    state.yaw_rate);
    SendMessage(msg);
  }

  /**
   * @brief 发送飞行仪表状态。
   *
   * @param state 飞行仪表状态。
   */
  void SendFlightHud(const FlightHud &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_vfr_hud_pack(kSystemId,
                                   kComponentId,
                                   &msg,
                                   state.airspeed,
                                   state.groundspeed,
                                   state.heading,
                                   state.throttle,
                                   state.altitude,
                                   state.climb);
    SendMessage(msg);
  }

  /**
   * @brief 发送 RC 通道状态。
   *
   * @param state RC 通道状态。
   */
  void SendRcChannels(const RcChannels &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_rc_channels_pack(kSystemId,
                                       kComponentId,
                                       &msg,
                                       state.time_boot_ms,
                                       state.count,
                                       state.channel[0],
                                       state.channel[1],
                                       state.channel[2],
                                       state.channel[3],
                                       state.channel[4],
                                       state.channel[5],
                                       state.channel[6],
                                       state.channel[7],
                                       state.channel[8],
                                       state.channel[9],
                                       state.channel[10],
                                       state.channel[11],
                                       state.channel[12],
                                       state.channel[13],
                                       state.channel[14],
                                       state.channel[15],
                                       state.channel[16],
                                       state.channel[17],
                                       state.rssi);
    SendMessage(msg);
  }

  /**
   * @brief 发送输出通道状态。
   *
   * @param state 输出通道状态。
   */
  void SendOutputStatus(const OutputStatus &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_servo_output_raw_pack(kSystemId,
                                            kComponentId,
                                            &msg,
                                            state.time_usec,
                                            state.port,
                                            state.output[0],
                                            state.output[1],
                                            state.output[2],
                                            state.output[3],
                                            state.output[4],
                                            state.output[5],
                                            state.output[6],
                                            state.output[7],
                                            state.output[8],
                                            state.output[9],
                                            state.output[10],
                                            state.output[11],
                                            state.output[12],
                                            state.output[13],
                                            state.output[14],
                                            state.output[15]);
    SendMessage(msg);
  }

  /**
   * @brief 发送扩展运行状态。
   *
   * @param state 扩展运行状态。
   */
  void SendExtendedState(const ExtendedState &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_extended_sys_state_pack(kSystemId,
                                              kComponentId,
                                              &msg,
                                              state.vtol_state,
                                              static_cast<uint8_t>(state.landed_state));
    SendMessage(msg);
  }

  /**
   * @brief 发送状态文本。
   *
   * @param state 状态文本。
   */
  void SendStatusText(const StatusText &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_statustext_pack(kSystemId,
                                      kComponentId,
                                      &msg,
                                      static_cast<uint8_t>(state.severity),
                                      state.text,
                                      state.id,
                                      state.chunk_seq);
    SendMessage(msg);
  }

  /**
   * @brief 发送长命令请求。
   *
   * @param state 长命令请求。
   */
  void SendCommandRequest(const CommandRequest &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_command_long_pack(kSystemId,
                                        kComponentId,
                                        &msg,
                                        state.target_system,
                                        state.target_component,
                                        state.command,
                                        state.confirmation,
                                        state.argument[0],
                                        state.argument[1],
                                        state.argument[2],
                                        state.argument[3],
                                        state.argument[4],
                                        state.argument[5],
                                        state.argument[6]);
    SendMessage(msg);
  }

  /**
   * @brief 发送命令响应。
   *
   * @param state 命令响应。
   */
  void SendCommandResponse(const CommandResponse &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_command_ack_pack(kSystemId,
                                       kComponentId,
                                       &msg,
                                       state.command,
                                       static_cast<uint8_t>(state.result),
                                       state.progress,
                                       state.result_detail,
                                       state.target_system,
                                       state.target_component);
    SendMessage(msg);
  }

  /**
   * @brief 发送手动控制输入。
   *
   * @param state 手动控制输入。
   */
  void SendManualControl(const ManualControl &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_manual_control_pack(kSystemId,
                                          kComponentId,
                                          &msg,
                                          state.target,
                                          state.x,
                                          state.y,
                                          state.z,
                                          state.r,
                                          state.buttons,
                                          state.buttons2,
                                          0U,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0);
    SendMessage(msg);
  }

  /**
   * @brief 发送固件版本信息。
   *
   * @param state 固件版本信息。
   */
  void SendVersionInfo(const VersionInfo &state)
  {
    mavlink_message_t msg {};
    (void)mavlink_msg_autopilot_version_pack(kSystemId,
                                             kComponentId,
                                             &msg,
                                             state.capabilities,
                                             state.flight_software_version,
                                             state.middleware_software_version,
                                             state.os_software_version,
                                             state.board_version,
                                             state.flight_version_hash,
                                             state.middleware_version_hash,
                                             state.os_version_hash,
                                             state.vendor_id,
                                             state.product_id,
                                             state.unique_id,
                                             state.unique_id_ext);
    SendMessage(msg);
  }

private:
  /**
   * @brief 发送一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void SendMessage(const mavlink_message_t &msg)
  {
    if (link_ == nullptr) {
      return;
    }

    link_->SendMessage(msg);
  }

  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP */
