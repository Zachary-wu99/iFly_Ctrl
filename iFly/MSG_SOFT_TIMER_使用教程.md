# msg 与 soft_timer 使用教程

本文档配套示例文件：

- `E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\app_main.cpp`

目标：

- 演示如何创建软件定时器
- 演示如何创建 Topic
- 演示如何发布消息
- 演示如何订阅消息
- 演示 `Copy()`、`CopyLatest()`、`DeleteTask()` 的基本使用方式

## 1. 工程中这两个模块分别做什么

### `soft_timer`

`soft_timer` 是一个基于 `SysTick` 的轻量级软件定时器服务。

它的特点：

- `SysTick` 每 1ms 递增一次内部 tick
- 主循环中调用 `Dispatch()` 后，所有到期任务会被依次执行
- 支持周期任务
- 支持一次性任务
- 支持 `DeleteTask()` 删除任务

当前工程里的关键文件：

- `E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\lib\soft_timer\soft_timer.hpp`
- `E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\lib\soft_timer\soft_timer.cpp`
- `E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\Core\Src\stm32f4xx_it.c`

### `msg`

`msg` 是一个轻量级 Topic 发布/订阅工具，风格接近 uORB。

它的特点：

- 一个 Topic 可以有多个订阅者
- 每个订阅者有自己独立的本地消息队列
- 发布时会 fan-out 到每个订阅者
- 支持逐条取消息的 `Copy()`
- 支持只取最新值的 `CopyLatest()`

当前工程里的关键文件：

- `E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\lib\msg\msg.hpp`

## 2. 先决条件

想让软件定时器正常工作，需要满足两个条件。

### 条件 1：SysTick 中断里调用 tick 接口

在 `stm32f4xx_it.c` 中，需要有下面这一句：

```cpp
ifly_soft_timer_systick_tick();
```

它的作用是：

- 每 1ms 给 `soft_timer` 递增一次时间基准

### 条件 2：主循环中不断调用 `Dispatch()`

如果只创建任务，不调用 `Dispatch()`，任务不会真正执行。

典型写法如下：

```cpp
while (1) {
  (void)iFly::SoftTimerService::Instance().Dispatch();
}
```

## 3. 示例里演示了什么

`app_main.cpp` 里演示了五件事：

1. 创建一个消息结构体 `DemoCounterMessage`
2. 创建一个 Topic `g_counter_topic`
3. 创建一个发布端 `g_counter_pub`
4. 创建两个订阅端
5. 创建多个定时器任务来演示发布、订阅、一次性任务和删除任务

## 4. 如何创建 Topic

示例代码：

```cpp
struct DemoCounterMessage final {
  uint32_t counter = 0U;
  uint32_t publish_tick_ms = 0U;
  uint32_t publish_ok_count = 0U;
};

using DemoCounterTopic = iFly::MsgTopic<DemoCounterMessage, 4U, 8U>;

DemoCounterTopic g_counter_topic("demo_counter_topic");
DemoCounterTopic::Publication g_counter_pub(g_counter_topic);
DemoCounterTopic::Subscription g_stream_sub;
DemoCounterTopic::Subscription g_latest_sub;
```

解释：

- `DemoCounterMessage` 是消息类型
- `4U` 表示最多 4 个订阅者
- `8U` 表示每个订阅者本地缓存 8 条消息
- `Publication` 用来发布消息
- `Subscription` 用来订阅消息

注意：

- 消息结构体最好只放简单字段
- 推荐使用标准布局、可平凡拷贝的数据类型

## 5. 如何订阅 Topic

示例代码：

```cpp
(void)g_stream_sub.Subscribe(g_counter_topic);
(void)g_latest_sub.Subscribe(g_counter_topic);
```

建议顺序：

1. 先建 Topic
2. 再创建订阅者
3. 先执行 `Subscribe()`
4. 再启动发布任务

这样能避免前几条消息在订阅建立前就被发掉。

## 6. 如何发布消息

示例里在 `PublishCounterMessage()` 回调中每秒发布一条消息：

```cpp
DemoCounterMessage message {};
message.counter = ++g_publish_counter;
message.publish_tick_ms = SoftTimer::Instance().Now();
message.publish_ok_count = g_publish_ok_count + 1U;

if (g_counter_pub.Publish(message)) {
  ++g_publish_ok_count;
}
```

这里的关键点：

- 先构造消息对象
- 再调用 `Publish()`
- 如果返回 `true`，表示这次发布成功

## 7. 如何创建软件定时器

底层接口是 `SoftTimerService::CreateTask()`，示例里做了一个封装函数 `CreateTimerTask()`。

底层本质配置如下：

```cpp
iFly::SoftTimerService::TaskConfig config {};
config.callback = callback;
config.context = context;
config.interval_ms = intervalMs;
config.start_delay_ms = startDelayMs;
config.priority = priority;
config.auto_reload = autoReload;

handle = iFly::SoftTimerService::Instance().CreateTask(config);
```

字段含义：

- `callback`：定时器触发后的回调函数
- `context`：传给回调的上下文指针
- `interval_ms`：周期时间
- `start_delay_ms`：首次触发延时
- `priority`：优先级，数值越小优先级越高
- `auto_reload`：`true` 为周期任务，`false` 为一次性任务

## 8. 周期任务示例

### 发布任务

```cpp
CreateTimerTask(g_publish_task, PublishCounterMessage, nullptr, 1000U, 1000U, 5U, true);
```

含义：

- 1 秒后首次触发
- 之后每 1 秒触发一次
- 回调函数里发布一条消息

### 逐条消费任务

```cpp
CreateTimerTask(g_stream_consume_task, ConsumeStreamMessages, nullptr, 200U, 200U, 20U, true);
```

含义：

- 每 200ms 检查一次订阅者队列
- 如果有新消息，就用 `Copy()` 一条一条取出

### 最新值观察任务

```cpp
CreateTimerTask(g_latest_watch_task, WatchLatestMessage, nullptr, 1500U, 1500U, 30U, true);
```

含义：

- 每 1500ms 取一次最新值
- 使用 `CopyLatest()`，更适合状态类主题

## 9. 一次性任务示例

示例代码：

```cpp
CreateTimerTask(g_one_shot_task, OneShotHello, nullptr, 1000U, 3000U, 10U, false);
```

这里要注意：

- `auto_reload = false`
- 表示这个任务只会执行一次
- 本例会在启动 3 秒后输出一条提示日志

## 10. 删除任务示例

示例代码：

```cpp
const bool deleted = iFly::SoftTimerService::Instance().DeleteTask(g_latest_watch_task);
```

本例中：

- `g_delete_demo_task` 会在启动 12 秒后执行一次
- 它会删除 `g_latest_watch_task`
- 删除成功后，`latest` 观察日志就不会再继续输出

## 11. `Copy()` 与 `CopyLatest()` 的区别

### `Copy()`

适合场景：

- 每一条消息都想处理
- 不希望跳过中间值

示例：

```cpp
while (g_stream_sub.Copy(message, &generation)) {
  // 逐条处理
}
```

### `CopyLatest()`

适合场景：

- 只关心当前最新状态
- 中间值可以丢弃

示例：

```cpp
if (g_latest_sub.CopyLatest(latest, &generation)) {
  // 只处理最新一条
}
```

## 12. 示例运行后会看到什么

如果 USB CDC 已连接，在虚拟串口工具中会看到类似日志：

```text
================ msg + soft_timer 示例 ================
1. PublishCounterMessage(): 每 1 秒发布一条消息
2. ConsumeStreamMessages(): 演示 Updated() + Copy()
3. WatchLatestMessage(): 演示 CopyLatest()
4. OneShotHello(): 演示一次性定时器
5. DeleteTaskExample(): 演示 DeleteTask()
=======================================================
[publish] ok, generation=1, counter=1, tick=1000
[copy] generation=1, counter=1, tick=1000, pending=0, lost=0
[latest] generation=1, counter=1, publish_ok=1, pending=0, lost=0
[one-shot] 这是一次性定时器，只会执行一次，当前 tick=3000
[delete-task] latest 观察任务删除结果=success
```

## 13. 推荐使用顺序

如果你后面要在自己的业务里接入，建议按这个顺序写：

1. 定义消息结构体
2. 定义 Topic、Publication、Subscription
3. 在初始化阶段先执行 `Subscribe()`
4. 创建一个或多个定时器任务
5. 在回调里使用 `Publish()`、`Copy()` 或 `CopyLatest()`
6. 在主循环里持续调用 `Dispatch()`

## 14. 相关文件

- 示例入口：`E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\app_main.cpp`
- 教程文档：`E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\MSG_SOFT_TIMER_使用教程.md`
- 软件定时器头文件：`E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\lib\soft_timer\soft_timer.hpp`
- 消息模块头文件：`E:\project\DragonFly_Project\myfly\fly_project\iFly_Ctrl\iFly\lib\msg\msg.hpp`

## 15. 一句话总结

最核心的两句可以记住：

- 定时器要靠 `CreateTask()` 创建，靠 `Dispatch()` 执行
- Topic 要先 `Subscribe()`，发布时用 `Publish()`，消费时用 `Copy()` 或 `CopyLatest()`
