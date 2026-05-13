#include "task.hpp"

namespace {

struct TaskSlot final {
  const char *name = nullptr; /**< 任务名称。 */
  iFly::TaskCallback callback = nullptr; /**< 任务回调函数。 */
  void *context = nullptr; /**< 任务回调上下文。 */
  iFly::TaskHandle handle = iFly::kInvalidTaskHandle; /**< 上层任务句柄。 */
  iFly::SoftTimerService::TaskHandle timer_handle =
      iFly::SoftTimerService::kInvalidTaskHandle; /**< 绑定的软定时器句柄。 */
  uint32_t period_ms = 0U; /**< 任务周期，单位为毫秒。 */
  uint32_t default_start_delay_ms = 0U; /**< 默认启动延时，单位为毫秒。 */
  uint32_t requested_delay_ms = 0U; /**< 当前请求的延时时长，单位为毫秒。 */
  uint32_t generation = 0U; /**< 槽位代数计数。 */
  uint8_t priority = iFly::SoftTimerService::kLowestPriority; /**< 任务优先级。 */
  uint8_t slot_index = iFly::kMaxTasks; /**< 当前槽位索引。 */
  bool allocated = false; /**< 槽位是否已分配。 */
  bool auto_reload = true; /**< 是否自动重装。 */
  bool suspended = false; /**< 是否处于挂起状态。 */
  bool running = false; /**< 是否正在执行回调。 */
  bool pending_delete = false; /**< 是否等待回调结束后删除。 */
  bool delay_requested = false; /**< 是否已请求延后执行。 */
};

struct TaskRuntimeState final {
  TaskSlot tasks[iFly::kMaxTasks] {}; /**< 固定大小任务槽位表。 */
  iFly::TaskHandle current_task_handle = iFly::kInvalidTaskHandle; /**< 当前正在执行的任务句柄。 */
  uint8_t current_task_index = iFly::kMaxTasks; /**< 当前正在执行的槽位索引。 */
};

TaskRuntimeState &Runtime() {
  static TaskRuntimeState runtime; // 任务模块全局运行时状态。
  return runtime;
}

iFly::TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation) {
  return (generation << 16U) | static_cast<uint32_t>(slot_index + 1U);
}

uint8_t ExtractTaskIndex(iFly::TaskHandle handle) {
  return static_cast<uint8_t>((handle & 0xFFFFU) - 1U);
}

uint32_t ExtractTaskGeneration(iFly::TaskHandle handle) {
  return handle >> 16U;
}

bool IsValidTaskHandle(iFly::TaskHandle handle) {
  return handle != iFly::kInvalidTaskHandle;
}

bool IsHandleMatch(const TaskSlot &slot,
                   iFly::TaskHandle handle,
                   uint8_t slot_index) {
  return slot.allocated && (slot.slot_index == slot_index) &&
         (slot.generation == ExtractTaskGeneration(handle));
}

uint32_t ResolveStartDelayMs(const iFly::TaskConfig &config) {
  if (config.start_delay_ms != iFly::kUsePeriodAsStartDelay) {
    return config.start_delay_ms;
  }

  return (config.period_ms > 0U) ? config.period_ms : 0U;
}

uint8_t FindFreeSlot() {
  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  for (uint8_t slot_index = 0U; slot_index < iFly::kMaxTasks; ++slot_index) {
    if (!runtime.tasks[slot_index].allocated) {
      return slot_index;
    }
  }

  return iFly::kMaxTasks;
}

int16_t FindSlotIndex(iFly::TaskHandle handle) {
  if (!IsValidTaskHandle(handle)) {
    return -1;
  }

  const uint8_t slot_index = ExtractTaskIndex(handle); // 句柄编码中的槽位索引。
  if (slot_index >= iFly::kMaxTasks) {
    return -1;
  }

  return IsHandleMatch(Runtime().tasks[slot_index], handle, slot_index) ? slot_index
                                                                         : -1;
}

void ClearTaskSlot(uint8_t slot_index) {
  TaskSlot &slot = Runtime().tasks[slot_index]; // 需要清空的任务槽位。
  slot.name = nullptr;
  slot.callback = nullptr;
  slot.context = nullptr;
  slot.handle = iFly::kInvalidTaskHandle;
  slot.timer_handle = iFly::SoftTimerService::kInvalidTaskHandle;
  slot.period_ms = 0U;
  slot.default_start_delay_ms = 0U;
  slot.requested_delay_ms = 0U;
  slot.priority = iFly::SoftTimerService::kLowestPriority;
  slot.slot_index = iFly::kMaxTasks;
  slot.allocated = false;
  slot.auto_reload = true;
  slot.suspended = false;
  slot.running = false;
  slot.pending_delete = false;
  slot.delay_requested = false;
}

void TaskEntry(void *context);

iFly::SoftTimerService::TaskHandle StartTimer(TaskSlot &slot, uint32_t delay_ms) {
  iFly::SoftTimerService::TaskConfig config {}; // 底层软定时器创建配置。
  config.callback = &TaskEntry;
  config.context = &slot;
  config.interval_ms = 0U;
  config.start_delay_ms = delay_ms;
  config.priority = slot.priority;
  config.auto_reload = true;

  const iFly::SoftTimerService::TaskHandle timer_handle =
      iFly::SoftTimerService::Instance().CreateTask(config);
  if (timer_handle == iFly::SoftTimerService::kInvalidTaskHandle) {
    return iFly::SoftTimerService::kInvalidTaskHandle;
  }

  slot.timer_handle = timer_handle;
  slot.suspended = false;
  return timer_handle;
}

bool StopTimer(TaskSlot &slot) {
  if (slot.timer_handle == iFly::SoftTimerService::kInvalidTaskHandle) {
    return true;
  }

  const bool result =
      iFly::SoftTimerService::Instance().DeleteTask(slot.timer_handle);
  if (result) {
    slot.timer_handle = iFly::SoftTimerService::kInvalidTaskHandle;
  }

  return result;
}

bool RearmTimer(TaskSlot &slot, uint32_t delay_ms) {
  if (!StopTimer(slot)) {
    return false;
  }

  return StartTimer(slot, delay_ms) !=
         iFly::SoftTimerService::kInvalidTaskHandle;
}

void TaskEntry(void *context) {
  TaskSlot *slot = reinterpret_cast<TaskSlot *>(context); // 当前回调关联的任务槽位。
  if ((slot == nullptr) || !slot->allocated || (slot->slot_index >= iFly::kMaxTasks)) {
    return;
  }

  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  const uint8_t slot_index = slot->slot_index; // 当前槽位索引。
  const iFly::TaskHandle handle = slot->handle; // 当前任务句柄快照。

  runtime.current_task_handle = handle;
  runtime.current_task_index = slot_index;
  slot->running = true;

  if (slot->callback != nullptr) {
    slot->callback(slot->context);
  }

  runtime.current_task_handle = iFly::kInvalidTaskHandle;
  runtime.current_task_index = iFly::kMaxTasks;

  // 回调期间若任务已经被删除或槽位被复用，则不再继续处理。
  if (!IsHandleMatch(*slot, handle, slot_index)) {
    return;
  }

  slot->running = false;

  if (slot->pending_delete) {
    ClearTaskSlot(slot_index);
    return;
  }

  if (slot->suspended) {
    slot->timer_handle = iFly::SoftTimerService::kInvalidTaskHandle;
    return;
  }

  if (slot->delay_requested) {
    const uint32_t delay_ms = slot->requested_delay_ms; // 用户请求的新延时参数。
    slot->delay_requested = false;
    slot->requested_delay_ms = 0U;

    if (iFly::SoftTimerService::Instance().DelayCurrentTask(delay_ms)) {
      return;
    }

    ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload && (slot->period_ms > 0U)) {
    if (iFly::SoftTimerService::Instance().DelayCurrentTask(slot->period_ms)) {
      return;
    }

    ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload) {
    slot->suspended = true;
    slot->timer_handle = iFly::SoftTimerService::kInvalidTaskHandle;
    return;
  }

  ClearTaskSlot(slot_index);
}

} // namespace

namespace iFly {

TaskHandle TaskCreate(const TaskConfig &config) {
  if (config.callback == nullptr) {
    return kInvalidTaskHandle;
  }

  const uint8_t slot_index = FindFreeSlot(); // 新任务分配到的槽位索引。
  if (slot_index >= kMaxTasks) {
    return kInvalidTaskHandle;
  }

  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  TaskSlot &slot = runtime.tasks[slot_index]; // 本次要初始化的任务槽位。

  uint32_t generation = slot.generation + 1U; // 新句柄对应的槽位代数。
  if (generation == 0U) {
    generation = 1U;
  }

  slot.name = config.name;
  slot.callback = config.callback;
  slot.context = config.context;
  slot.handle = MakeTaskHandle(slot_index, generation);
  slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
  slot.period_ms = config.period_ms;
  slot.default_start_delay_ms = ResolveStartDelayMs(config);
  slot.requested_delay_ms = 0U;
  slot.generation = generation;
  slot.priority = config.priority;
  slot.slot_index = slot_index;
  slot.allocated = true;
  slot.auto_reload = config.auto_reload;
  slot.suspended = false;
  slot.running = false;
  slot.pending_delete = false;
  slot.delay_requested = false;

  if (StartTimer(slot, slot.default_start_delay_ms) ==
      SoftTimerService::kInvalidTaskHandle) {
    ClearTaskSlot(slot_index);
    return kInvalidTaskHandle;
  }

  return slot.handle;
}

TaskHandle TaskCreatePeriodic(TaskCallback callback,
                              void *context,
                              uint32_t period_ms,
                              uint8_t priority,
                              uint32_t start_delay_ms,
                              const char *name) {
  TaskConfig config {}; // 周期任务创建配置。
  config.name = name;
  config.callback = callback;
  config.context = context;
  config.period_ms = period_ms;
  config.start_delay_ms = start_delay_ms;
  config.priority = priority;
  config.auto_reload = true;
  return TaskCreate(config);
}

TaskHandle TaskCreateOneShot(TaskCallback callback,
                             void *context,
                             uint32_t delay_ms,
                             uint8_t priority,
                             const char *name) {
  TaskConfig config {}; // 单次任务创建配置。
  config.name = name;
  config.callback = callback;
  config.context = context;
  config.period_ms = 0U;
  config.start_delay_ms = delay_ms;
  config.priority = priority;
  config.auto_reload = false;
  return TaskCreate(config);
}

bool TaskDelete(TaskHandle handle) {
  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = Runtime().tasks[static_cast<uint8_t>(slot_index)]; // 目标任务槽位。
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;

  if (slot.running) {
    slot.pending_delete = true;
    if (slot.timer_handle != SoftTimerService::kInvalidTaskHandle) {
      (void)SoftTimerService::Instance().DeleteTask(slot.timer_handle);
      slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
    }
    return true;
  }

  (void)StopTimer(slot);
  ClearTaskSlot(static_cast<uint8_t>(slot_index));
  return true;
}

void TaskDeleteAll() {
  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    TaskSlot &slot = runtime.tasks[slot_index]; // 当前遍历到的任务槽位。
    if (!slot.allocated) {
      continue;
    }

    slot.delay_requested = false;
    slot.requested_delay_ms = 0U;

    if (slot.running) {
      slot.pending_delete = true;
      if (slot.timer_handle != SoftTimerService::kInvalidTaskHandle) {
        (void)SoftTimerService::Instance().DeleteTask(slot.timer_handle);
        slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
      }
      continue;
    }

    (void)StopTimer(slot);
    ClearTaskSlot(slot_index);
  }
}

bool TaskDelay(TaskHandle handle, uint32_t delay_ms) {
  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  TaskSlot &slot = runtime.tasks[static_cast<uint8_t>(slot_index)]; // 目标任务槽位。

  if (slot.running && (runtime.current_task_handle == handle) &&
      (runtime.current_task_index == static_cast<uint8_t>(slot_index))) {
    slot.requested_delay_ms = delay_ms;
    slot.delay_requested = true;
    return true;
  }

  slot.pending_delete = false;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;
  slot.suspended = false;

  return RearmTimer(slot, delay_ms);
}

bool TaskDelayCurrent(uint32_t delay_ms) {
  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  if ((runtime.current_task_handle == kInvalidTaskHandle) ||
      (runtime.current_task_index >= kMaxTasks)) {
    return false;
  }

  TaskSlot &slot = runtime.tasks[runtime.current_task_index]; // 当前正在执行的任务槽位。
  if (!IsHandleMatch(slot, runtime.current_task_handle,
                     runtime.current_task_index) ||
      !slot.running) {
    return false;
  }

  slot.requested_delay_ms = delay_ms;
  slot.delay_requested = true;
  return true;
}

bool TaskSuspend(TaskHandle handle) {
  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = Runtime().tasks[static_cast<uint8_t>(slot_index)]; // 目标任务槽位。
  if (slot.suspended) {
    return true;
  }

  slot.suspended = true;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;

  if (slot.running) {
    if (slot.timer_handle != SoftTimerService::kInvalidTaskHandle) {
      (void)SoftTimerService::Instance().DeleteTask(slot.timer_handle);
      slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
    }
    return true;
  }

  return StopTimer(slot);
}

bool TaskResume(TaskHandle handle, uint32_t delay_ms) {
  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = Runtime().tasks[static_cast<uint8_t>(slot_index)]; // 目标任务槽位。
  if (!slot.suspended &&
      (slot.timer_handle != SoftTimerService::kInvalidTaskHandle)) {
    return true;
  }

  const uint32_t resolved_delay =
      (delay_ms == kUsePeriodAsStartDelay) ? slot.default_start_delay_ms
                                           : delay_ms; // 恢复任务后实际使用的启动延时。

  slot.pending_delete = false;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;
  slot.suspended = false;

  return StartTimer(slot, resolved_delay) !=
         SoftTimerService::kInvalidTaskHandle;
}

bool TaskIsAlive(TaskHandle handle) {
  return FindSlotIndex(handle) >= 0;
}

bool TaskIsSuspended(TaskHandle handle) {
  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  return Runtime().tasks[static_cast<uint8_t>(slot_index)].suspended;
}

bool TaskGetInfo(TaskHandle handle, TaskInfo *info) {
  if (info == nullptr) {
    return false;
  }

  const int16_t slot_index = FindSlotIndex(handle); // 目标任务槽位索引。
  if (slot_index < 0) {
    return false;
  }

  const TaskSlot &slot =
      Runtime().tasks[static_cast<uint8_t>(slot_index)]; // 目标任务槽位快照。
  info->name = slot.name;
  info->period_ms = slot.period_ms;
  info->priority = slot.priority;
  info->auto_reload = slot.auto_reload;
  info->suspended = slot.suspended;
  return true;
}

uint32_t TaskCount() {
  TaskRuntimeState &runtime = Runtime(); // 任务模块运行时状态表。
  uint32_t count = 0U; // 当前已分配任务数量。
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    if (runtime.tasks[slot_index].allocated) {
      ++count;
    }
  }

  return count;
}

uint32_t TaskDispatch() {
  return SoftTimerService::Instance().Dispatch();
}

uint32_t TaskNow() {
  return SoftTimerService::Instance().Now();
}

} // namespace iFly

