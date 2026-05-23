/**
 * @file task_all_init.cpp
 * @brief 初始化任务。
 */

#include "task.hpp"
#include "task_all_init.hpp"
#include "mavlink_link.hpp"
#include "usb_uart.hpp"

namespace {

  constexpr uint16_t kUsbRxQueueSize = 1000U;

  iFly::UsbUart Usb_Cdc(kUsbRxQueueSize);
  iFly::MavlinkLink Mavlink(&Usb_Cdc);

}

bool InitMavlinkTask(iFly::MavlinkLink *link);
bool InitLedCtrlTask(void);
bool InitPidCtrlTask(void);
bool InitW25q32TestTask(void);
bool InitRcTask(void);

namespace iFly {

  bool InitAllTasks(void)
  {
    Usb_Cdc.Init();

    bool init_sta = InitMavlinkTask(&Mavlink);
    init_sta = InitLedCtrlTask() && init_sta;
    init_sta = InitPidCtrlTask() && init_sta;
    init_sta = InitW25q32TestTask() && init_sta;
    init_sta = InitRcTask() && init_sta;
    return init_sta;
  }

}

