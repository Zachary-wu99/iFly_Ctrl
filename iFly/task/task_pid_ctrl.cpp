/**
 * @file task_pid_ctrl.cpp
 * @brief PID控制任务。
 */

#include "task.hpp"

namespace {

  iFly::TaskHandle pid_ctrl_handle = iFly::kInvalidTaskHandle;

}

void PidCtrlTask(void *context)
{

}


bool InitPidCtrlTask(void)
{
  pid_ctrl_handle = iFly::TaskCreatePeriodic(&PidCtrlTask,
                                               nullptr,
                                               1U,
                                               iFly::SoftTimerService::kHighestPriority+2,
                                               0U,
                                               "pid_ctrl");

  return pid_ctrl_handle != iFly::kInvalidTaskHandle;
}

