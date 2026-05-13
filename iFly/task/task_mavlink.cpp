/**
 * @file task_mavlink.cpp
 * @brief MAVLink 轮询任务。
 */

#include "mavlink_link.hpp"
#include "task.hpp"

namespace {

iFly::TaskHandle mavlink_handle = iFly::kInvalidTaskHandle;

void MavlinkTask(void *context)
{
  iFly::MavlinkLink *link = static_cast<iFly::MavlinkLink *>(context);
  if (link == nullptr) {
    return;
  }
}

} // namespace

bool InitMavlinkTask(iFly::MavlinkLink *link)
{
  if (link == nullptr) {
    return false;
  }

  mavlink_handle = iFly::TaskCreatePeriodic(&MavlinkTask,
                                            link,
                                            50U,
                                            iFly::SoftTimerService::kLowestPriority,
                                            0U,
                                            "mavlink");

  return mavlink_handle != iFly::kInvalidTaskHandle;
}

