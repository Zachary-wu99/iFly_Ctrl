#include "task.hpp"

namespace iFly {

TaskManager &TaskManager::Instance()
{
  static TaskManager instance;
  return instance;
}

TaskManager::TaskHandle TaskManager::CreateTask(const TaskConfig &config)
{
  if (config.callback == nullptr) {
    return kInvalidTaskHandle;
  }

  const uint8_t slot_index = FindFreeSlot();
  if (slot_index >= kMaxTasks) {
    return kInvalidTaskHandle;
  }

  TaskSlot &slot = tasks_[slot_index];

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

  if (StartTimer(slot, slot.default_start_delay_ms) ==
      SoftTimerService::kInvalidTaskHandle) {
    ClearTaskSlot(slot_index);
    return kInvalidTaskHandle;
  }

  return slot.handle;
}

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

bool TaskManager::DelayTask(TaskHandle handle, uint32_t delay_ms)
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  TaskSlot &slot = tasks_[static_cast<uint8_t>(slot_index)];

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

  return RearmTimer(slot, delay_ms);
}

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
    if (slot.timer_handle != SoftTimerService::kInvalidTaskHandle) {
      (void)SoftTimerService::Instance().DeleteTask(slot.timer_handle);
      slot.timer_handle = SoftTimerService::kInvalidTaskHandle;
    }
    return true;
  }

  return StopTimer(slot);
}

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

bool TaskManager::IsTaskAlive(TaskHandle handle) const
{
  return FindSlotIndex(handle) >= 0;
}

bool TaskManager::IsTaskSuspended(TaskHandle handle) const
{
  const int16_t slot_index = FindSlotIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  return tasks_[static_cast<uint8_t>(slot_index)].suspended;
}

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

uint32_t TaskManager::Dispatch()
{
  return SoftTimerService::Instance().Dispatch();
}

uint32_t TaskManager::Now() const
{
  return SoftTimerService::Instance().Now();
}

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

    if (SoftTimerService::Instance().DelayCurrentTask(delay_ms)) {
      return;
    }

    manager.ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload && (slot->period_ms > 0U)) {
    if (SoftTimerService::Instance().DelayCurrentTask(slot->period_ms)) {
      return;
    }

    manager.ClearTaskSlot(slot_index);
    return;
  }

  if (slot->auto_reload) {
    slot->suspended = true;
    slot->timer_handle = SoftTimerService::kInvalidTaskHandle;
    return;
  }

  manager.ClearTaskSlot(slot_index);
}

TaskManager::TaskHandle TaskManager::MakeTaskHandle(uint8_t slot_index,
                                                    uint32_t generation)
{
  return (generation << 16U) | static_cast<uint32_t>(slot_index + 1U);
}

uint8_t TaskManager::ExtractTaskIndex(TaskHandle handle)
{
  return static_cast<uint8_t>((handle & 0xFFFFU) - 1U);
}

uint32_t TaskManager::ExtractTaskGeneration(TaskHandle handle)
{
  return handle >> 16U;
}

bool TaskManager::IsValidTaskHandle(TaskHandle handle)
{
  return handle != kInvalidTaskHandle;
}

uint32_t TaskManager::ResolveStartDelayMs(const TaskConfig &config)
{
  if (config.start_delay_ms != kUsePeriodAsStartDelay) {
    return config.start_delay_ms;
  }

  return (config.period_ms > 0U) ? config.period_ms : 0U;
}

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

bool TaskManager::RearmTimer(TaskSlot &slot, uint32_t delay_ms)
{
  if (!StopTimer(slot)) {
    return false;
  }

  return StartTimer(slot, delay_ms) != SoftTimerService::kInvalidTaskHandle;
}

uint8_t TaskManager::FindFreeSlot() const
{
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    if (!tasks_[slot_index].allocated) {
      return slot_index;
    }
  }

  return kMaxTasks;
}

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

bool TaskManager::IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                                uint8_t slot_index) const
{
  return slot.allocated && (slot.slot_index == slot_index) &&
         (slot.generation == ExtractTaskGeneration(handle));
}

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
