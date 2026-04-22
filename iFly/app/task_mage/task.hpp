/**
 * @file task.hpp
 * @brief 任务管理模块接口。
 */
#ifndef IFLY_TASK_HPP
#define IFLY_TASK_HPP

#include <stdint.h>

#include "soft_timer.hpp"

namespace iFly::task {

/**
 * @brief 基于 `SoftTimerService` 的上层任务管理器。
 */
class TaskManager final {
public:
  using TaskCallback = void (*)(void *context); /**< 任务回调函数签名。 */
  using TaskHandle = uint32_t; /**< 上层任务句柄类型。 */

  static constexpr TaskHandle kInvalidTaskHandle = 0U; /**< 无效任务句柄。 */
  static constexpr uint8_t kMaxTasks = SoftTimerService::kMaxTasks; /**< 最大任务数量。 */
  static constexpr uint32_t kUsePeriodAsStartDelay =
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
   * @brief 获取任务管理器单例。
   *
   * @return 单例引用。
   */
  static TaskManager &Instance();

  /**
   * @brief 按完整配置创建任务。
   *
   * @param config 任务配置。
   * @return 创建成功返回有效任务句柄，否则返回 `kInvalidTaskHandle`。
   */
  TaskHandle CreateTask(const TaskConfig &config);

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
  TaskHandle CreatePeriodicTask(TaskCallback callback, void *context,
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
  TaskHandle CreateOneShotTask(TaskCallback callback, void *context,
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
  bool DeleteTask(TaskHandle handle);

  /**
   * @brief 删除全部任务。
   */
  void DeleteAllTasks();

  /**
   * @brief 延后指定任务的下一次执行。
   *
   * @param handle 目标任务句柄。
   * @param delay_ms 延时时长，单位为毫秒。
   * @return 延后成功返回 `true`。
   */
  bool DelayTask(TaskHandle handle, uint32_t delay_ms);

  /**
   * @brief 延后当前正在执行任务的下一次执行。
   *
   * @param delay_ms 延时时长，单位为毫秒。
   * @return 延后成功返回 `true`。
   */
  bool DelayCurrentTask(uint32_t delay_ms);

  /**
   * @brief 挂起指定任务。
   *
   * @param handle 目标任务句柄。
   * @return 挂起成功返回 `true`。
   */
  bool SuspendTask(TaskHandle handle);

  /**
   * @brief 恢复指定任务。
   *
   * @param handle 目标任务句柄。
   * @param delay_ms 恢复后的首次执行延时。
   * @return 恢复成功返回 `true`。
   */
  bool ResumeTask(TaskHandle handle,
                  uint32_t delay_ms = kUsePeriodAsStartDelay);

  /**
   * @brief 判断任务是否仍然存在。
   *
   * @param handle 目标任务句柄。
   * @return 存在返回 `true`。
   */
  bool IsTaskAlive(TaskHandle handle) const;

  /**
   * @brief 判断任务是否处于挂起状态。
   *
   * @param handle 目标任务句柄。
   * @return 挂起返回 `true`。
   */
  bool IsTaskSuspended(TaskHandle handle) const;

  /**
   * @brief 获取指定任务的信息快照。
   *
   * @param handle 目标任务句柄。
   * @param info 输出信息对象。
   * @return 获取成功返回 `true`。
   */
  bool GetTaskInfo(TaskHandle handle, TaskInfo *info) const;

  /**
   * @brief 获取当前有效任务数量。
   *
   * @return 有效任务数量。
   */
  uint32_t TaskCount() const;

  /**
   * @brief 派发当前已到期任务。
   *
   * @return 本次实际执行的任务数量。
   */
  uint32_t Dispatch();

  /**
   * @brief 获取当前系统时间。
   *
   * @return 当前毫秒时间戳。
   */
  uint32_t Now() const;

private:
  /**
   * @brief 任务槽位。
   */
  struct TaskSlot final {
    const char *name = nullptr; /**< 任务名称。 */
    TaskCallback callback = nullptr; /**< 任务回调函数。 */
    void *context = nullptr; /**< 回调上下文。 */
    TaskHandle handle = kInvalidTaskHandle; /**< 上层任务句柄。 */
    SoftTimerService::TaskHandle timer_handle =
        SoftTimerService::kInvalidTaskHandle; /**< 绑定的软定时器句柄。 */
    uint32_t period_ms = 0U; /**< 任务周期，单位为毫秒。 */
    uint32_t default_start_delay_ms = 0U; /**< 默认启动延时，单位为毫秒。 */
    uint32_t requested_delay_ms = 0U; /**< 当前请求的延后执行时间。 */
    uint32_t generation = 0U; /**< 槽位代数计数。 */
    uint8_t priority = SoftTimerService::kLowestPriority; /**< 任务优先级。 */
    uint8_t slot_index = kMaxTasks; /**< 槽位索引。 */
    bool allocated = false; /**< 槽位是否已分配。 */
    bool auto_reload = true; /**< 是否自动重装。 */
    bool suspended = false; /**< 是否处于挂起状态。 */
    bool running = false; /**< 是否正在执行回调。 */
    bool pending_delete = false; /**< 是否等待回调结束后删除。 */
    bool delay_requested = false; /**< 是否已请求延后执行。 */
  };

  TaskManager() = default;

  /**
   * @brief 任务统一入口函数。
   *
   * @param context 回调上下文。
   */
  static void TaskEntry(void *context);

  /**
   * @brief 组合任务句柄。
   *
   * @param slot_index 槽位索引。
   * @param generation 槽位代数。
   * @return 组合后的任务句柄。
   */
  static TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation);

  /**
   * @brief 从任务句柄中提取槽位索引。
   *
   * @param handle 任务句柄。
   * @return 槽位索引。
   */
  static uint8_t ExtractTaskIndex(TaskHandle handle);

  /**
   * @brief 从任务句柄中提取槽位代数。
   *
   * @param handle 任务句柄。
   * @return 槽位代数。
   */
  static uint32_t ExtractTaskGeneration(TaskHandle handle);

  /**
   * @brief 判断任务句柄是否有效。
   *
   * @param handle 任务句柄。
   * @return 有效返回 `true`。
   */
  static bool IsValidTaskHandle(TaskHandle handle);

  /**
   * @brief 解析配置中的启动延时。
   *
   * @param config 任务配置。
   * @return 解析后的启动延时。
   */
  static uint32_t ResolveStartDelayMs(const TaskConfig &config);

  /**
   * @brief 为任务启动底层软定时器。
   *
   * @param slot 目标任务槽位。
   * @param delay_ms 启动延时。
   * @return 创建成功返回软定时器句柄。
   */
  SoftTimerService::TaskHandle StartTimer(TaskSlot &slot, uint32_t delay_ms);

  /**
   * @brief 停止任务对应的底层软定时器。
   *
   * @param slot 目标任务槽位。
   * @return 停止成功返回 `true`。
   */
  bool StopTimer(TaskSlot &slot);

  /**
   * @brief 重新装填任务定时器。
   *
   * @param slot 目标任务槽位。
   * @param delay_ms 延时参数。
   * @return 重装成功返回 `true`。
   */
  bool RearmTimer(TaskSlot &slot, uint32_t delay_ms);

  /**
   * @brief 查找空闲任务槽位。
   *
   * @return 空闲槽位索引。
   */
  uint8_t FindFreeSlot() const;

  /**
   * @brief 按任务句柄查找槽位索引。
   *
   * @param handle 任务句柄。
   * @return 找到时返回槽位索引，否则返回负值。
   */
  int16_t FindSlotIndex(TaskHandle handle) const;

  /**
   * @brief 判断句柄是否与指定槽位匹配。
   *
   * @param slot 待检查槽位。
   * @param handle 任务句柄。
   * @param slot_index 槽位索引。
   * @return 匹配返回 `true`。
   */
  bool IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                     uint8_t slot_index) const;

  /**
   * @brief 清空指定任务槽位。
   *
   * @param slot_index 槽位索引。
   */
  void ClearTaskSlot(uint8_t slot_index);

  TaskSlot tasks_[kMaxTasks] {}; /**< 固定大小任务表。 */
  TaskHandle current_task_handle_ = kInvalidTaskHandle; /**< 当前正在执行的任务句柄。 */
  uint8_t current_task_index_ = kMaxTasks; /**< 当前正在执行的槽位索引。 */
};

using Task = TaskManager;

} // namespace iFly::task

#endif /* IFLY_TASK_HPP */
