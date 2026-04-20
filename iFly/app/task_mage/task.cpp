// 任务管理模块实现。
// 负责任务槽位、句柄校验、底层定时器重装和任务生命周期管理。
#include "task.hpp"

namespace iFly::task {

// 返回单例实例。
TaskManager &TaskManager::Instance()
{
  // 函数内静态对象实现单例，避免全局初始化顺序问题。
  static TaskManager instance;
  return instance;
}

// 创建一个任务槽位并启动定时触发。
TaskManager::TaskHandle TaskManager::CreateTask(const TaskConfig &config)
{
  // 没有回调就没有创建意义，直接拒绝。
  if (config.callback == nullptr) {
    return kInvalidTaskHandle;
  }

  const uint8_t slot_index = FindFreeSlot();
  if (slot_index >= kMaxTasks) {
    return kInvalidTaskHandle;
  }

  TaskSlot &slot = tasks_[slot_index];

  // 代数递增，用于防止旧句柄在槽位复用后误操作新任务。
  uint32_t generation = slot.generation + 1U;
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

  // 上层任务本质上是一个“一次性触发后进入统一入口”的 SoftTimer 任务。
  if (StartTimer(slot, slot.default_start_delay_ms) ==
      SoftTimerService::kInvalidTaskHandle) {
    ClearTaskSlot(slot_index);
    return kInvalidTaskHandle;
  }

  return slot.handle;
}

// 创建周期任务。
TaskManager::TaskHandle TaskManager::CreatePeriodicTask(
    TaskCallback callback, void *context, uint32_t period_ms, uint8_t priority,
    uint32_t start_delay_ms, const char *name)
{
  TaskConfig config {};
  config.name = name;
  config.callback = callback;
  config.context = context;
  config.period_ms = period_ms;
  config.start_delay_ms = start_delay_ms;
  config.priority = priority;
  config.auto_reload = true;
  return CreateTask(config);
}

// 创建一次性任务。
TaskManager::TaskHandle TaskManager::CreateOneShotTask(
    TaskCallback callback, void *context, uint32_t delay_ms, uint8_t priority,
    const char *name)
{
  TaskConfig config {};
  config.name = name;
  config.callback = callback;
  config.context = context;
  config.period_ms = 0U;
  config.start_delay_ms = delay_ms;
  config.priority = priority;
  config.auto_reload = false;
  return CreateTask(config);
}

// 删除指定任务。
bool TaskManager::DeleteTask(TaskHandle handle)
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;

  if (slot.running) {
    // 若当前任务正处于回调中，则标记为“回调返回后删除”。
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

// 删除所有任务。
void TaskManager::DeleteAllTasks()
{
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    TaskSlot &slot = tasks_[slot_index];
    if (!slot.allocated) {
      continue;
    }

    slot.delay_requested = false;
    slot.requested_delay_ms = 0U;

    if (slot.running) {
      // 正在执行的任务不能立刻清槽，延后到回调退出后处理。
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

// 重设指定任务的下一次触发延时。
bool TaskManager::DelayTask(TaskHandle handle, uint32_t delay_ms)
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];

  // 如果目标任务正是当前回调中的任务，则记录请求，等回调退出后再重装。
  if (slot.running && (current_task_handle_ == handle) &&
      (current_task_index_ == static_cast<uint8_t>(slot_index))) {
    slot.requested_delay_ms = delay_ms;
    slot.delay_requested = true;
    return true;
  }

  slot.pending_delete = false;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;
  slot.suspended = false;

  // 对未运行的任务，直接重装底层定时器即可。
  return RearmTimer(slot, delay_ms);
}

// 请求延后当前正在执行的任务。
bool TaskManager::DelayCurrentTask(uint32_t delay_ms)
{
  if ((current_task_handle_ == kInvalidTaskHandle) ||
      (current_task_index_ >= kMaxTasks)) {
    return false;
  }

  TaskSlot &slot = tasks_[current_task_index_];
  if (!IsHandleMatch(slot, current_task_handle_, current_task_index_) ||
      !slot.running) {
    return false;
  }

  slot.requested_delay_ms = delay_ms;
  slot.delay_requested = true;
  return true;
}

// 挂起指定任务。
bool TaskManager::SuspendTask(TaskHandle handle)
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];
  if (slot.suspended) {
    return true;
  }

  slot.suspended = true;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;

  if (slot.running) {
    // 若任务当前正在执行，只取消后续触发，不打断本次回调。
    if (slot.timer_handle != SoftTimerService::kInvalidTaskHandle) {
      (void)SoftTimerService::Instance().DeleteTask(slot.timer_handle);
      slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
    }
    return true;
  }

  return StopTimer(slot);
}

// 恢复挂起任务并重新安排触发时间。
bool TaskManager::ResumeTask(TaskHandle handle, uint32_t delay_ms)
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];
  if (!slot.suspended &&
      (slot.timer_handle != SoftTimerService::kInvalidTaskHandle)) {
    return true;
  }

  const uint32_t resolved_delay =
      (delay_ms == kUsePeriodAsStartDelay) ? slot.default_start_delay_ms
                                           : delay_ms;

  slot.pending_delete = false;
  slot.delay_requested = false;
  slot.requested_delay_ms = 0U;
  slot.suspended = false;

  return StartTimer(slot, resolved_delay) !=
         SoftTimerService::kInvalidTaskHandle;
}

// 检查任务句柄是否仍然有效。
bool TaskManager::IsTaskAlive(TaskHandle handle) const
{
  return FindSlotIndex(handle) >= 0;
}

// 检查任务是否处于挂起状态。
bool TaskManager::IsTaskSuspended(TaskHandle handle) const
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  return tasks_[static_cast<uint8_t>(slot_index)].suspended;
}

// 读取任务当前信息。
bool TaskManager::GetTaskInfo(TaskHandle handle, TaskInfo *info) const
{
  if (info == nullptr) {
    return false;
  }

  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  const TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];
  info->name = slot.name;
  info->period_ms = slot.period_ms;
  info->priority = slot.priority;
  info->auto_reload = slot.auto_reload;
  info->suspended = slot.suspended;
  return true;
}

// 统计当前已分配的任务数量。
uint32_t TaskManager::TaskCount() const
{
  uint32_t count = 0U;

  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    if (tasks_[slot_index].allocated) {
      ++count;
    }
  }

  return count;
}

// 派发当前已到期的任务。
uint32_t TaskManager::Dispatch()
{
  return SoftTimerService::Instance().Dispatch();
}

// 返回当前时基计数值。
uint32_t TaskManager::Now() const
{
  return SoftTimerService::Instance().Now();
}

// 作为底层定时器入口转发到实际任务回调。
void TaskManager::TaskEntry(void *context)
{
  TaskSlot *slot = reinterpret_cast<TaskSlot *>(context);
  if ((slot == nullptr) || !slot->allocated || (slot->slot_index >= kMaxTasks)) {
    return;
  }

  TaskManager &manager = Instance();
  const uint8_t slot_index = slot->slot_index;
  const TaskHandle handle = slot->handle;

  manager.current_task_handle_ = handle;
  manager.current_task_index_ = slot_index;
  slot->running = true;

  if (slot->callback != nullptr) {
    slot->callback(slot->context);
  }

  manager.current_task_handle_ = kInvalidTaskHandle;
  manager.current_task_index_ = kMaxTasks;

  // 若回调期间该任务已经被删除/复用，则不再继续处理后续逻辑。
  if (!manager.IsHandleMatch(*slot, handle, slot_index)) {
    return;
  }

  slot->running = false;

  if (slot->pending_delete) {
    manager.ClearTaskSlot(slot_index);
    return;
  }

  if (slot->suspended) {
    slot->timer_handle = SoftTimerService::kInvalidTaskHandle;
    return;
  }

  if (slot->delay_requested) {
    const uint32_t delay_ms = slot->requested_delay_ms;
    slot->delay_requested = false;
    slot->requested_delay_ms = 0U;

    // 这里依赖底层“当前任务延时”能力，把下一次唤醒时间交给 SoftTimer。
    if (SoftTimerService::Instance().DelayCurrentTask(delay_ms)) {
      return;
    }

    manager.ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload && (slot->period_ms > 0U)) {
    // 固定周期任务自动按周期重装。
    if (SoftTimerService::Instance().DelayCurrentTask(slot->period_ms)) {
      return;
    }

    manager.ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload) {
    // 手动重装任务执行一次后进入挂起态，保留句柄供后续再次启动。
    slot->suspended = true;
    slot->timer_handle = SoftTimerService::kInvalidTaskHandle;
    return;
  }

  // 单次任务执行完成后直接释放。
  manager.ClearTaskSlot(slot_index);
}

// 构造带代数信息的任务句柄。
TaskManager::TaskHandle TaskManager::MakeTaskHandle(uint8_t slot_index,
                                                    uint32_t generation)
{
  return (generation << 16U) | static_cast<uint32_t>(slot_index + 1U);
}

// 从句柄中提取槽位索引。
uint8_t TaskManager::ExtractTaskIndex(TaskHandle handle)
{
  return static_cast<uint8_t>((handle & 0xFFFFU) - 1U);
}

// 从句柄中提取代数计数。
uint32_t TaskManager::ExtractTaskGeneration(TaskHandle handle)
{
  return handle >> 16U;
}

// 检查任务句柄是否有效。
bool TaskManager::IsValidTaskHandle(TaskHandle handle)
{
  return handle != kInvalidTaskHandle;
}

// 解析任务的首次启动延时。
uint32_t TaskManager::ResolveStartDelayMs(const TaskConfig &config)
{
  if (config.start_delay_ms != kUsePeriodAsStartDelay) {
    return config.start_delay_ms;
  }

  return (config.period_ms > 0U) ? config.period_ms : 0U;
}

// 为任务启动底层定时器。
SoftTimerService::TaskHandle TaskManager::StartTimer(TaskSlot &slot,
                                                     uint32_t delay_ms)
{
  SoftTimerService::TaskConfig config {};
  config.callback = &TaskEntry;
  config.context = &slot;
  config.interval_ms = 0U;
  config.start_delay_ms = delay_ms;
  config.priority = slot.priority;
  config.auto_reload = true;

  const SoftTimerService::TaskHandle timer_handle =
      SoftTimerService::Instance().CreateTask(config);
  if (timer_handle == SoftTimerService::kInvalidTaskHandle) {
    return SoftTimerService::kInvalidTaskHandle;
  }

  slot.timer_handle = timer_handle;
  slot.suspended = false;
  return timer_handle;
}

// 停止任务关联的底层定时器。
bool TaskManager::StopTimer(TaskSlot &slot)
{
  if (slot.timer_handle == SoftTimerService::kInvalidTaskHandle) {
    return true;
  }

  const bool result = SoftTimerService::Instance().DeleteTask(slot.timer_handle);
  if (result) {
    slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
  }

  return result;
}

// 按新延时重装底层定时器。
bool TaskManager::RearmTimer(TaskSlot &slot, uint32_t delay_ms)
{
  if (!StopTimer(slot)) {
    return false;
  }

  return StartTimer(slot, delay_ms) != SoftTimerService::kInvalidTaskHandle;
}

// 查找空闲任务槽位。
uint8_t TaskManager::FindFreeSlot() const
{
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    if (!tasks_[slot_index].allocated) {
      return slot_index;
    }
  }

  return kMaxTasks;
}

// 根据句柄查找任务槽位索引。
int16_t TaskManager::FindSlotIndex(TaskHandle handle) const
{
  if (!IsValidTaskHandle(handle)) {
    return -1;
  }

  const uint8_t slot_index = ExtractTaskIndex(handle);
  if (slot_index >= kMaxTasks) {
    return -1;
  }

  return IsHandleMatch(tasks_[slot_index], handle, slot_index) ? slot_index : -1;
}

// 校验槽位与句柄是否匹配。
bool TaskManager::IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                                uint8_t slot_index) const
{
  return slot.allocated && (slot.slot_index == slot_index) &&
         (slot.generation == ExtractTaskGeneration(handle));
}

// 清空指定槽位。
void TaskManager::ClearTaskSlot(uint8_t slot_index)
{
  TaskSlot &slot = tasks_[slot_index];
  slot.name = nullptr;
  slot.callback = nullptr;
  slot.context = nullptr;
  slot.handle = kInvalidTaskHandle;
  slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
  slot.period_ms = 0U;
  slot.default_start_delay_ms = 0U;
  slot.requested_delay_ms = 0U;
  slot.priority = SoftTimerService::kLowestPriority;
  slot.slot_index = kMaxTasks;
  slot.allocated = false;
  slot.auto_reload = true;
  slot.suspended = false;
  slot.running = false;
  slot.pending_delete = false;
  slot.delay_requested = false;
}

} // namespace iFly::task
