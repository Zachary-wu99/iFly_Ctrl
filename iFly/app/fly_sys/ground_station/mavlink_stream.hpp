/**
 * @file mavlink_stream.hpp
 * @brief MAVLink 周期数据流调度接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HPP

#include <stdint.h>

#include "mavlink_link.hpp"

namespace iFly {

/**
 * @brief MAVLink 周期数据流调度服务。
 */
class MavlinkStream final {
public:
  using SendCallback = void (*)(); /**< 周期发送回调函数。 */

  /**
   * @brief MAVLink 周期数据流配置。
   */
  struct StreamConfig final {
    const char *type_name = nullptr; /**< MAVLink 类型名称。 */
    SendCallback send = nullptr; /**< 周期发送回调函数。 */
    uint16_t period_ms = 0U; /**< 发送周期，单位为毫秒。 */
  };

  /**
   * @brief 构造 MAVLink 周期数据流调度对象。
   *
   * @param link MAVLink 字节流链路。
   */
  explicit MavlinkStream(MavlinkLink *link = nullptr);

  /**
   * @brief 绑定 MAVLink 字节流链路。
   *
   * @param link MAVLink 字节流链路。
   */
  void BindLink(MavlinkLink *link);

  /**
   * @brief 获取当前绑定的 MAVLink 字节流链路。
   *
   * @return MAVLink 字节流链路。
   */
  MavlinkLink *Link() const;

  /**
   * @brief 获取当前活动的 MAVLink 字节流链路。
   *
   * @return MAVLink 字节流链路。
   */
  static MavlinkLink *ActiveLink();

  /**
   * @brief 更新周期数据流回调调度。
   *
   * @param now_ms 当前毫秒时间戳。
   */
  void Update(uint32_t now_ms);

  /**
   * @brief 重置所有周期数据流的下次发送时间。
   *
   * @param now_ms 当前毫秒时间戳。
   */
  void ResetStreams(uint32_t now_ms = 0U);

private:
  static constexpr uint8_t kMaxStreams = 17U; /**< 最大周期数据流数量。 */

  /**
   * @brief 获取周期数据流数量。
   *
   * @return 周期数据流数量。
   */
  static uint8_t StreamCount();

  /**
   * @brief 获取周期数据流配置。
   *
   * @param index 周期数据流索引。
   * @return 周期数据流配置。
   */
  static const StreamConfig &StreamAt(uint8_t index);

  inline static MavlinkLink *active_link_ = nullptr; /**< 当前活动链路。 */

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
  uint32_t next_send_ms_[kMaxStreams] {}; /**< 下次发送时间戳表。 */
  bool scheduled_[kMaxStreams] {}; /**< 下次发送时间是否已装载。 */
};

} // namespace iFly

#include "mavlink_stream/attitude.hpp"
#include "mavlink_stream/battery_status.hpp"
#include "mavlink_stream/command_request.hpp"
#include "mavlink_stream/command_response.hpp"
#include "mavlink_stream/extended_state.hpp"
#include "mavlink_stream/flight_hud.hpp"
#include "mavlink_stream/global_position.hpp"
#include "mavlink_stream/gps_status.hpp"
#include "mavlink_stream/heartbeat.hpp"
#include "mavlink_stream/local_position.hpp"
#include "mavlink_stream/manual_control.hpp"
#include "mavlink_stream/output_status.hpp"
#include "mavlink_stream/parameter_value.hpp"
#include "mavlink_stream/rc_channels.hpp"
#include "mavlink_stream/status_text.hpp"
#include "mavlink_stream/system_status.hpp"
#include "mavlink_stream/version_info.hpp"

namespace iFly {

inline constexpr MavlinkStream::StreamConfig kMavlinkStreams[] = {
    {"HEARTBEAT", &MavlinkStreamSendHeartbeat, 1000U},
    {"SYS_STATUS", &MavlinkStreamSendSystemStatus, 1000U},
    {"BATTERY_STATUS", &MavlinkStreamSendBatteryStatus, 1000U},
    {"GPS_RAW_INT", &MavlinkStreamSendGpsStatus, 200U},
    {"GLOBAL_POSITION_INT", &MavlinkStreamSendGlobalPosition, 200U},
    {"LOCAL_POSITION_NED", &MavlinkStreamSendLocalPosition, 200U},
    {"ATTITUDE", &MavlinkStreamSendAttitude, 100U},
    {"VFR_HUD", &MavlinkStreamSendFlightHud, 200U},
    {"RC_CHANNELS", &MavlinkStreamSendRcChannels, 200U},
    {"SERVO_OUTPUT_RAW", &MavlinkStreamSendOutputStatus, 200U},
    {"EXTENDED_SYS_STATE", &MavlinkStreamSendExtendedState, 1000U},
    {"STATUSTEXT", &MavlinkStreamSendStatusText, 1000U},
    {"COMMAND_LONG", &MavlinkStreamSendCommandRequest, 1000U},
    {"COMMAND_ACK", &MavlinkStreamSendCommandResponse, 1000U},
    {"MANUAL_CONTROL", &MavlinkStreamSendManualControl, 200U},
    {"AUTOPILOT_VERSION", &MavlinkStreamSendVersionInfo, 1000U},
    {"PARAM_VALUE", &MavlinkStreamSendParameterValue, 1000U},
};

inline MavlinkStream::MavlinkStream(MavlinkLink *link)
    : link_(link)
{
  active_link_ = link;
}

inline void MavlinkStream::BindLink(MavlinkLink *link)
{
  link_ = link;
  active_link_ = link;
}

inline MavlinkLink *MavlinkStream::Link() const
{
  return link_;
}

inline MavlinkLink *MavlinkStream::ActiveLink()
{
  return active_link_;
}

inline void MavlinkStream::Update(uint32_t now_ms)
{
  const uint8_t stream_count = StreamCount();
  for (uint8_t stream_index = 0U; stream_index < stream_count; ++stream_index) {
    const StreamConfig &stream = StreamAt(stream_index);
    if ((stream.send == nullptr) || (stream.period_ms == 0U)) {
      continue;
    }

    if (!scheduled_[stream_index]) {
      next_send_ms_[stream_index] = now_ms;
      scheduled_[stream_index] = true;
    }

    if (static_cast<int32_t>(now_ms - next_send_ms_[stream_index]) < 0) {
      continue;
    }

    stream.send();
    next_send_ms_[stream_index] = now_ms + stream.period_ms;
  }
}

inline void MavlinkStream::ResetStreams(uint32_t now_ms)
{
  const uint8_t stream_count = StreamCount();
  for (uint8_t stream_index = 0U; stream_index < stream_count; ++stream_index) {
    next_send_ms_[stream_index] = now_ms;
    scheduled_[stream_index] = true;
  }
}

inline uint8_t MavlinkStream::StreamCount()
{
  return static_cast<uint8_t>(sizeof(kMavlinkStreams) /
                              sizeof(kMavlinkStreams[0]));
}

inline const MavlinkStream::StreamConfig &MavlinkStream::StreamAt(
    uint8_t index)
{
  return kMavlinkStreams[index];
}

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HPP */
