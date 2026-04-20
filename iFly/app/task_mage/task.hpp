// 任务管理模块接口。
// 在 SoftTimer 之上提供更直接的任务级抽象。
#ifndef IFLY_TASK_HPP
#define IFLY_TASK_HPP

#include <stdint.h>

#include "soft_timer.hpp"

namespace iFly::task {

/**
 * @brief 基于 SoftTimer 的上层任务管理器。
 *
 * @details
 * - 这一层对上提供“任务”语义，对下复用 `SoftTimerService` 做实际调度。
 * - 上层通过任务句柄管理任务，不直接暴露底层软定时器句柄。
 * - 任务支持创建、删除、挂起、恢复、查询、延时重装等常见操作。
 */
class TaskManager final {
public:
  /** @brief 任务回调函数签名。 */
  using TaskCallback = void (*)(void *context);
  /** @brief 上层任务句柄类型。 */
  using TaskHandle = uint32_t;

  /** @brief 无效任务句柄。 */
  static constexpr TaskHandle kInvalidTaskHandle = 0U;
  /** @brief 最大任务数量，直接沿用底层 SoftTimer 的容量。 */
  static constexpr uint8_t kMaxTasks = SoftTimerService::kMaxTasks;
  /** @brief 特殊标记：首次启动延时默认沿用任务周期。 */
  static constexpr uint32_t kUsePeriodAsStartDelay = 0xFFFFFFFFUL;

  /**
   * @brief 任务创建配置。
   *
   * @details
   * - `period_ms > 0`：表示固定周期任务。
   * - `period_ms == 0 && auto_reload == true`：表示“手动重装任务”，执行一次后进入挂起态，
   *   后续通过 `DelayTask()` / `ResumeTask()` 再次启动。
   * - `auto_reload == false`：表示单次任务，执行后自动删除。
   */
  struct TaskConfig final {
    /** @brief 任务名，可为空，仅用于调试和查询。 */
    const char *name = nullptr;
    /** @brief 任务回调，不能为空。 */
    TaskCallback callback = nullptr;
    /** @brief 回调上下文指针，原样透传给用户回调。 */
    void *context = nullptr;
    /** @brief 固定周期，单位 ms；为 0 表示手动重装任务。 */
    uint32_t period_ms = 0U;
    /** @brief 首次启动延时，单位 ms；默认跟随 `period_ms`。 */
    uint32_t start_delay_ms = kUsePeriodAsStartDelay;
    /** @brief 优先级，数值越小优先级越高。 */
    uint8_t priority = SoftTimerService::kLowestPriority;
    /** @brief 是否自动重装。true 为周期/手动任务，false 为单次任务。 */
    bool auto_reload = true;
  };

  /** @brief 提供给上层查询的任务信息快照。 */
  struct TaskInfo final {
    const char *name = nullptr;
    uint32_t period_ms = 0U;
    uint8_t priority = SoftTimerService::kLowestPriority;
    bool auto_reload = true;
    bool suspended = false;
  };

  /** @brief 获取单例对象。 */
  static TaskManager &Instance();

  /**
   * @brief 按完整配置创建任务。
   * @return 成功返回有效任务句柄，失败返回 `kInvalidTaskHandle`。
   */
  TaskHandle CreateTask(const TaskConfig &config);

  /**
   * @brief 快速创建周期任务。
   * @details
   * 周期任务会在每次执行完成后，自动延后 `period_ms` 再次运行。
   */
  TaskHandle CreatePeriodicTask(
      TaskCallback callback, void *context, uint32_t period_ms,
      uint8_t priority = SoftTimerService::kLowestPriority,
      uint32_t start_delay_ms = kUsePeriodAsStartDelay,
      const char *name = nullptr);

  /**
   * @brief 快速创建单次任务。
   * @details
   * 单次任务仅执行一次，执行结束后会自动从任务管理器中删除。
   */
  TaskHandle CreateOneShotTask(
      TaskCallback callback, void *context, uint32_t delay_ms,
      uint8_t priority = SoftTimerService::kLowestPriority,
      const char *name = nullptr);

  /** @brief 删除指定任务。 */
  bool DeleteTask(TaskHandle handle);
  /** @brief 删除全部任务。 */
  void DeleteAllTasks();

  /**
   * @brief 延后指定任务的下一次执行时间。
   * @details
   * - 若任务当前未运行，则直接重装底层定时器。
   * - 若任务当前正在运行，并且就是当前回调中的任务，则在回调退出后生效。
   */
  bool DelayTask(TaskHandle handle, uint32_t delay_ms);

  /**
   * @brief 在任务回调内部延后“当前任务”的下一次执行。
   * @details
   * 这个接口通常在任务内部调用，用来实现“本次执行完，X ms 后再来一次”。
   */
  bool DelayCurrentTask(uint32_t delay_ms);

  /** @brief 挂起任务。挂起后任务不会继续触发。 */
  bool SuspendTask(TaskHandle handle);
  /**
   * @brief 恢复任务。
   * @param delay_ms 恢复后到下一次执行的延时；默认沿用任务默认启动延时。
   */
  bool ResumeTask(TaskHandle handle,
                  uint32_t delay_ms = kUsePeriodAsStartDelay);

  /** @brief 判断任务句柄当前是否仍然有效。 */
  bool IsTaskAlive(TaskHandle handle) const;
  /** @brief 判断任务是否处于挂起态。 */
  bool IsTaskSuspended(TaskHandle handle) const;
  /** @brief 获取任务信息。 */
  bool GetTaskInfo(TaskHandle handle, TaskInfo *info) const;

  /** @brief 获取当前已分配的任务数量。 */
  uint32_t TaskCount() const;
  /** @brief 派发所有到期任务，底层直接转发给 SoftTimer。 */
  uint32_t Dispatch();
  /** @brief 获取当前任务管理器的时间基准，单位 ms。 */
  uint32_t Now() const;

private:
  /**
   * @brief 任务槽位。
   * @details
   * 使用固定数组管理，避免运行期动态内存分配。
   */
  struct TaskSlot final {
    const char *name = nullptr;
    TaskCallback callback = nullptr;
    void *context = nullptr;
    TaskHandle handle = kInvalidTaskHandle;
    /** @brief 底层 SoftTimer 对应的实际定时任务句柄。 */
    SoftTimerService::TaskHandle timer_handle =
        SoftTimerService::kInvalidTaskHandle;
    uint32_t period_ms = 0U;
    uint32_t default_start_delay_ms = 0U;
    uint32_t requested_delay_ms = 0U;
    uint32_t generation = 0U;
    uint8_t priority = SoftTimerService::kLowestPriority;
    uint8_t slot_index = kMaxTasks;
    bool allocated = false;
    bool auto_reload = true;
    bool suspended = false;
    bool running = false;
    bool pending_delete = false;
    bool delay_requested = false;
  };

  TaskManager() = default;

  /** @brief 底层 SoftTimer 实际触发时进入的统一回调入口。 */
  static void TaskEntry(void *context);

  /** @brief 编码任务句柄。 */
  static TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation);
  /** @brief 从句柄中解析槽位索引。 */
  static uint8_t ExtractTaskIndex(TaskHandle handle);
  /** @brief 从句柄中解析代数。 */
  static uint32_t ExtractTaskGeneration(TaskHandle handle);
  /** @brief 判断任务句柄是否非零有效。 */
  static bool IsValidTaskHandle(TaskHandle handle);
  /** @brief 解析创建任务时的默认启动延时。 */
  static uint32_t ResolveStartDelayMs(const TaskConfig &config);

  /** @brief 为某个槽位启动底层定时器。 */
  SoftTimerService::TaskHandle StartTimer(TaskSlot &slot, uint32_t delay_ms);
  /** @brief 停止某个槽位对应的底层定时器。 */
  bool StopTimer(TaskSlot &slot);
  /** @brief 重新装载某个槽位的底层定时器。 */
  bool RearmTimer(TaskSlot &slot, uint32_t delay_ms);

  /** @brief 查找空闲槽位。 */
  uint8_t FindFreeSlot() const;
  /** @brief 根据句柄查找槽位索引，失败返回 -1。 */
  int16_t FindSlotIndex(TaskHandle handle) const;
  /** @brief 校验句柄是否与当前槽位中的任务匹配。 */
  bool IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                     uint8_t slot_index) const;
  /** @brief 清空槽位并释放任务。 */
  void ClearTaskSlot(uint8_t slot_index);

private:
  /** @brief 固定大小任务表。 */
  TaskSlot tasks_[kMaxTasks] {};
  /** @brief 当前正在执行的任务句柄，仅在回调期间有效。 */
  TaskHandle current_task_handle_ = kInvalidTaskHandle;
  /** @brief 当前正在执行的任务槽位，仅在回调期间有效。 */
  uint8_t current_task_index_ = kMaxTasks;
};

/** @brief 简写别名，方便上层直接使用。 */
using Task = TaskManager;

} // namespace iFly::task

#endif /* IFLY_TASK_HPP */
