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
  static constexpr uint32_t kUsePeriodAsStartDelay = 0xFFFFFFFFUL; /**< 使用周期作为默认启动延时。 */

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
   * @brief 供上层查询的任务信息快照。
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

  TaskHandle CreatePeriodicTask(
      TaskCallback callback, void *context, uint32_t period_ms,
      uint8_t priority = SoftTimerService::kLowestPriority,
      uint32_t start_delay_ms = kUsePeriodAsStartDelay,
      const char *name = nullptr);

  TaskHandle CreateOneShotTask(
      TaskCallback callback, void *context, uint32_t delay_ms,
      uint8_t priority = SoftTimerService::kLowestPriority,
      const char *name = nullptr);

  bool DeleteTask(TaskHandle handle);
  void DeleteAllTasks();
  bool DelayTask(TaskHandle handle, uint32_t delay_ms);
  bool DelayCurrentTask(uint32_t delay_ms);
  bool SuspendTask(TaskHandle handle);
  bool ResumeTask(TaskHandle handle,
                  uint32_t delay_ms = kUsePeriodAsStartDelay);
  bool IsTaskAlive(TaskHandle handle) const;
  bool IsTaskSuspended(TaskHandle handle) const;
  bool GetTaskInfo(TaskHandle handle, TaskInfo *info) const;
  uint32_t TaskCount() const;
  uint32_t Dispatch();
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
        SoftTimerService::kInvalidTaskHandle; /**< 绑定的底层软定时器句柄。 */
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
    bool pending_delete = false; /**< 是否在回调结束后删除。 */
    bool delay_requested = false; /**< 是否已请求延后执行。 */
  };

  TaskManager() = default;

  static void TaskEntry(void *context);
  static TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation);
  static uint8_t ExtractTaskIndex(TaskHandle handle);
  static uint32_t ExtractTaskGeneration(TaskHandle handle);
  static bool IsValidTaskHandle(TaskHandle handle);
  static uint32_t ResolveStartDelayMs(const TaskConfig &config);
  SoftTimerService::TaskHandle StartTimer(TaskSlot &slot, uint32_t delay_ms);
  bool StopTimer(TaskSlot &slot);
  bool RearmTimer(TaskSlot &slot, uint32_t delay_ms);
  uint8_t FindFreeSlot() const;
  int16_t FindSlotIndex(TaskHandle handle) const;
  bool IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                     uint8_t slot_index) const;
  void ClearTaskSlot(uint8_t slot_index);

  TaskSlot tasks_[kMaxTasks] {}; /**< 固定大小任务表。 */
  TaskHandle current_task_handle_ = kInvalidTaskHandle; /**< 当前正在执行的任务句柄。 */
  uint8_t current_task_index_ = kMaxTasks; /**< 当前正在执行的槽位索引。 */
};

using Task = TaskManager;

} // namespace iFly::task

#endif /* IFLY_TASK_HPP */
