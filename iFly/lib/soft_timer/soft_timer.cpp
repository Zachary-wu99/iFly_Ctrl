#include "soft_timer.hpp"

#include "stm32f4xx_hal.h"

namespace {

/**
 * @brief 简单的关中断保护对象。
 * @details
 * 构造时保存当前 PRIMASK 并关中断，析构时恢复原始状态。
 * 这里用于保护任务表和软件 tick 的并发访问。
 */
class IrqGuard final {
public:
  IrqGuard() noexcept : primask_(__get_PRIMASK())
  {
    __disable_irq();
  }

  ~IrqGuard()
  {
    __set_PRIMASK(primask_);
  }

private:
  uint32_t primask_;
};

} // namespace

namespace iFly {

SoftTimerService &SoftTimerService::Instance() noexcept
{
  // 使用函数内静态对象实现单例，避免全局初始化顺序问题。
  static SoftTimerService instance;
  return instance;
}

SoftTimerService::TaskHandle SoftTimerService::CreateTask(const TaskConfig &config) noexcept
{
  // 回调为空或周期为 0 都属于非法配置，直接拒绝创建。
  if ((config.callback == nullptr) || (config.interval_ms == 0U)) {
    return kInvalidTaskHandle;
  }

  // 默认情况下，首次触发延时与周期保持一致。
  const uint32_t first_delay_ms =
      (config.start_delay_ms == kUseIntervalAsStartDelay) ? config.interval_ms : config.start_delay_ms;

  IrqGuard irq_guard;

  // 在固定槽位数组中寻找空闲位置，不做动态内存分配。
  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    TaskSlot &slot = tasks_[slot_index];
    if (slot.allocated) {
      continue;
    }

    // generation 用来避免旧句柄在槽位复用后误删新任务。
    uint32_t generation = slot.generation + 1U;
    if (generation == 0U) {
      generation = 1U;
    }

    // creation_order 用于同优先级任务的稳定排序。
    uint32_t creation_order = ++creation_counter_;
    if (creation_order == 0U) {
      creation_order = ++creation_counter_;
    }

    slot.callback = config.callback;
    slot.context = config.context;
    slot.interval_ms = config.interval_ms;
    slot.next_release_tick = tick_count_ + first_delay_ms;
    slot.creation_order = creation_order;
    slot.generation = generation;
    slot.priority = config.priority;
    slot.allocated = true;
    slot.auto_reload = config.auto_reload;
    slot.running = false;
    slot.pending_delete = false;
    return MakeTaskHandle(slot_index, generation);
  }

  return kInvalidTaskHandle;
}

bool SoftTimerService::DeleteTask(TaskHandle handle) noexcept
{
  if (!IsValidTaskHandle(handle)) {
    return false;
  }

  IrqGuard irq_guard;
  const uint8_t slot_index = ExtractTaskIndex(handle);
  TaskSlot &slot = tasks_[slot_index];
  if (!IsHandleMatch(slot, handle, slot_index)) {
    return false;
  }

  // 若任务正在执行，不能立即清槽，否则可能破坏当前回调上下文。
  if (slot.running) {
    slot.pending_delete = true;
    slot.auto_reload = false;
    return true;
  }

  ClearTaskSlot(slot_index);
  return true;
}

void SoftTimerService::DeleteAllTasks() noexcept
{
  IrqGuard irq_guard;

  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    TaskSlot &slot = tasks_[slot_index];
    if (!slot.allocated) {
      continue;
    }

    // 正在执行的任务延迟到回调返回后再删除。
    if (slot.running) {
      slot.pending_delete = true;
      slot.auto_reload = false;
      continue;
    }

    ClearTaskSlot(slot_index);
  }
}

uint32_t SoftTimerService::Dispatch() noexcept
{
  // 非抢占式调度器不允许自身重入。
  if (running_dispatch_) {
    return 0U;
  }

  running_dispatch_ = true;
  uint32_t executed_count = 0U;

  // 循环取出当前所有到期任务，直到没有就绪任务为止。
  while (true) {
    uint8_t ready_index = kMaxTasks;
    TaskCallback callback = nullptr;
    void *context = nullptr;
    TaskHandle handle = kInvalidTaskHandle;

    {
      IrqGuard irq_guard;
      ready_index = FindReadyTaskIndex(tick_count_);
      if (ready_index == kMaxTasks) {
        break;
      }

      TaskSlot &slot = tasks_[ready_index];
      // 标记为运行中，防止被立即删除或重复选中。
      slot.running = true;
      callback = slot.callback;
      context = slot.context;
      handle = MakeTaskHandle(ready_index, slot.generation);
    }

    // 真正的用户回调始终在中断外执行。
    if (callback != nullptr) {
      callback(context);
      ++executed_count;
    }

    {
      IrqGuard irq_guard;
      TaskSlot &slot = tasks_[ready_index];
      if (!IsHandleMatch(slot, handle, ready_index)) {
        continue;
      }

      slot.running = false;

      // 单次任务或被请求删除的任务在这里统一回收。
      if (slot.pending_delete || !slot.auto_reload) {
        ClearTaskSlot(ready_index);
        continue;
      }

      // 周期任务以下一次“当前时刻 + 周期”重新装载。
      slot.next_release_tick = tick_count_ + slot.interval_ms;
    }
  }

  running_dispatch_ = false;
  return executed_count;
}

uint32_t SoftTimerService::Now() const noexcept
{
  return tick_count_;
}

void SoftTimerService::OnSysTick() noexcept
{
  // 1ms 软时基累加，保持 ISR 足够轻量。
  ++tick_count_;
}

bool SoftTimerService::IsValidTaskHandle(TaskHandle handle) noexcept
{
  return handle != kInvalidTaskHandle;
}

SoftTimerService::TaskHandle SoftTimerService::MakeTaskHandle(uint8_t slot_index, uint32_t generation) noexcept
{
  return (generation << 16U) | static_cast<uint32_t>(slot_index + 1U);
}

uint8_t SoftTimerService::ExtractTaskIndex(TaskHandle handle) noexcept
{
  return static_cast<uint8_t>((handle & 0xFFFFU) - 1U);
}

uint32_t SoftTimerService::ExtractTaskGeneration(TaskHandle handle) noexcept
{
  return handle >> 16U;
}

uint8_t SoftTimerService::FindReadyTaskIndex(uint32_t now) const noexcept
{
  uint8_t best_index = kMaxTasks;

  for (uint8_t slot_index = 0U; slot_index < kMaxTasks; ++slot_index) {
    const TaskSlot &slot = tasks_[slot_index];
    if (!slot.allocated || slot.running) {
      continue;
    }

    // 用带符号减法处理 tick 回绕后的“是否到期”判断。
    if (static_cast<int32_t>(now - slot.next_release_tick) < 0) {
      continue;
    }

    if (best_index == kMaxTasks) {
      best_index = slot_index;
      continue;
    }

    const TaskSlot &best_slot = tasks_[best_index];

    // 先比较优先级，数值越小优先级越高。
    if (slot.priority < best_slot.priority) {
      best_index = slot_index;
      continue;
    }

    // 优先级相同则保持先创建先执行，保证调度顺序稳定。
    if ((slot.priority == best_slot.priority) && (slot.creation_order < best_slot.creation_order)) {
      best_index = slot_index;
    }
  }

  return best_index;
}

bool SoftTimerService::IsHandleMatch(const TaskSlot &slot, TaskHandle handle, uint8_t slot_index) const noexcept
{
  return slot.allocated && (slot.generation == ExtractTaskGeneration(handle)) &&
         (slot_index == ExtractTaskIndex(handle));
}

void SoftTimerService::ClearTaskSlot(uint8_t slot_index) noexcept
{
  // 保留 generation，不清零，这样旧句柄不会重新匹配到后续复用任务。
  TaskSlot &slot = tasks_[slot_index];
  slot.callback = nullptr;
  slot.context = nullptr;
  slot.interval_ms = 0U;
  slot.next_release_tick = 0U;
  slot.creation_order = 0U;
  slot.priority = kLowestPriority;
  slot.allocated = false;
  slot.auto_reload = true;
  slot.running = false;
  slot.pending_delete = false;
}

} // namespace iFly

extern "C" void ifly_soft_timer_systick_tick(void)
{
  // C 中断入口只转发到 C++ 单例，不在中断里跑任务。
  iFly::SoftTimerService::Instance().OnSysTick();
}
