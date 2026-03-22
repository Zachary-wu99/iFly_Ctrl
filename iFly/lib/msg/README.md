# msg 模块说明

## 1. 模块目标

`msg.hpp` 提供了一个适合当前 `iFly_Ctrl` 裸机工程的、轻量级的 PX4 uORB 风格发布订阅模型。

它想解决的问题不是“传输字节流”，而是“在模块之间传递结构化消息”。

典型使用场景：

- IMU 采样任务发布 `ImuSample`
- 姿态解算模块订阅 `ImuSample`
- 遥控接收模块发布 `RcInput`
- 控制器模块订阅 `RcInput`
- 状态估计模块发布 `AttitudeEstimate`
- 下游控制模块订阅 `AttitudeEstimate`

和直接用 UART/USB 字节队列相比，这种做法更像“消息总线”：

- 有明确的 Topic 概念
- 消息是结构体，不是裸字节流
- 发布者和订阅者可以解耦
- 每个订阅者都可以独立消费自己的消息副本

---

## 2. 当前实现的定位

这份实现是“uORB 风格”，不是完整复刻 PX4 uORB。

它保留了以下核心思想：

- 按 Topic 发布与订阅
- 使用 generation 表示消息更新代数
- 订阅者可以判断 `Updated()`
- 订阅者可以拿“下一条消息”或“最新一条消息”

但为了适配当前工程，也做了工程化裁剪：

- 不做全局设备节点和字符串注册表
- 不依赖动态内存
- 不依赖 RTOS
- 不依赖文件系统或 ioctl 风格接口
- 直接用 C++ 模板和静态对象表达 Topic

所以更准确地说，它是：

`uORB 的接口思路 + 适配当前 STM32 裸机工程的静态实现`

---

## 3. 目录与文件

- `msg.hpp`
  核心实现，头文件模板库
- `README.md`
  使用说明、教程与设计摘要
- `IMPLEMENTATION.md`
  更详细的实现过程、设计动机和关键细节说明

---

## 4. 核心类说明

## 4.1 `MsgTopic<MessageType, kMaxSubscribers, kQueueDepth>`

这是 Topic 本体。

模板参数含义：

- `MessageType`
  消息结构体类型
- `kMaxSubscribers`
  该 Topic 最多允许多少个订阅者
- `kQueueDepth`
  每个订阅者本地缓存多少条消息

例如：

```cpp
struct ImuSample final {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
};

static iFly::MsgTopic<ImuSample, 4U, 8U> g_imu_topic("sensor_imu");
```

这表示：

- Topic 名字是 `sensor_imu`
- 最多 4 个订阅者
- 每个订阅者最多缓存 8 条消息

---

## 4.2 `Publication`

发布端包装，用来让业务代码写起来更像 uORB。

```cpp
static iFly::MsgTopic<ImuSample, 4U, 8U>::Publication g_imu_pub(g_imu_topic);
```

发布消息：

```cpp
ImuSample sample {};
sample.ax = 1.0f;
sample.ay = 2.0f;
sample.az = 3.0f;

(void)g_imu_pub.Publish(sample);
```

---

## 4.3 `Subscription`

订阅端对象。

```cpp
static iFly::MsgTopic<ImuSample, 4U, 8U>::Subscription g_imu_sub;
```

订阅 Topic：

```cpp
(void)g_imu_sub.Subscribe(g_imu_topic);
```

读取消息：

```cpp
ImuSample sample {};
if (g_imu_sub.Copy(sample)) {
  // 逐条处理一条消息
}
```

只拿最新值：

```cpp
ImuSample latest {};
if (g_imu_sub.CopyLatest(latest)) {
  // 只关心最新状态
}
```

---

## 5. 这个实现为什么不是“一个 Topic 一条共享队列”

如果一个 Topic 只有一条共享队列，多个订阅者会有两个严重问题：

1. 多个订阅者会互相抢消息
2. 一个订阅者取走消息后，别的订阅者就看不到了

这和 uORB 期望的行为不一致。

uORB 风格更接近：

- 发布者发布一条消息
- 每个订阅者都能看到这条消息
- 每个订阅者按自己的节奏消费

所以这里采用的是：

- Topic 维护订阅者表
- 每个订阅者拥有自己的无锁队列
- 发布时 fan-out 到每个订阅者的本地队列

这样更符合消息总线的语义。

---

## 6. 为什么底层仍然使用无锁队列

每个订阅者的本地缓存底层是：

```cpp
StaticLockFreeQueue<...>
```

这样做的目的：

- 发布时向订阅者推消息开销很小
- 订阅者读消息时不需要全局锁
- 适合 ISR / 主循环之间传递消息
- 不需要动态内存

不过要明确一点：

- “每个订阅者自己的队列”这一层是基于无锁队列的
- “Topic 统一分发给多个订阅者”这一层做了短临界区串行化

也就是说，整个模块不是纯粹的全局 lock-free 总线，而是：

`短串行化的 Topic 分发 + 无锁订阅者本地队列`

这是为了兼顾：

- 工程可控性
- 当前底层队列的并发约束
- 裸机环境下的实现复杂度

---

## 7. generation 的意义

每发布一次消息，Topic 就会把 generation 加一。

然后每个订阅者内部会记录两件事：

- 最近一次发布到自己队列的 generation
- 最近一次自己成功消费的 generation

于是就可以实现：

```cpp
if (subscription.Updated()) {
  // 有尚未消费的新消息
}
```

这和 PX4 uORB 的“是否更新”语义很接近。

---

## 8. 队列满时为什么丢最旧消息

当某个订阅者处理太慢时，它的本地队列可能会满。

当前策略是：

1. 优先尝试直接写入
2. 如果写不进去，就先丢掉一条最旧的消息
3. 再把最新消息写进去

这种策略适合以下主题：

- IMU
- 姿态
- 角速度
- 遥控输入
- 传感器状态

因为这类数据通常“最新值比最旧值更重要”。

如果以后你有某些主题要求“绝不能丢历史消息”，那就需要单独做另一种策略。

---

## 9. 对消息类型的要求

`MessageType` 需要满足：

- 标准布局
- 可平凡复制
- 可默认构造

也就是更偏向 POD 风格。

推荐这样写：

```cpp
struct RcInput final {
  uint32_t timestamp_ms = 0U;
  int16_t roll = 0;
  int16_t pitch = 0;
  int16_t yaw = 0;
  int16_t throttle = 0;
};
```

不推荐把下面这些放进消息结构体：

- `std::string`
- `std::vector`
- 自定义析构函数
- 复杂继承体系
- 动态内存所有权对象

原因很直接：这个模块的实现假设消息能安全地按字节整体复制。

---

## 10. 最小使用教程

## 10.1 定义消息类型

```cpp
struct ImuSample final {
  uint32_t timestamp_ms = 0U;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
};
```

## 10.2 定义 Topic、发布者、订阅者

```cpp
static iFly::MsgTopic<ImuSample, 4U, 8U> g_imu_topic("sensor_imu");
static iFly::MsgTopic<ImuSample, 4U, 8U>::Publication g_imu_pub(g_imu_topic);
static iFly::MsgTopic<ImuSample, 4U, 8U>::Subscription g_imu_sub;
```

## 10.3 初始化时建立订阅关系

```cpp
void InitImuBus()
{
  (void)g_imu_sub.Subscribe(g_imu_topic);
}
```

## 10.4 发布消息

```cpp
void PublishImuSample()
{
  ImuSample sample {};
  sample.timestamp_ms = 1234U;
  sample.ax = 0.1f;
  sample.ay = 0.2f;
  sample.az = 9.8f;

  (void)g_imu_pub.Publish(sample);
}
```

## 10.5 顺序消费每一条消息

```cpp
void ConsumeImuSequentially()
{
  ImuSample sample {};
  while (g_imu_sub.Copy(sample)) {
    // 每条都处理
  }
}
```

## 10.6 只读取最新消息

```cpp
void ConsumeOnlyLatestImu()
{
  ImuSample latest {};
  if (g_imu_sub.CopyLatest(latest)) {
    // 只处理当前最新值
  }
}
```

## 10.7 判断是否有更新

```cpp
if (g_imu_sub.Updated()) {
  ImuSample latest {};
  (void)g_imu_sub.CopyLatest(latest);
}
```

---

## 11. 获取调试信息

你可以通过下面这些接口观察运行状态：

- `topic.Generation()`
- `topic.SubscriberCount()`
- `topic.BusyPublishCount()`
- `subscription.PendingCount()`
- `subscription.LostCount()`
- `subscription.LastPublishedGeneration()`
- `subscription.LastConsumedGeneration()`

典型判断思路：

- `BusyPublishCount()` 持续增长
  说明同一个 Topic 可能有多个上下文同时高频发布
- `LostCount()` 持续增长
  说明某个订阅者消费太慢，队列深度不够
- `PendingCount()` 长期很大
  说明订阅端处理速度落后于发布速度

---

## 12. 推荐的使用模式

推荐：

- ISR 中只做采样/组包后快速发布
- 主循环或任务中订阅并处理
- 对“状态类主题”多用 `CopyLatest()`
- 对“事件类主题”多用 `Copy()`

不推荐：

- 在消息结构体里塞复杂 C++ 对象
- 同一个 Topic 里让多个发布上下文长期并发抢发布
- 把队列深度设得特别大，导致 RAM 占用不可控

---

## 13. 适合哪些主题

比较适合：

- IMU
- 气压计
- 姿态解算结果
- 遥控输入
- 控制器输出
- 电池状态

相对不太适合：

- 超大块二进制数据
- 长字符串消息
- 必须完整保留全部历史记录的日志流

---

## 14. 当前实现边界

当前版本已经有：

- Topic
- Publication
- Subscription
- Updated
- generation
- 丢包统计
- 每订阅者独立队列

当前还没有：

- 全局 Topic 注册中心
- 按字符串查找 Topic
- 多实例 Topic（比如 `sensor_accel0/1/2` 的统一注册）
- 阻塞等待新消息
- 发布优先级与调度集成

如果后面你需要，我可以继续往下补：

- `TopicRegistry`
- 多实例 Topic
- `poll` / `wait` 风格接口
- 和 `soft_timer` / `app_main` 联动的完整示例

---

## 15. 一句话总结

这份模块不是为了完整复刻 PX4 uORB，而是为了在当前 STM32 裸机工程里，用尽量小的实现成本，得到：

- 清晰的消息 Topic 抽象
- 接近 uORB 的发布订阅接口
- 基于无锁队列的每订阅者缓存
- 不依赖动态内存的可控实现
