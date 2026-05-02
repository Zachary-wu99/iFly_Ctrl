/**
 * @file task_all_init.cpp
 * @brief 初始化任务。
 */

#include "task.hpp"
#include "task_all_init.hpp"
#include "flight_ctrl_cli.hpp"
#include "usb_uart.hpp"

namespace {

  constexpr uint16_t kUsbRxQueueSize = 120U;
  constexpr char kDefaultCliTransport[] = "usb";

  iFly::FlightCtrlCli CLI;
  iFly::UsbUart Usb_Cdc(kUsbRxQueueSize);

}

bool InitCliPollTask(iFly::FlightCtrlCli *cli);
bool InitLedCtrlTask(void);
bool InitPidCtrlTask(void);

namespace iFly {

  bool InitAllTasks(void)
  {
    CLI.Init();
    CLI.RegisterTransport("usb",&Usb_Cdc);
    CLI.UseTransport("usb");
    Usb_Cdc.Init();

    bool init_sta = InitCliPollTask(&CLI);
    init_sta = InitLedCtrlTask() && init_sta;
    init_sta = InitPidCtrlTask() && init_sta;
    return init_sta;
  }

}

