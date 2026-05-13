/**
 * @file mavlink_send.hpp
 * @brief MAVLink 状态发送接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP

#include <stdint.h>

#include "mavlink_link.hpp"
#include "mavlink_parameter_service.hpp"
#include "sys_state_type.hpp"

namespace iFly {

/**
 * @brief MAVLink 状态发送服务。
 */
class MavlinkSend final {
public:
  using StreamCallback = void (MavlinkSend::*)(); /**< 周期发送回调函数。 */
  using StreamHandle = uint32_t; /**< 周期发送句柄类型。 */

  static constexpr StreamHandle kInvalidStreamHandle = 0U; /**< 无效周期发送句柄。 */
  static constexpr uint8_t kMaxStreams = 16U; /**< 最大周期发送数量。 */
  static constexpr uint32_t kUseIntervalAsStartDelay =
      0xFFFFFFFFUL; /**< 使用发送周期作为首次发送延时。 */

  /**
   * @brief 周期发送注册配置。
   */
  struct StreamConfig final {
    const char *name = nullptr; /**< 周期发送名称，仅用于调试。 */
    StreamCallback callback = nullptr; /**< 周期发送回调函数。 */
    uint32_t interval_ms = 0U; /**< 发送周期，单位为毫秒。 */
    uint32_t start_delay_ms = 0U; /**< 首次发送延时，单位为毫秒。 */
    bool enabled = true; /**< 注册后是否立即启用。 */
  };

  /**
   * @brief 构造 MAVLink 发送服务对象。
   *
   * @param link MAVLink 字节流链路。
   */
  explicit MavlinkSend(MavlinkLink *link = nullptr);

  /**
   * @brief 绑定 MAVLink 字节流链路。
   *
   * @param link MAVLink 字节流链路。
   */
  void BindLink(MavlinkLink *link);

  /**
   * @brief 注册周期发送回调。
   *
   * @param config 周期发送配置。
   * @return 注册成功返回有效句柄。
   */
  StreamHandle RegisterStream(const StreamConfig &config);

  /**
   * @brief 注册周期发送回调。
   *
   * @param name 周期发送名称。
   * @param callback 周期发送回调函数。
   * @param interval_ms 发送周期，单位为毫秒。
   * @param start_delay_ms 首次发送延时，单位为毫秒。
   * @return 注册成功返回有效句柄。
   */
  StreamHandle RegisterStream(const char *name,
                              StreamCallback callback,
                              uint32_t interval_ms,
                              uint32_t start_delay_ms = 0U);

  /**
   * @brief 注销周期发送回调。
   *
   * @param handle 周期发送句柄。
   * @return 注销成功返回 `true`。
   */
  bool UnregisterStream(StreamHandle handle);

  /**
   * @brief 设置周期发送间隔。
   *
   * @param handle 周期发送句柄。
   * @param interval_ms 发送周期，单位为毫秒。
   * @return 设置成功返回 `true`。
   */
  bool SetStreamInterval(StreamHandle handle, uint32_t interval_ms);

  /**
   * @brief 启用或停用周期发送。
   *
   * @param handle 周期发送句柄。
   * @param enabled 是否启用。
   * @param start_delay_ms 重新启用后的首次发送延时，单位为毫秒。
   * @return 设置成功返回 `true`。
   */
  bool EnableStream(StreamHandle handle,
                    bool enabled,
                    uint32_t start_delay_ms = 0U);

  /**
   * @brief 判断周期发送是否启用。
   *
   * @param handle 周期发送句柄。
   * @return 已启用返回 `true`。
   */
  bool IsStreamEnabled(StreamHandle handle) const;

  /**
   * @brief 更新周期发送调度。
   *
   * @param now_ms 当前毫秒时间戳。
   */
  void Update(uint32_t now_ms);

  /**
   * @brief 重置所有周期发送的下次发送时间。
   *
   * @param now_ms 当前毫秒时间戳。
   */
  void ResetStreams(uint32_t now_ms = 0U);

  /**
   * @brief 发送心跳状态。
   */
  void SendHeartbeat();

  /**
   * @brief 发送系统健康状态。
   */
  void SendSystemStatus();

  /**
   * @brief 发送电池状态。
   */
  void SendBatteryStatus();

  /**
   * @brief 发送 GPS 状态。
   */
  void SendGpsStatus();

  /**
   * @brief 发送全局位置状态。
   */
  void SendGlobalPosition();

  /**
   * @brief 发送本地 NED 位置状态。
   */
  void SendLocalPosition();

  /**
   * @brief 发送姿态状态。
   */
  void SendAttitude();

  /**
   * @brief 发送飞行仪表状态。
   */
  void SendFlightHud();

  /**
   * @brief 发送 RC 通道状态。
   */
  void SendRcChannels();

  /**
   * @brief 发送输出通道状态。
   */
  void SendOutputStatus();

  /**
   * @brief 发送扩展运行状态。
   */
  void SendExtendedState();

  /**
   * @brief 发送状态文本。
   */
  void SendStatusText();

  /**
   * @brief 发送长命令请求。
   */
  void SendCommandRequest();

  /**
   * @brief 发送命令响应。
   */
  void SendCommandResponse();

  /**
   * @brief 发送手动控制输入。
   */
  void SendManualControl();

  /**
   * @brief 发送固件版本信息。
   */
  void SendVersionInfo();

  /**
   * @brief 发送 MAVLink 参数值。
   */
  void SendParameterValue();

  /**
   * @brief 发送 MAVLink 参数值。
   *
   * @param parameter MAVLink 参数值快照。
   */
  void SendParameterValue(const MavlinkParameterValue &parameter);

private:
  /**
   * @brief 周期发送槽位。
   */
  struct StreamSlot final {
    const char *name = nullptr; /**< 周期发送名称。 */
    StreamCallback callback = nullptr; /**< 周期发送回调函数。 */
    StreamHandle handle = kInvalidStreamHandle; /**< 周期发送句柄。 */
    uint32_t interval_ms = 0U; /**< 发送周期，单位为毫秒。 */
    uint32_t start_delay_ms = 0U; /**< 首次发送延时，单位为毫秒。 */
    uint32_t next_send_ms = 0U; /**< 下次发送时间戳。 */
    uint32_t generation = 0U; /**< 槽位代数计数。 */
    uint8_t slot_index = kMaxStreams; /**< 当前槽位索引。 */
    bool allocated = false; /**< 槽位是否已分配。 */
    bool enabled = false; /**< 周期发送是否启用。 */
    bool scheduled = false; /**< 下次发送时间是否已装载。 */
  };

  /**
   * @brief 查找空闲周期发送槽位。
   *
   * @return 空闲槽位索引，未找到返回 `kMaxStreams`。
   */
  uint8_t FindFreeStreamSlot() const;

  /**
   * @brief 查找周期发送槽位。
   *
   * @param handle 周期发送句柄。
   * @return 槽位索引，未找到返回 `-1`。
   */
  int16_t FindStreamIndex(StreamHandle handle) const;

  /**
   * @brief 判断句柄是否匹配指定槽位。
   *
   * @param slot 周期发送槽位。
   * @param handle 周期发送句柄。
   * @param slot_index 槽位索引。
   * @return 匹配返回 `true`。
   */
  bool IsStreamHandleMatched(const StreamSlot &slot,
                             StreamHandle handle,
                             uint8_t slot_index) const;

  /**
   * @brief 清空周期发送槽位。
   *
   * @param slot_index 槽位索引。
   */
  void ClearStreamSlot(uint8_t slot_index);

  /**
   * @brief 装载周期发送时间。
   *
   * @param slot 周期发送槽位。
   * @param now_ms 当前毫秒时间戳。
   */
  void ScheduleStream(StreamSlot &slot, uint32_t now_ms);

  /**
   * @brief 发送一帧 MAVLink 消息。
   *
   * @param msg MAVLink 消息。
   */
  void SendMessage(const mavlink_message_t &msg);

  /**
   * @brief 组合周期发送句柄。
   *
   * @param slot_index 槽位索引。
   * @param generation 槽位代数。
   * @return 周期发送句柄。
   */
  static StreamHandle MakeStreamHandle(uint8_t slot_index,
                                       uint32_t generation);

  /**
   * @brief 从周期发送句柄中提取槽位索引。
   *
   * @param handle 周期发送句柄。
   * @return 槽位索引。
   */
  static uint8_t ExtractStreamIndex(StreamHandle handle);

  /**
   * @brief 从周期发送句柄中提取槽位代数。
   *
   * @param handle 周期发送句柄。
   * @return 槽位代数。
   */
  static uint32_t ExtractStreamGeneration(StreamHandle handle);

  /**
   * @brief 解析首次发送延时。
   *
   * @param interval_ms 发送周期，单位为毫秒。
   * @param start_delay_ms 首次发送延时，单位为毫秒。
   * @return 实际首次发送延时。
   */
  static uint32_t ResolveStartDelay(uint32_t interval_ms,
                                    uint32_t start_delay_ms);

  static constexpr uint8_t kSystemId = 25U; /**< 本机 MAVLink 系统 ID。 */
  static constexpr uint8_t kComponentId = MAV_COMP_ID_AUTOPILOT1; /**< 本机 MAVLink 组件 ID。 */

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
  StreamSlot streams_[kMaxStreams] {}; /**< 周期发送槽位表。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_HPP */
