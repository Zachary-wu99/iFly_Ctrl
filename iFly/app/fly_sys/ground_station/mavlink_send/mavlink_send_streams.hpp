/**
 * @file mavlink_send_streams.hpp
 * @brief MAVLink 周期发送注册接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_STREAMS_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_STREAMS_HPP

#include "mavlink_send.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterHeartbeatStream(MavlinkSend *send,
                                                  uint32_t interval_ms,
                                                  uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterSystemStatusStream(MavlinkSend *send,
                                                     uint32_t interval_ms,
                                                     uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterBatteryStatusStream(MavlinkSend *send,
                                                      uint32_t interval_ms,
                                                      uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterGpsStatusStream(MavlinkSend *send,
                                                  uint32_t interval_ms,
                                                  uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterGlobalPositionStream(MavlinkSend *send,
                                                       uint32_t interval_ms,
                                                       uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterLocalPositionStream(MavlinkSend *send,
                                                      uint32_t interval_ms,
                                                      uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterAttitudeStream(MavlinkSend *send,
                                                 uint32_t interval_ms,
                                                 uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterFlightHudStream(MavlinkSend *send,
                                                  uint32_t interval_ms,
                                                  uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterRcChannelsStream(MavlinkSend *send,
                                                   uint32_t interval_ms,
                                                   uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterOutputStatusStream(MavlinkSend *send,
                                                     uint32_t interval_ms,
                                                     uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterExtendedStateStream(MavlinkSend *send,
                                                      uint32_t interval_ms,
                                                      uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterStatusTextStream(MavlinkSend *send,
                                                   uint32_t interval_ms,
                                                   uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterCommandRequestStream(MavlinkSend *send,
                                                       uint32_t interval_ms,
                                                       uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterCommandResponseStream(MavlinkSend *send,
                                                        uint32_t interval_ms,
                                                        uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterManualControlStream(MavlinkSend *send,
                                                      uint32_t interval_ms,
                                                      uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterVersionInfoStream(MavlinkSend *send,
                                                    uint32_t interval_ms,
                                                    uint32_t start_delay_ms = 0U);
MavlinkSend::StreamHandle RegisterParameterValueStream(MavlinkSend *send,
                                                       uint32_t interval_ms,
                                                       uint32_t start_delay_ms = 0U);

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_SEND_STREAMS_HPP */
