/**
 * @file task_all_init.cpp
 * @brief 初始化任务。
 */

#include "task.hpp"
#include "task_all_init.hpp"
#include "flight_ctrl_cli.hpp"
#include "mavlink_link.hpp"
#include "usb_uart.hpp"

namespace {

  constexpr uint16_t kUsbRxQueueSize = 500U;

  iFly::FlightCtrlCli Mavlink_CLI;
  iFly::UsbUart Usb_Cdc(kUsbRxQueueSize);
  iFly::MavlinkLink Mavlink(&Usb_Cdc);

}

bool InitMavlinkTask(iFly::MavlinkLink *link);
bool InitLedCtrlTask(void);
bool InitPidCtrlTask(void);

namespace iFly {

  bool InitAllTasks(void)
  {
    Mavlink_CLI.Init();
    Mavlink_CLI.Console().DisableActivationKey();

    Usb_Cdc.Init();

    bool init_sta = InitMavlinkTask(&Mavlink);
    init_sta = InitLedCtrlTask() && init_sta;
    init_sta = InitPidCtrlTask() && init_sta;
    return init_sta;
  }

}
