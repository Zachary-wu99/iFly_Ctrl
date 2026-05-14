/**
 * @file task_mavlink.cpp
 * @brief MAVLink 轮询任务。
 */

#include "mavlink_link.hpp"
#include "mavlink_receiver.hpp"
#include "mavlink_stream.hpp"
#include "task.hpp"

namespace {

iFly::TaskHandle mavlink_handle = iFly::kInvalidTaskHandle;
iFly::MavlinkReceiver mavlink_receiver;
iFly::MavlinkStream mavlink_stream;

void MavlinkTask(void *context)
{
  iFly::MavlinkLink *link = static_cast<iFly::MavlinkLink *>(context);
  if (link == nullptr) {
    return;
  }

  (void)mavlink_receiver.Poll();
  mavlink_stream.Update(iFly::TaskNow());
}

} // namespace

bool InitMavlinkTask(iFly::MavlinkLink *link)
{
  if (link == nullptr) {
    return false;
  }

  mavlink_receiver.BindLink(link);
  mavlink_stream.BindLink(link);

  mavlink_handle = iFly::TaskCreatePeriodic(&MavlinkTask,
                                            link,
                                            50U,
                                            iFly::SoftTimerService::kLowestPriority,
                                            0U,
                                            "mavlink");

  return mavlink_handle != iFly::kInvalidTaskHandle;
}

