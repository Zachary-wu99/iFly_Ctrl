---
title: 时间与调度实现
description: systick_timer、soft_timer 与 task_mage 的实现关系和关键代码注解
---

# 时间与调度实现

## 时间基准链路

时间相关模块的依赖关系如下：

```text
SysTick 中断
  -> SysTickNsTimer::OnSysTick()
  -> SoftTimerService::OnSysTick()

systick_time
  -> HAL_GetTick() 提供 ms
  -> DWT->CYCCNT + 回绕扩展提供 us/ns

tick
  -> 对 systick_time 再封装

task_mage
  -> 对 SoftTimerService 再封装
```

## `SysTickNsTimer`

`SysTickNsTimer` 使用 `DWT->CYCCNT` 获取 CPU 周期计数，再结合 `wrap_high_` 把 32 位计数扩展为 64 位时间源。

关键代码如下：

```cpp
uint64_t SysTickNsTimer::NowTicks() const
{
  EnsureEnabled();

  const uint32_t high_before = wrap_high_;
  uint32_t current_cycles = DWT->CYCCNT;
  const uint32_t high_after = wrap_high_;

  if (high_before != high_after) {
    current_cycles = DWT->CYCCNT;
    return (static_cast<uint64_t>(high_after) << 32U) | current_cycles;
  }

  if (current_cycles < last_cycle_sample_) {
    return (static_cast<uint64_t>(high_before + 1U) << 32U) | current_cycles;
  }

  return (static_cast<uint64_t>(high_before) << 32U) | current_cycles;
}
```

代码含义如下：

- `EnsureEnabled()` 负责开启 `DWT` 计数器。
- `wrap_high_` 保存高 32 位回绕计数。
- 读取期间如果 `wrap_high_` 发生变化，说明 `SysTick` 已处理回绕，需要重新拼接高低位。
- 如果本次读到的 `DWT->CYCCNT` 小于上次采样值，说明硬件计数器刚刚回绕，但 `SysTick` 还未更新高位，函数会临时补上 `+1`。

## `SoftTimerService`

`SoftTimerService` 维护固定大小的 `tasks_[16]` 数组，每个任务槽保存回调、周期、优先级、下一次释放时刻和运行状态。

调度流程的核心逻辑如下：

```cpp
ready_index = FindReadyTaskIndex(tick_count_);
if (ready_index == kMaxTasks) {
  break;
}

TaskSlot &slot = tasks_[ready_index];
slot.running = true;
callback = slot.callback;
context = slot.context;

if (callback != nullptr) {
  callback(context);
}

slot.running = false;

if (slot.pending_delete || !slot.auto_reload) {
  ClearTaskSlot(ready_index);
  continue;
}

if (slot.interval_ms > 0U) {
  slot.next_release_tick = tick_count_ + slot.interval_ms;
  continue;
}
```

代码含义如下：

- `FindReadyTaskIndex()` 会在所有已分配任务里选择一个已到期任务。
- 选择顺序先比较优先级，再比较创建顺序。
- 回调始终在主循环中执行，不在中断中执行。
- 周期任务在回调返回后根据 `interval_ms` 重新装载下一次触发时间。
- 单次任务或待删除任务在回调返回后清理任务槽。

## `task_mage`

`task_mage/task.cpp` 没有重新实现调度器，而是把 `SoftTimerService` 封装为更接近应用层语义的接口：

- `TaskCreatePeriodic()` 对应周期任务
- `TaskCreateOneShot()` 对应单次任务
- `TaskDelayCurrent()` 对应当前任务延后执行
- `TaskDispatch()` 直接调用 `SoftTimerService::Dispatch()`

## 当前主循环行为

当前 `app_main()` 的实现如下：

```cpp
extern "C" void app_main(void)
{
  (void)iFly::InitAllTasks();
  while (1) {
    (void)iFly::TaskDispatch();
  }
}
```

这意味着：

- 所有任务都由主循环串行分发
- 任务回调之间不存在抢占式切换
- 回调执行时间会直接影响后续任务的分发时刻
