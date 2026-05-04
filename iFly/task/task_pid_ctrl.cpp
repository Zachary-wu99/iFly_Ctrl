/**
 * @file task_pid_ctrl.cpp
 * @brief PID控制任务。
 */

#include "pid.hpp"
#include "project_parameter_manager.hpp"
#include "task.hpp"

namespace {

  iFly::TaskHandle pid_ctrl_handle = iFly::kInvalidTaskHandle;
  iFly::Pid speed_pid;
  iFly::Pid angle_pid;
  iFly::Pid position_pid;

void ConfigurePidControllers()
{
  const iFly::ProjectParameters &parameters =
      iFly::ProjectParameterManager::Instance().Data();

  speed_pid.Configure(iFly::GetProjectParameter(parameters,
                                                &iFly::ProjectParameters::control,
                                                &iFly::ControlParameters::speed_pid));
  angle_pid.Configure(iFly::GetProjectParameter(parameters,
                                                &iFly::ProjectParameters::control,
                                                &iFly::ControlParameters::angle_pid));
  position_pid.Configure(iFly::GetProjectParameter(parameters,
                                                   &iFly::ProjectParameters::control,
                                                   &iFly::ControlParameters::position_pid));
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
