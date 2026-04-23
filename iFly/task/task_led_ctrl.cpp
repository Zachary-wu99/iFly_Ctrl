/**
 * @file task_led_ctrl.cpp
 * @brief led控制任务。
 */

#include "task.hpp"



namespace {

  iFly::TaskHandle led_ctrl_handle = iFly::kInvalidTaskHandle;

}

void LedCtrlTask(void *context)
{

}


bool InitLedCtrlTask(void)
{
  led_ctrl_handle = iFly::TaskCreatePeriodic(&LedCtrlTask,
                                               nullptr,
                                               500U,
                                               iFly::SoftTimerService::kLowestPriority-1,
                                               0U,
                                               "led_ctrl");

  return led_ctrl_handle != iFly::kInvalidTaskHandle;
}

