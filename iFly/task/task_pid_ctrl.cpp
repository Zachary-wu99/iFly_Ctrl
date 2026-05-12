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

void LoadPidConfig(const char *kp_name,
                   const char *ki_name,
                   const char *kd_name,
                   const char *kff_name,
                   const char *integral_min_name,
                   const char *integral_max_name,
                   const char *output_min_name,
                   const char *output_max_name,
                   const char *derivative_cutoff_name,
                   const char *dt_min_name,
                   const char *dt_max_name,
                   const char *derivative_mode_name,
                   iFly::Pid::Config *config)
{
  if (config == nullptr) {
    return;
  }

  iFly::ProjectParameterManager &parameters =
      iFly::ProjectParameterManager::Instance();
  uint8_t derivative_mode = 0U;

  (void)parameters.Read(kp_name, &config->kp);
  (void)parameters.Read(ki_name, &config->ki);
  (void)parameters.Read(kd_name, &config->kd);
  (void)parameters.Read(kff_name, &config->kff);
  (void)parameters.Read(integral_min_name, &config->integral_min);
  (void)parameters.Read(integral_max_name, &config->integral_max);
  (void)parameters.Read(output_min_name, &config->output_min);
  (void)parameters.Read(output_max_name, &config->output_max);
  (void)parameters.Read(derivative_cutoff_name, &config->derivative_cutoff_hz);
  (void)parameters.Read(dt_min_name, &config->dt_min_s);
  (void)parameters.Read(dt_max_name, &config->dt_max_s);
  if (parameters.Read(derivative_mode_name, &derivative_mode)) {
    config->derivative_mode =
        static_cast<iFly::Pid::DerivativeMode>(derivative_mode);
  }
}

void ConfigurePidControllers()
{
  iFly::Pid::Config speed_config {};
  iFly::Pid::Config angle_config {};
  iFly::Pid::Config position_config {};

  LoadPidConfig("SPD_PID_P", "SPD_PID_I", "SPD_PID_D", "SPD_PID_FF",
                "SPD_PID_IMIN", "SPD_PID_IMAX", "SPD_PID_OMIN",
                "SPD_PID_OMAX", "SPD_PID_FLTD", "SPD_PID_DTMIN",
                "SPD_PID_DTMAX", "SPD_PID_DMODE", &speed_config);
  LoadPidConfig("ANG_PID_P", "ANG_PID_I", "ANG_PID_D", "ANG_PID_FF",
                "ANG_PID_IMIN", "ANG_PID_IMAX", "ANG_PID_OMIN",
                "ANG_PID_OMAX", "ANG_PID_FLTD", "ANG_PID_DTMIN",
                "ANG_PID_DTMAX", "ANG_PID_DMODE", &angle_config);
  LoadPidConfig("POS_PID_P", "POS_PID_I", "POS_PID_D", "POS_PID_FF",
                "POS_PID_IMIN", "POS_PID_IMAX", "POS_PID_OMIN",
                "POS_PID_OMAX", "POS_PID_FLTD", "POS_PID_DTMIN",
                "POS_PID_DTMAX", "POS_PID_DMODE", &position_config);

  speed_pid.Configure(speed_config);
  angle_pid.Configure(angle_config);
  position_pid.Configure(position_config);
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
