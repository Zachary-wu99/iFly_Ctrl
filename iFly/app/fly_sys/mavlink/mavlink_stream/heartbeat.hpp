/**
 * @file heartbeat.hpp
 * @brief MAVLink HEARTBEAT 数据流回调。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HEARTBEAT_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HEARTBEAT_HPP

#include "mavlink_parameter_service.hpp"

namespace iFly {

inline void MavlinkStreamSendHeartbeat()
{
  MavlinkLink *link = MavlinkStream::ActiveLink();
  if (link == nullptr) {
    return;
  }

  MavlinkParameterService *parameter_service =
      MavlinkStream::ActiveParameterService();
  if (parameter_service == nullptr) {
    return;
  }

  MavlinkParameterValue vehicle_type {};
  MavlinkParameterValue autopilot_type {};

  mavlink_heartbeat_t heartbeat {};
  heartbeat.type = parameter_service->ReadByName("MAV_TYPE", &vehicle_type)
                       ? static_cast<uint8_t>(vehicle_type.value)
                       : MAV_TYPE_GENERIC;
  heartbeat.autopilot =
      parameter_service->ReadByName("MAV_AUTOPILOT", &autopilot_type)
          ? static_cast<uint8_t>(autopilot_type.value)
          : MAV_AUTOPILOT_GENERIC;
  heartbeat.base_mode = MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
  heartbeat.custom_mode = 0U;
  heartbeat.system_status = MAV_STATE_STANDBY;

  mavlink_message_t msg {};
  (void)link->PackHeartbeat(&msg, heartbeat);
  link->SendMessage(msg);
}

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_STREAM_HEARTBEAT_HPP */
