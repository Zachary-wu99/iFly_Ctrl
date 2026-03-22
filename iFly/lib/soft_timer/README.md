# soft_timer 使用说明

## 1. 模块目标

`soft_timer` 是一个基于 `SysTick` 的轻量级软件定时器模块，适合当前这个裸机/主循环工程。

它解决的是这类需求：

- 想创建多个周期任务或单次任务
- 想在创建任务时指定周期和优先级
- 不希望在中断里直接跑复杂业务
- 不希望引入 RTOS

这个模块的核心思想很简单：

- `SysTick` 中断每 `1ms` 只负责把软件时间 `tick` 加一
- 主循环里调用 `Dispatch()`，把已经到期的任务按规则执行
- 回调函数都在主循环上下文中执行，因此是非抢占式的

## 2. 目录与文件

- `soft_timer.hpp`
  对外接口定义，业务代码主要包含这个头文件
- `soft_timer.cpp`
  调度器具体实现

## 3. 实现过程说明

### 3.1 为什么不用在 SysTick 中断里直接执行任务

如果直接在 `SysTick` 中断里跑任务，会有几个问题：

- 回调一旦执行太久，会拉长中断时间
- 会影响其他中断响应
- 业务逻辑和中断上下文耦合太重，不利于维护
- USB、串口等模块里一旦有阻塞或复杂处理，很容易出问题

所以这里采用“两段式”设计：

1. `SysTick_Handler()` 中只调用 `ifly_soft_timer_systick_tick()`
2. `ifly_soft_timer_systick_tick()` 内部只执行 `tick_count_++`
3. 主循环里调用 `SoftTimerService::Dispatch()`
4. `Dispatch()` 找出已到期任务并调用回调

这样做的好处是：

- 中断保持很轻
- 任务逻辑都在主循环跑，行为更可控
- 很适合当前工程的裸机结构

### 3.2 为什么使用固定任务表

当前实现用的是固定大小数组：

```cpp
TaskSlot tasks_[kMaxTasks] {};
```

而不是运行时动态创建链表或容器。

原因是：

- 嵌入式环境下更稳定
- 不引入堆内存碎片
- 行为确定，调试简单
- 任务数量可控

当前默认最大任务数是：

```cpp
static constexpr uint8_t kMaxTasks = 16U;
```

如果后面任务数量增加，可以直接改这个值。

### 3.3 任务句柄为什么带 generation

任务句柄不是简单的数组下标，而是：

- 槽位索引
- generation 代数

组合出来的一个 `uint32_t`。

这样做是为了避免下面这种错误：

1. 任务 A 使用了槽位 3
2. 任务 A 被删除
3. 任务 B 又复用了槽位 3
4. 外部还拿着任务 A 的旧句柄去删除

如果只存下标，旧句柄就会误删任务 B。

引入 `generation` 后，旧句柄和新任务就不会匹配。

### 3.4 调度规则

`Dispatch()` 每次执行时，会不断扫描任务表，找出“当前最应该执行”的任务。

筛选规则如下：

1. 任务必须已经到期
2. 优先级数值越小越先执行
3. 若优先级相同，则创建时间更早的任务先执行

也就是说：

- `priority = 0` 比 `priority = 5` 更高
- 同优先级下，先创建的任务先跑

### 3.5 为什么是非抢占式

当一个任务回调开始执行后，调度器不会在它中途切去执行另一个任务。

这就是非抢占式的含义。

优点：

- 行为简单
- 不需要复杂上下文切换
- 很适合短小周期任务

注意点：

- 任务回调要尽量短
- 不要在回调里长时间阻塞
- 不要在回调里做大循环或长时间等待

否则会影响其他任务的调度及时性。

### 3.6 删除正在执行的任务时怎么处理

如果调用 `DeleteTask()` 时，目标任务正在回调里执行，模块不会立刻把槽位清掉，而是：

- 先标记 `pending_delete = true`
- 同时关闭 `auto_reload`
- 等当前回调返回后再真正删除

这样可以避免：

- 回调执行过程中槽位被提前清空
- 当前执行上下文被破坏

## 4. 已接入的位置

### 4.1 SysTick 中断

在 `Core/Src/stm32f4xx_it.c` 里：

```c
void SysTick_Handler(void)
{
  HAL_IncTick();
  ifly_soft_timer_systick_tick();
}
```

### 4.2 主循环调度

在 `iFly/app_main.cpp` 的主循环里：

```cpp
iFly::SoftTimerService::Instance().Dispatch();
```

这意味着只要主循环一直运行，软件定时器任务就会被持续调度。

## 5. 对外接口说明

## 5.1 获取单例

```cpp
auto &timer = iFly::SoftTimerService::Instance();
```

## 5.2 创建任务

```cpp
iFly::SoftTimerService::TaskHandle handle = timer.CreateTask({
    .callback = MyTask,
    .context = nullptr,
    .interval_ms = 1000,
    .start_delay_ms = iFly::SoftTimerService::kUseIntervalAsStartDelay,
    .priority = 2,
    .auto_reload = true,
});
```

字段说明：

- `callback`
  任务函数
- `context`
  回调透传参数
- `interval_ms`
  周期，单位毫秒，必须大于 0
- `start_delay_ms`
  首次触发延时
- `priority`
  非抢占式优先级，数值越小越高
- `auto_reload`
  `true` 周期执行，`false` 单次执行

## 5.3 删除任务

```cpp
bool ok = timer.DeleteTask(handle);
```

## 5.4 删除全部任务

```cpp
timer.DeleteAllTasks();
```

## 5.5 获取当前软件时间

```cpp
uint32_t now = timer.Now();
```

单位是毫秒。

## 6. 使用教程

### 6.1 定义任务函数

```cpp
static void BlinkTask(void *context)
{
  (void)context;
  // 在这里写你的业务逻辑
}
```

### 6.2 创建一个周期任务

```cpp
auto &timer = iFly::SoftTimerService::Instance();

const auto blink_task = timer.CreateTask({
    .callback = BlinkTask,
    .context = nullptr,
    .interval_ms = 500,
    .start_delay_ms = iFly::SoftTimerService::kUseIntervalAsStartDelay,
    .priority = 1,
    .auto_reload = true,
});
```

效果：

- 首次在 500ms 后执行
- 之后每隔 500ms 执行一次

### 6.3 创建一个立即启动的周期任务

```cpp
const auto task = timer.CreateTask({
    .callback = BlinkTask,
    .context = nullptr,
    .interval_ms = 100,
    .start_delay_ms = 0,
    .priority = 0,
    .auto_reload = true,
});
```

效果：

- 创建后下一次 `Dispatch()` 就可以执行
- 后续每 100ms 执行一次

### 6.4 创建一个单次任务

```cpp
const auto oneshot = timer.CreateTask({
    .callback = BlinkTask,
    .context = nullptr,
    .interval_ms = 1000,
    .start_delay_ms = 1000,
    .priority = 3,
    .auto_reload = false,
});
```

效果：

- 1 秒后执行一次
- 执行完成后自动删除

### 6.5 删除任务

```cpp
if (iFly::SoftTimerService::IsValidTaskHandle(blink_task)) {
  timer.DeleteTask(blink_task);
}
```

## 7. 建议的使用方式

推荐把每个任务都写成“短小、快速返回”的函数，例如：

- 轮询一次状态机
- 发送一小段数据
- 刷新一次 LED 状态
- 周期检查一个标志位

不建议在任务回调里做这些事：

- `HAL_Delay()`
- 长时间忙等
- 大量打印
- 长时间阻塞等待外设

原因很直接：

- 调度器是非抢占式的
- 一个任务卡太久，其他任务都会被拖慢

## 8. 当前实现的边界

当前版本的特点：

- 固定最多 16 个任务
- 1ms 精度
- 非抢占式
- 不支持任务暂停/恢复
- 不支持动态修改周期和优先级

如果后面你需要，我可以继续往下扩展：

- `PauseTask()/ResumeTask()`
- `ChangePeriod()`
- `ChangePriority()`
- 统计任务执行次数
- 统计任务超时/滞后情况

## 9. 最小示例

```cpp
#include "soft_timer.hpp"

static void ExampleTask(void *context)
{
  (void)context;
  // do something
}

void CreateExample()
{
  auto &timer = iFly::SoftTimerService::Instance();
  timer.CreateTask({
      .callback = ExampleTask,
      .context = nullptr,
      .interval_ms = 1000,
      .start_delay_ms = iFly::SoftTimerService::kUseIntervalAsStartDelay,
      .priority = 1,
      .auto_reload = true,
  });
}
```

## 10. 总结

这个模块的核心是：

- 中断里只记时
- 主循环里调度
- 固定任务表管理
- 优先级调度但不抢占

这套方式对当前 `iFly_Ctrl` 工程是合适的，结构清楚，也便于你后续继续往里挂周期任务。
