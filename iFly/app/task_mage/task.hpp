#ifndef IFLY_TASK_HPP
#define IFLY_TASK_HPP

#include <stdint.h>

#include "soft_timer.hpp"

namespace iFly {

class TaskManager final {
public:
  using TaskCallback = void (*)(void *context);
  using TaskHandle = uint32_t;

  static constexpr TaskHandle kInvalidTaskHandle = 0U;
  static constexpr uint8_t kMaxTasks = SoftTimerService::kMaxTasks;
  static constexpr uint32_t kUsePeriodAsStartDelay = 0xFFFFFFFFUL;

  struct TaskConfig final {
    const char *name = nullptr;
    TaskCallback callback = nullptr;
    void *context = nullptr;
    // Fixed period in ms. 0 means the task is manually rearmed.
    uint32_t period_ms = 0U;
    // First start delay in ms. Default follows period_ms.
    uint32_t start_delay_ms = kUsePeriodAsStartDelay;
    uint8_t priority = SoftTimerService::kLowestPriority;
    // true for periodic/manual task, false for one-shot task.
    bool auto_reload = true;
  };

  struct TaskInfo final {
    const char *name = nullptr;
    uint32_t period_ms = 0U;
    uint8_t priority = SoftTimerService::kLowestPriority;
    bool auto_reload = true;
    bool suspended = false;
  };

  static TaskManager &Instance();

  TaskHandle CreateTask(const TaskConfig &config);
  TaskHandle CreatePeriodicTask(TaskCallback callback, void *context,
                                uint32_t period_ms,
                                uint8_t priority = SoftTimerService::kLowestPriority,
                                uint32_t start_delay_ms = kUsePeriodAsStartDelay,
                                const char *name = nullptr);
  TaskHandle CreateOneShotTask(TaskCallback callback, void *context,
                               uint32_t delay_ms,
                               uint8_t priority = SoftTimerService::kLowestPriority,
                               const char *name = nullptr);

  bool DeleteTask(TaskHandle handle);
  void DeleteAllTasks();

  // Delay the next execution of a task.
  bool DelayTask(TaskHandle handle, uint32_t delay_ms);
  // Delay the current task from inside its callback.
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
  struct TaskSlot final {
    const char *name = nullptr;
    TaskCallback callback = nullptr;
    void *context = nullptr;
    TaskHandle handle = kInvalidTaskHandle;
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

private:
  TaskSlot tasks_[kMaxTasks] {};
  TaskHandle current_task_handle_ = kInvalidTaskHandle;
  uint8_t current_task_index_ = kMaxTasks;
};

using Task = TaskManager;

} // namespace iFly::task

#endif /* IFLY_TASK_HPP */
