/**
 * @file task_mavlink.cpp
 * @brief MAVLink 轮询任务。
 */

#include "mavlink_link.hpp"
#include "mavlink_receiver.hpp"
#include "mavlink_send.hpp"
#include "task.hpp"

namespace {

iFly::TaskHandle mavlink_handle = iFly::kInvalidTaskHandle;
iFly::MavlinkReceiver mavlink_receiver;
iFly::MavlinkSend mavlink_send;

void MavlinkTask(void *context)
{
  iFly::MavlinkLink *link = static_cast<iFly::MavlinkLink *>(context);
  if (link == nullptr) {
    return;
  }

  (void)mavlink_receiver.Poll();
  mavlink_send.Update(iFly::TaskNow());
}

} // namespace

bool InitMavlinkTask(iFly::MavlinkLink *link)
{
  if (link == nullptr) {
    return false;
  }

  mavlink_receiver.BindLink(link);
  mavlink_send.BindLink(link);

  mavlink_handle = iFly::TaskCreatePeriodic(&MavlinkTask,
                                            link,
                                            50U,
                                            iFly::SoftTimerService::kLowestPriority,
                                            0U,
                                            "mavlink");

  return mavlink_handle != iFly::kInvalidTaskHandle;
}

