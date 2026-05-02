/**
 * @file task_led_ctrl.cpp
 * @brief led控制任务。
 */

#include "gpio.hpp"
#include "task.hpp"

#include "led.hpp"

namespace {

  iFly::TaskHandle led_ctrl_handle = iFly::kInvalidTaskHandle;
  iFly::Led<iFly::GpioPortId::kC, iFly::GpioPinId::kPin8, iFly::LedActiveLevel::kHigh, true> LED_B(true);
  iFly::Led<iFly::GpioPortId::kC, iFly::GpioPinId::kPin9, iFly::LedActiveLevel::kHigh, true> LED_G(true);
  iFly::Led<iFly::GpioPortId::kA, iFly::GpioPinId::kPin8, iFly::LedActiveLevel::kHigh, true> LED_R(true);
}

void LedCtrlTask(void *context)
{
  (void)context;

  //(void)LED_B.Toggle();
  //(void)LED_R.Toggle();
  (void)LED_G.Toggle();
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

