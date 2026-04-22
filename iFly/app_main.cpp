#include "app_main.h"

#include <stdint.h>

#include "task.hpp"
#include "task_all_init.hpp"



extern "C" void app_main(void)
{
  (void)iFly::InitAllTasks();
  while (1) {
    // 执行任务调度
    (void)iFly::TaskDispatch();
  }
}
