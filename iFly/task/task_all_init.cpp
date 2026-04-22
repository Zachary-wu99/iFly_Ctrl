/**
 * @file task_all_init.cpp
 * @brief 初始化任务。
 */

#include "task.hpp"
#include "task_all_init.hpp"
#include "flight_ctrl_cli.hpp"

namespace {
    iFly::FlightCtrlCli cli;
}

bool InitCliPollTask(iFly::FlightCtrlCli *cli);
bool InitLedCtrlTask(void);

namespace iFly {

  bool InitAllTasks(void)
  {
    bool init_sta = InitCliPollTask(&cli);
    init_sta = InitLedCtrlTask() && init_sta;
    return init_sta;
  }

}

