/**
 * @file task_pid_ctrl.cpp
 * @brief PID控制任务。
 */

#include "pid.hpp"
#include "parameter_manager.hpp"
#include "task.hpp"

namespace {

  iFly::TaskHandle pid_ctrl_handle = iFly::kInvalidTaskHandle;
  iFly::Pid speed_pid;
  iFly::Pid angle_pid;
  iFly::Pid position_pid;

void ConfigurePidControllers()
{
  const iFly::SysParameters &parameters =
      iFly::ParameterManager::Instance().Data();

  speed_pid.Configure(parameters.control.speed_pid);
  angle_pid.Configure(parameters.control.angle_pid);
  position_pid.Configure(parameters.control.position_pid);
}



void PidCtrlTask(void *context)
{
  (void)context;

  
}

}

bool InitPidCtrlTask(void)
{
  ConfigurePidControllers();

  pid_ctrl_handle = iFly::TaskCreatePeriodic(&PidCtrlTask,
                                               nullptr,
                                               1U,
                                               iFly::SoftTimerService::kHighestPriority+2,
                                               0U,
                                               "pid_ctrl");

  return pid_ctrl_handle != iFly::kInvalidTaskHandle;
}

