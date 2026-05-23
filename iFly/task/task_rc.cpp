/**
 * @file task_rc.cpp
 * @brief RC 接收任务。
 */

#include "hardware_uart.hpp"
#include "rc_main.hpp"
#include "task.hpp"

namespace {

constexpr uint32_t kRcRxQueueSize = 200U;

iFly::HardwareUart<> RcUart(iFly::UartPortId::kUart6, kRcRxQueueSize);
iFly::Rc RcReceiver(&RcUart);
iFly::TaskHandle rc_handle = iFly::kInvalidTaskHandle;

/**
 * @brief RC 周期任务入口。
 *
 * @param context 任务上下文，当前未使用。
 */
void RcTask(void *context)
{
  (void)context;
  iFly::RcMain(iFly::TaskNow());
}

} // namespace

/**
 * @brief 初始化 RC 接收任务。
 *
 * @return 创建成功返回 `true`。
 */
bool InitRcTask(void)
{
  RcUart.Init();

  if (!iFly::RcMainInit(&RcReceiver)) {
    return false;
  }

  rc_handle = iFly::TaskCreatePeriodic(&RcTask,
                                       nullptr,
                                       5U,
                                       iFly::SoftTimerService::kHighestPriority + 1U,
                                       0U,
                                       "rc");
  return rc_handle != iFly::kInvalidTaskHandle;
}
