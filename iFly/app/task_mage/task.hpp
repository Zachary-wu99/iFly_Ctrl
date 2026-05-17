/**
 * @file task.hpp
 * @brief 任务管理模块接口。
 */
#ifndef IFLY_TASK_HPP
#define IFLY_TASK_HPP

#include <stdint.h>

#include "soft_timer.hpp"

namespace iFly {

using TaskCallback = void (*)(void *context); /**< 任务回调函数签名。 */
using TaskHandle = uint32_t; /**< 上层任务句柄类型。 */

inline constexpr TaskHandle kInvalidTaskHandle = 0U; /**< 无效任务句柄。 */
inline constexpr uint8_t kMaxTasks = SoftTimerService::kMaxTasks; /**< 最大任务数量。 */
inline constexpr uint32_t kUsePeriodAsStartDelay =
    0xFFFFFFFFUL; /**< 使用任务周期作为默认启动延时。 */

/**
 * @brief 任务创建配置。
 */
struct TaskConfig final {
  const char *name = nullptr; /**< 任务名称，仅用于调试与查询。 */
  TaskCallback callback = nullptr; /**< 任务回调函数。 */
  void *context = nullptr; /**< 回调上下文指针。 */
  uint32_t period_ms = 0U; /**< 任务周期，单位为毫秒。 */
  uint32_t start_delay_ms = kUsePeriodAsStartDelay; /**< 首次启动延时，单位为毫秒。 */
  uint8_t priority = SoftTimerService::kLowestPriority; /**< 调度优先级。 */
  bool auto_reload = true; /**< 是否自动重装。 */
};

/**
 * @brief 面向上层查询的任务信息快照。
 */
struct TaskInfo final {
  const char *name = nullptr; /**< 任务名称。 */
  uint32_t period_ms = 0U; /**< 当前任务周期，单位为毫秒。 */
  uint8_t priority = SoftTimerService::kLowestPriority; /**< 当前任务优先级。 */
  bool auto_reload = true; /**< 是否自动重装。 */
  bool suspended = false; /**< 当前是否处于挂起状态。 */
};

/**
 * @brief 按完整配置创建任务。
 *
 * @param config 任务配置。
 * @return 创建成功返回有效任务句柄，否则返回 `kInvalidTaskHandle`。
 */
TaskHandle TaskCreate(const TaskConfig &config);

/**
 * @brief 创建周期任务。
 *
 * @param callback 任务回调函数。
 * @param context 回调上下文指针。
 * @param period_ms 任务周期，单位为毫秒。
 * @param priority 调度优先级。
 * @param start_delay_ms 首次启动延时，单位为毫秒。
 * @param name 任务名称。
 * @return 创建成功返回有效任务句柄，否则返回 `kInvalidTaskHandle`。
 */
TaskHandle TaskCreatePeriodic(TaskCallback callback,
                              void *context,
                              uint32_t period_ms,
                              uint8_t priority =
                                  SoftTimerService::kLowestPriority,
                              uint32_t start_delay_ms =
                                  kUsePeriodAsStartDelay,
                              const char *name = nullptr);

/**
 * @brief 创建一次性任务。
 *
 * @param callback 任务回调函数。
 * @param context 回调上下文指针。
 * @param delay_ms 延时时长，单位为毫秒。
 * @param priority 调度优先级。
 * @param name 任务名称。
 * @return 创建成功返回有效任务句柄，否则返回 `kInvalidTaskHandle`。
 */
TaskHandle TaskCreateOneShot(TaskCallback callback,
                             void *context,
                             uint32_t delay_ms,
                             uint8_t priority =
                                 SoftTimerService::kLowestPriority,
                             const char *name = nullptr);

/**
 * @brief 删除指定任务。
 *
 * @param handle 目标任务句柄。
 * @return 删除成功返回 `true`。
 */
bool TaskDelete(TaskHandle handle);

/**
 * @brief 删除全部任务。
 */
void TaskDeleteAll();

/**
 * @brief 延后指定任务的下一次执行。
 *
 * @param handle 目标任务句柄。
 * @param delay_ms 延时时长，单位为毫秒。
 * @return 延后成功返回 `true`。
 */
bool TaskDelay(TaskHandle handle, uint32_t delay_ms);

/**
 * @brief 延后当前正在执行任务的下一次执行。
 *
 * @param delay_ms 延时时长，单位为毫秒。
 * @return 延后成功返回 `true`。
 */
bool TaskDelayCurrent(uint32_t delay_ms);

/**
 * @brief 挂起指定任务。
 *
 * @param handle 目标任务句柄。
 * @return 挂起成功返回 `true`。
 */
bool TaskSuspend(TaskHandle handle);

/**
 * @brief 恢复指定任务。
 *
 * @param handle 目标任务句柄。
 * @param delay_ms 恢复后的首次执行延时。
 * @return 恢复成功返回 `true`。
 */
bool TaskResume(TaskHandle handle,
                uint32_t delay_ms = kUsePeriodAsStartDelay);

/**
 * @brief 判断任务是否仍然存在。
 *
 * @param handle 目标任务句柄。
 * @return 存在返回 `true`。
 */
bool TaskIsAlive(TaskHandle handle);

/**
 * @brief 判断任务是否处于挂起状态。
 *
 * @param handle 目标任务句柄。
 * @return 挂起返回 `true`。
 */
bool TaskIsSuspended(TaskHandle handle);

/**
 * @brief 获取指定任务的信息快照。
 *
 * @param handle 目标任务句柄。
 * @param info 输出信息对象。
 * @return 获取成功返回 `true`。
 */
bool TaskGetInfo(TaskHandle handle, TaskInfo *info);

/**
 * @brief 获取当前有效任务数量。
 *
 * @return 有效任务数量。
 */
uint32_t TaskCount();

/**
 * @brief 派发当前已到期任务。
 *
 * @return 本次实际执行的任务数量。
 */
uint32_t TaskDispatch();

/**
 * @brief 获取当前系统时间。
 *
 * @return 当前毫秒时间戳。
 */
uint32_t TaskNow();

} // namespace iFly

#endif /* IFLY_TASK_HPP */

