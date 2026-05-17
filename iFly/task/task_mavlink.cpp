/**
 * @file task_mavlink.cpp
 * @brief MAVLink 轮询任务。
 */

#include "mavlink_link.hpp"
#include "mavlink_main.hpp"
#include "task.hpp"

namespace {

iFly::TaskHandle mavlink_handle = iFly::kInvalidTaskHandle;

void MavlinkTask(void *context)
{
  iFly::MavlinkLink *link = static_cast<iFly::MavlinkLink *>(context);
  if (link == nullptr) {
    return;
  }

  iFly::MavlinkMain(iFly::TaskNow());
}

} // namespace

bool InitMavlinkTask(iFly::MavlinkLink *link)
{
  if (link == nullptr) {
    return false;
  }

  if (!iFly::MavlinkMainInit(link)) {
    return false;
  }

  mavlink_handle = iFly::TaskCreatePeriodic(&MavlinkTask,
                                            link,
                                            20U,
                                            iFly::SoftTimerService::kLowestPriority,
                                            0U,
                                            "mavlink");

  return mavlink_handle != iFly::kInvalidTaskHandle;
}

