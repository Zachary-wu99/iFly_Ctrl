/**
 * @file mavlink_main.hpp
 * @brief MAVLink 主入口接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_MAIN_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_MAIN_HPP

#include <stdint.h>

#include "common/mavlink.h"

namespace iFly {

class MavlinkLink;

bool MavlinkMainInit(MavlinkLink *link);
void MavlinkMain(uint32_t now_ms);
bool MavlinkMainHandleConsole(const mavlink_message_t &msg);

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_MAIN_HPP */

