/**
 * @file task_dronecan.cpp
 * @brief DroneCAN 测试发送任务。
 */

#include "dronecan_node.hpp"
#include "task.hpp"

namespace {

iFly::TaskHandle dronecan_handle = iFly::kInvalidTaskHandle;
iFly::DroneCanNode dronecan_node;

void DroneCanTask(void *context)
{
  iFly::DroneCanNode *node = static_cast<iFly::DroneCanNode *>(context);
  if (node == nullptr) {
    return;
  }

  node->Poll();
}

} // namespace

bool InitDroneCanTask(void)
{
  if (!dronecan_node.Init()) {
    return false;
  }

  dronecan_handle = iFly::TaskCreatePeriodic(&DroneCanTask,
                                             &dronecan_node,
                                             1U,
                                             iFly::SoftTimerService::kHighestPriority + 1U,
                                             0U,
                                             "dronecan");

  return dronecan_handle != iFly::kInvalidTaskHandle;
}
