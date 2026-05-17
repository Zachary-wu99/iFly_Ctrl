# observer 模块教学文档

本文讲解 `iFly/app/observer` 目录下的观察者通道实现。当前目录只有一个核心文件：

- `observer_channel.hpp`：单头文件实现，包含 `ObserverChannel`、`CallbackConsumer`、`ObserverHub` 以及回调参数类型推导工具。

读完本文后，你应该能做到三件事：

1. 会在业务代码里创建通道、发布消息、订阅消息、读取消息。
2. 能理解这个模块内部如何用序号、环形缓冲区和 `stamp` 保证读到稳定快照。
3. 知道这个模块适合什么场景、不适合什么场景，以及遇到问题时该从哪里排查。

## 1. 这个模块解决什么问题

这个模块实现的是一种“单发布者、多消费者”的观察者通道。

传统观察者模式通常是这样：

1. 观察者注册回调。
2. 被观察者状态变化时，立刻逐个调用回调。
3. 回调在发布动作里同步执行。

本模块的做法不同：

1. 发布者只负责把最新消息写进通道。
2. 每个消费者自己保存读取进度。
3. 消费者在自己的任务循环里主动调用 `TryRead`、`TryReadLatest`、`PollOnce` 或 `Drain` 读取数据。
4. `Publish` 本身不会主动调用消费者回调。

所以它更像“无锁快照通道”，而不是“发布时同步回调所有观察者”的经典实现。

适合的场景：

- 一个传感器任务持续发布姿态、位置、电池、电机状态等消息。
- 多个模块需要读取同一类消息，但每个模块的读取频率不同。
- 只需要保留最近几条历史，不需要无限队列。
- 希望发布操作轻量，不希望发布者被消费者阻塞。

不适合的场景：

- 多个发布者同时写同一个通道。
- 必须保证每条历史消息永不丢失。
- 需要阻塞等待、条件变量通知或操作系统消息队列语义。
- 需要通道替你自动调度线程或自动执行回调。

## 2. 核心概念

### 2.1 消息类型 `T`

`ObserverChannel<T>` 中的 `T` 就是通道承载的消息类型。

示例：

```cpp
struct AttitudeMsg {
  float roll = 0.0F;
  float pitch = 0.0F;
  float yaw = 0.0F;
};

iFly::ObserverChannel<AttitudeMsg> attitudeChannel;
```

这表示 `attitudeChannel` 只传输 `AttitudeMsg`。

消息类型至少要满足这些实际要求：

- 能默认构造，因为通道槽位里有 `T value {}`。
- 能被赋值，因为发布时会执行 `slot.value = message`。
- 能被复制读出，因为读取时会执行 `out = slot.value`。

文件里的静态断言允许“可拷贝赋值或可移动赋值”，但读取路径实际从 `const T` 复制到 `out`，所以实践中建议让消息类型保持默认构造、可拷贝赋值。

### 2.2 发布者

发布者调用：

```cpp
channel.Publish(message);
```

或：

```cpp
channel.Emplace(args...);
```

`Publish` 写入一条完整消息快照，并把全局发布序号加一。

注意：当前实现面向“单发布者”。`Publish` 内部用 `publishedSequence_.load() + 1` 计算下一个序号，没有用 CAS 竞争多个发布者，所以不要让多个线程同时向同一个 `ObserverChannel` 调用 `Publish`。

### 2.3 消费者

消费者是 `ObserverChannel<T>::Consumer`。

你不能直接构造一个有效消费者，应该通过通道订阅得到：

```cpp
auto consumer = channel.SubscribeLatest();
```

或者：

```cpp
auto consumer = channel.SubscribeFromNext();
```

每个消费者内部有自己的 `nextSequence_`，表示“下一次我想读哪个序号”。

多个消费者互不影响：

- 消费者 A 读到第 10 条，不会改变消费者 B 的读取位置。
- 消费者 B 可以落后，也可以直接追最新。

消费者是移动对象：

- 不能拷贝。
- 可以移动。
- 析构时会自动释放注册槽位。

### 2.4 历史深度 `kHistoryDepth`

模板参数 `kHistoryDepth` 表示通道最多保留最近多少条消息。

默认是 4：

```cpp
iFly::ObserverChannel<AttitudeMsg> channel;
```

等价于：

```cpp
iFly::ObserverChannel<AttitudeMsg, 4U, 4U> channel;
```

如果你想保留最近 8 条：

```cpp
iFly::ObserverChannel<AttitudeMsg, 8U> channel;
```

通道内部用环形数组保存历史。假设历史深度是 4：

| 发布序号 | 槽位计算 `(sequence - 1) % 4` | 槽位 |
| --- | --- | --- |
| 1 | 0 | slots_[0] |
| 2 | 1 | slots_[1] |
| 3 | 2 | slots_[2] |
| 4 | 3 | slots_[3] |
| 5 | 0 | slots_[0]，覆盖序号 1 |
| 6 | 1 | slots_[1]，覆盖序号 2 |

历史深度越大，慢消费者越不容易丢历史，但占用内存也越多。

### 2.5 最大消费者数 `kMaxConsumers`

模板参数 `kMaxConsumers` 表示最多允许多少个消费者同时附着在通道上。

默认是 4：

```cpp
iFly::ObserverChannel<AttitudeMsg, 8U, 4U> channel;
```

如果同时订阅超过这个数量，新的订阅会返回一个未附着的消费者。你应该检查：

```cpp
auto consumer = channel.SubscribeLatest();
if (!consumer.IsAttached()) {
  // 订阅失败：消费者槽位满了
}
```

消费者对象析构后，槽位会释放。

## 3. 最小使用步骤

下面从零开始写一个最小例子。

### 第 1 步：包含头文件

```cpp
#include "iFly/app/observer/observer_channel.hpp"
```

如果当前源文件的 include 路径已经指向 `iFly/app`，也可能写成：

```cpp
#include "observer/observer_channel.hpp"
```

具体取决于工程的 include path 配置。

### 第 2 步：定义消息类型

```cpp
struct BatteryMsg {
  float voltage = 0.0F;
  float current = 0.0F;
  uint8_t percent = 0U;
};
```

这里给每个字段默认值，是为了满足默认构造要求。

### 第 3 步：创建通道

```cpp
iFly::ObserverChannel<BatteryMsg, 4U, 2U> batteryChannel;
```

含义：

- `BatteryMsg`：通道传输电池消息。
- `4U`：最多保留最近 4 条历史。
- `2U`：最多同时 2 个消费者。

### 第 4 步：创建消费者

如果消费者想先读到“当前最新快照”，使用：

```cpp
auto consumer = batteryChannel.SubscribeLatest();
```

如果消费者只关心“订阅之后的新消息”，使用：

```cpp
auto consumer = batteryChannel.SubscribeFromNext();
```

订阅之后建议检查：

```cpp
if (!consumer.IsAttached()) {
  // 没有拿到消费者槽位，后续 TryRead 只会返回 kNoData
}
```

### 第 5 步：发布消息

方式一：先构造对象，再发布。

```cpp
BatteryMsg msg;
msg.voltage = 16.2F;
msg.current = 3.4F;
msg.percent = 82U;

batteryChannel.Publish(msg);
```

方式二：直接传临时对象。

```cpp
batteryChannel.Publish(BatteryMsg {16.2F, 3.4F, 82U});
```

方式三：用 `Emplace`。

```cpp
batteryChannel.Emplace(16.2F, 3.4F, 82U);
```

注意：这个 `Emplace` 内部会先构造一个本地临时 `T value(...)`，再调用 `Publish(std::move(value))`。它不是直接在环形槽位里原地构造。

### 第 6 步：读取下一条消息

```cpp
BatteryMsg out;
uint32_t sequence = 0U;
const iFly::ConsumeStatus status = consumer.TryRead(out, &sequence);

if (status == iFly::ConsumeStatus::kOk) {
  // out 是读到的消息，sequence 是消息序号
} else if (status == iFly::ConsumeStatus::kNoData) {
  // 当前没有新数据
} else if (status == iFly::ConsumeStatus::kOverflowed) {
  // 消费者落后太多，旧历史已被覆盖；out 是当前还能读到的最早可用消息
}
```

`TryRead` 的语义是“按序读下一条”。如果你的消费者没有落后，它会一条一条读。

### 第 7 步：读取最新消息

```cpp
BatteryMsg latest;
const iFly::ConsumeStatus status = consumer.TryReadLatest(latest);
```

`TryReadLatest` 的语义是“直接读当前最新”。它适合状态类消息，比如姿态、电压、遥控输入当前值。

当前实现中，`TryReadLatest` 会主动跳到最新发布序号。中间被跳过的消息不会逐条返回，也不会像 `TryRead` 那样主要依赖 `kOverflowed` 提醒你历史被覆盖。所以：

- 想处理每条事件，优先用 `TryRead`。
- 只关心最新状态，优先用 `TryReadLatest`。

### 第 8 步：周期性消费

在任务循环里，常见写法如下：

```cpp
void BatteryConsumerTask() {
  static auto consumer = batteryChannel.SubscribeLatest();

  BatteryMsg msg;
  uint32_t sequence = 0U;
  const auto status = consumer.TryRead(msg, &sequence);

  if (status == iFly::ConsumeStatus::kNoData) {
    return;
  }

  if (status == iFly::ConsumeStatus::kOverflowed) {
    // 可以记录一次丢历史告警
  }

  // 使用 msg 更新业务状态
}
```

如果任务每次运行都想把积压的数据全部处理完，可以这样：

```cpp
void DrainBatteryMessages() {
  static auto consumer = batteryChannel.SubscribeFromNext();

  BatteryMsg msg;
  while (consumer.TryRead(msg) != iFly::ConsumeStatus::kNoData) {
    // 逐条处理 msg
  }
}
```

## 4. `ConsumeStatus` 怎么理解

`ConsumeStatus` 有三个值。

| 状态 | 含义 | 常见处理 |
| --- | --- | --- |
| `kNoData` | 当前没有可读数据 | 直接返回，等待下一轮 |
| `kOk` | 成功读到一条消息 | 正常处理 |
| `kOverflowed` | 消费者想读的旧序号已经被覆盖 | 处理当前返回的数据，并记录丢历史 |

重点是区分 `kNoData` 和 `kOverflowed`：

- `kNoData` 表示不是错误，只是现在没有新消息。
- `kOverflowed` 表示消费者太慢，通道历史深度不够，旧消息已经被覆盖。

举例：历史深度是 3。

```cpp
iFly::ObserverChannel<int, 3U, 1U> channel;
auto consumer = channel.SubscribeFromNext();

channel.Publish(10); // sequence 1
channel.Publish(20); // sequence 2
channel.Publish(30); // sequence 3
channel.Publish(40); // sequence 4，历史保留 2、3、4
channel.Publish(50); // sequence 5，历史保留 3、4、5
```

消费者从订阅后一直没读，它的 `nextSequence_` 还是 1。

这时调用：

```cpp
int value = 0;
uint32_t sequence = 0U;
auto status = consumer.TryRead(value, &sequence);
```

结果是：

- `status == kOverflowed`
- `sequence == 3`
- `value == 30`

原因是序号 1、2 已经被覆盖，当前最早还能读的是序号 3。

## 5. `SubscribeLatest` 和 `SubscribeFromNext` 的区别

这两个函数只影响“订阅刚创建时，消费者从哪个序号开始读”。

### 5.1 `SubscribeLatest`

```cpp
auto consumer = channel.SubscribeLatest();
```

规则：

- 如果通道还没有任何发布，消费者从序号 1 开始等。
- 如果通道已经发布过消息，消费者从当前最新序号开始读。

适合：

- 状态类消息。
- 新消费者启动后，先拿一次当前状态。
- 例如姿态、电池、电机输出、当前飞行模式。

### 5.2 `SubscribeFromNext`

```cpp
auto consumer = channel.SubscribeFromNext();
```

规则：

- 消费者从“当前最新序号 + 1”开始等。
- 订阅之前已经存在的历史不会读。

适合：

- 只关心之后发生的新事件。
- 不想处理启动前的旧状态。
- 例如某些命令、触发事件、边沿变化。

### 5.3 一张表看懂

假设通道已经发布到序号 10。

| 操作 | 消费者初始 `nextSequence_` | 第一次 `TryRead` 会读到 |
| --- | --- | --- |
| `SubscribeLatest()` | 10 | 序号 10 |
| `SubscribeFromNext()` | 11 | 暂时 `kNoData`，等序号 11 |

如果通道还没有发布过任何消息：

| 操作 | 消费者初始 `nextSequence_` |
| --- | --- |
| `SubscribeLatest()` | 1 |
| `SubscribeFromNext()` | 1 |

## 6. 重置读取位置

消费者提供两个重置函数。

### 6.1 `ResetToLatest`

```cpp
consumer.ResetToLatest();
```

效果：

- 如果已经有消息，把下一次读取位置设为当前最新序号。
- 如果还没有消息，把下一次读取位置设为 1。

下一次 `TryRead` 会尝试读取当前最新消息。

适合：

- 消费者已经落后很多，不想补历史，只想从当前状态继续。

### 6.2 `ResetToNextPublication`

```cpp
consumer.ResetToNextPublication();
```

效果：

- 把下一次读取位置设为“当前最新序号 + 1”。
- 当前已有消息全部跳过。

适合：

- 进入某个新状态后，只想等之后的新消息。

## 7. `ObserverChannel` 内部实现

这一节解释 `ObserverChannel` 为什么能工作。

### 7.1 内部数据结构

核心成员有三个：

```cpp
std::array<Slot, kHistoryDepth> slots_ {};
std::array<Registration, kMaxConsumers> registrations_ {};
std::atomic<uint32_t> publishedSequence_ {0U};
```

含义：

- `slots_`：环形历史缓冲区，保存最近 `kHistoryDepth` 条消息。
- `registrations_`：消费者注册表，记录哪个消费者槽位正在使用。
- `publishedSequence_`：最近一次成功发布的消息序号，初始是 0。

`Slot` 里有：

```cpp
std::atomic<uint32_t> stamp {0U};
T value {};
```

含义：

- `value` 保存消息。
- `stamp` 标记这个槽位当前保存的是哪个序号，以及是否正在写入。

`Registration` 里有：

```cpp
std::atomic<bool> active {false};
```

含义：

- `false`：这个消费者槽位空闲。
- `true`：这个消费者槽位已被某个 `Consumer` 占用。

### 7.2 发布序号

发布序号从 1 开始。

```cpp
const uint32_t nextSequence =
    publishedSequence_.load(std::memory_order_relaxed) + 1U;
```

每次 `Publish` 都会生成一个新序号。

`publishedSequence_ == 0` 表示还没有任何消息发布。

### 7.3 序号如何映射到槽位

映射函数是：

```cpp
static constexpr uint32_t SlotIndex(uint32_t sequence) {
  return (sequence - 1U) % kHistoryDepth;
}
```

假设 `kHistoryDepth == 4`：

| sequence | SlotIndex |
| --- | --- |
| 1 | 0 |
| 2 | 1 |
| 3 | 2 |
| 4 | 3 |
| 5 | 0 |
| 6 | 1 |

这就是环形缓冲区。

### 7.4 最早还保留的序号

函数：

```cpp
static constexpr uint32_t OldestRetainedSequence(uint32_t published) {
  if (published >= kHistoryDepth) {
    return published - kHistoryDepth + 1U;
  }

  return 1U;
}
```

含义：

- 如果发布数量还没超过历史深度，最早可读序号是 1。
- 如果发布数量超过历史深度，最早可读序号是 `published - kHistoryDepth + 1`。

例子：历史深度 4，最新发布序号 10。

```text
oldest = 10 - 4 + 1 = 7
```

所以当前还保留序号 7、8、9、10。

### 7.5 `stamp` 的作用

发布者写一个槽位时，不是只写 `value`。它会按顺序做四件事：

```cpp
slot.stamp.store(stableStamp | 0x01U, std::memory_order_release);
slot.value = std::forward<Message>(message);
slot.stamp.store(stableStamp, std::memory_order_release);
publishedSequence_.store(nextSequence, std::memory_order_release);
```

逐步解释：

1. 计算 `stableStamp = sequence << 1U`。
2. 先把 `stamp` 写成奇数，表示这个槽位正在写。
3. 把消息写进 `slot.value`。
4. 再把 `stamp` 写成偶数，表示这个槽位已稳定。
5. 最后更新 `publishedSequence_`，让消费者知道有新消息。

为什么奇偶能表达状态？

- `stableStamp` 是 `sequence << 1`，最低位一定是 0，所以是偶数。
- `stableStamp | 0x01` 把最低位置 1，所以是奇数。
- 奇数表示写入中。
- 偶数表示稳定可读。

### 7.6 读者如何确认快照稳定

读取时调用 `TryCopySequence`：

```cpp
const uint32_t before = slot.stamp.load(std::memory_order_acquire);
if (before != expectedStamp) {
  return false;
}

out = slot.value;

const uint32_t after = slot.stamp.load(std::memory_order_acquire);
if (after == expectedStamp) {
  return true;
}
```

它的逻辑是：

1. 先读一次 `stamp`。
2. 如果 `stamp` 不是期望序号的稳定值，说明槽位不是我要的消息，或者正在写，返回失败。
3. 复制 `value` 到 `out`。
4. 再读一次 `stamp`。
5. 如果前后都等于期望值，说明复制期间槽位没有被覆盖，返回成功。
6. 如果复制期间槽位被发布者覆盖，第二次 `stamp` 会不匹配，返回失败。

`TryCopySequence` 最多内部重试 3 次。如果仍然不稳定，它返回 `false`，外层 `TryReadImpl` 会重新计算当前最新序号和最早保留序号后再试。

这是一种 seqlock 风格的快照读取思路。

### 7.7 `TryReadImpl` 的完整流程

`TryRead` 和 `TryReadLatest` 最终都会进入：

```cpp
ConsumeStatus TryReadImpl(uint32_t &nextSequence,
                          bool latestOnly,
                          T &out,
                          uint32_t *sequence);
```

参数含义：

- `nextSequence`：当前消费者下一次想读的序号，函数成功后会更新它。
- `latestOnly`：`false` 表示按序读；`true` 表示直接读最新。
- `out`：输出消息。
- `sequence`：输出实际读到的序号，可以是 `nullptr`。

流程如下：

1. 读取 `publishedSequence_`，得到当前最新发布序号。
2. 如果还没有发布，返回 `kNoData`。
3. 如果消费者想读的序号大于最新发布序号，返回 `kNoData`。
4. 根据最新发布序号计算当前最早还保留的序号 `oldest`。
5. 如果是按序读取，目标序号是 `nextSequence`。
6. 如果是读取最新，目标序号是 `published`。
7. 如果按序读取时目标序号比 `oldest` 还旧，说明历史被覆盖，把目标序号改成 `oldest`，状态设为 `kOverflowed`。
8. 调用 `TryCopySequence` 复制目标序号对应的消息。
9. 如果复制失败，说明槽位变化了，回到第 1 步重新判断。
10. 如果复制成功，把消费者的 `nextSequence` 更新为 `targetSequence + 1`。
11. 如果调用者传入了 `sequence` 指针，写入实际读到的序号。
12. 返回 `kOk` 或 `kOverflowed`。

## 8. `CallbackConsumer` 怎么用

`CallbackConsumer` 是一个薄封装：它把 `Consumer` 和回调函数放在一起。

它不会自动创建线程，也不会自动被 `Publish` 调用。你仍然需要主动轮询。

### 8.1 手动创建 `CallbackConsumer`

```cpp
iFly::ObserverChannel<BatteryMsg, 4U, 2U> channel;

auto callbackConsumer = iFly::MakeCallbackConsumer<
    iFly::ObserverChannel<BatteryMsg, 4U, 2U>>(
    channel.SubscribeLatest(),
    [](const BatteryMsg &msg) {
      // 处理电池消息
    });
```

之后调用：

```cpp
callbackConsumer.PollOnce();
```

`PollOnce` 做的事情：

1. 创建一个临时 `value_type value {}`。
2. 调用底层消费者的 `TryRead(value)`。
3. 如果返回 `kNoData`，直接返回，不调用回调。
4. 如果读到数据，调用 `callback_(value)`。
5. 返回读取状态。

### 8.2 只处理最新值

```cpp
callbackConsumer.PollLatest();
```

它内部调用 `TryReadLatest`，所以适合状态类消息。

### 8.3 一次处理完积压消息

```cpp
uint32_t handled = callbackConsumer.Drain();
```

`Drain` 会循环调用 `PollOnce`，直到返回 `kNoData`。

适合：

- 消费者任务频率低于发布者。
- 但你仍希望尽量按序处理积压消息。

不适合：

- 单次任务运行时间必须非常短。
- 积压消息太多会影响实时性。

## 9. `ObserverHub` 怎么用

`ObserverHub` 用来统一管理多种消息类型对应的多个通道。

如果只有一种消息，用 `ObserverChannel` 就够了。

如果系统里有很多消息类型，比如姿态、电池、位置，可以用 `ObserverHub` 把它们放在一起。

### 9.1 定义多个消息类型

```cpp
struct AttitudeMsg {
  float roll = 0.0F;
  float pitch = 0.0F;
  float yaw = 0.0F;
};

struct PositionMsg {
  double lat = 0.0;
  double lon = 0.0;
  float alt = 0.0F;
};

struct BatteryMsg {
  float voltage = 0.0F;
  float current = 0.0F;
  uint8_t percent = 0U;
};
```

### 9.2 定义 Hub 类型

```cpp
using AppObserverHub = iFly::ObserverHub<
    iFly::ObserverChannel<AttitudeMsg, 8U, 4U>,
    iFly::ObserverChannel<PositionMsg, 4U, 2U>,
    iFly::ObserverChannel<BatteryMsg, 4U, 2U>>;
```

每个 `ObserverChannel` 的 `value_type` 必须唯一。

不能这样写：

```cpp
using BadHub = iFly::ObserverHub<
    iFly::ObserverChannel<BatteryMsg, 4U, 2U>,
    iFly::ObserverChannel<BatteryMsg, 8U, 2U>>;
```

因为 `ObserverHub` 根据消息类型查找通道。同一种消息类型出现两次，它无法判断应该发布到哪个通道，编译时会报错。

### 9.3 创建 Hub 实例

```cpp
AppObserverHub hub;
```

### 9.4 按消息类型发布

```cpp
hub.Publish(AttitudeMsg {1.0F, 2.0F, 3.0F});
hub.Publish(BatteryMsg {16.2F, 3.4F, 82U});
```

`ObserverHub::Publish` 会根据传入对象的类型自动找到对应通道。

内部逻辑相当于：

```cpp
Channel<AttitudeMsg>().Publish(...);
```

### 9.5 按消息类型原地构造并发布

```cpp
hub.Emplace<BatteryMsg>(16.2F, 3.4F, 82U);
```

这里必须显式写 `<BatteryMsg>`，因为 `Emplace` 只拿到构造参数，不能总是可靠推导出目标消息类型。

### 9.6 按消息类型订阅

```cpp
auto batteryConsumer = hub.SubscribeLatest<BatteryMsg>();
auto attitudeConsumer = hub.SubscribeFromNext<AttitudeMsg>();
```

读取方式和直接使用 `ObserverChannel` 一样：

```cpp
BatteryMsg battery;
if (batteryConsumer.TryReadLatest(battery) == iFly::ConsumeStatus::kOk) {
  // 使用 battery
}
```

### 9.7 直接访问某个通道

```cpp
auto &batteryChannel = hub.Channel<BatteryMsg>();
```

这可以用于：

- 查询 `PublishedSequence()`。
- 调用通道级 API。
- 在少数场景下把某个通道传给已有函数。

## 10. `BindLatest` 和 `BindFromNext`

`ObserverHub` 还支持直接把回调绑定到对应消息类型。

### 10.1 绑定最新值回调

```cpp
auto batteryReader = hub.BindLatest([](const BatteryMsg &msg) {
  // 处理电池最新状态
});
```

`BindLatest` 会做三件事：

1. 从 lambda 参数里推导出消息类型 `BatteryMsg`。
2. 调用 `SubscribeLatest<BatteryMsg>()` 创建消费者。
3. 返回一个 `CallbackConsumer`。

之后你仍然要主动轮询：

```cpp
batteryReader.PollLatest();
```

或者：

```cpp
batteryReader.PollOnce();
```

### 10.2 绑定下一次发布回调

```cpp
auto attitudeReader = hub.BindFromNext([](const AttitudeMsg &msg) {
  // 处理订阅之后的新姿态消息
});
```

之后主动调用：

```cpp
attitudeReader.Drain();
```

### 10.3 回调参数推导规则

模块通过 `FunctionTraits` 和 `CallbackArgument` 推导回调参数。

支持常见形式：

```cpp
void OnBattery(BatteryMsg msg);
void OnBatteryConstRef(const BatteryMsg &msg);

auto reader1 = hub.BindLatest(OnBattery);
auto reader2 = hub.BindLatest(OnBatteryConstRef);
auto reader3 = hub.BindLatest([](const BatteryMsg &msg) {
  // ...
});
```

它会把参数类型做标准化：

- `BatteryMsg` -> `BatteryMsg`
- `const BatteryMsg &` -> `BatteryMsg`
- `volatile BatteryMsg &` -> `BatteryMsg`

不建议用于这些形式：

- 没有参数的回调。
- 多个参数的回调。
- 泛型 lambda，例如 `[](const auto &msg) {}`。
- 重载了多个 `operator()` 的复杂仿函数。

因为当前类型推导只处理“一个明确参数”的可调用对象。

## 11. 常见代码模板

### 11.1 状态发布者模板

```cpp
struct MotorOutputMsg {
  float m1 = 0.0F;
  float m2 = 0.0F;
  float m3 = 0.0F;
  float m4 = 0.0F;
};

iFly::ObserverChannel<MotorOutputMsg, 4U, 4U> motorOutputChannel;

void MotorOutputProducerStep() {
  MotorOutputMsg msg;
  msg.m1 = 0.10F;
  msg.m2 = 0.12F;
  msg.m3 = 0.09F;
  msg.m4 = 0.11F;

  motorOutputChannel.Publish(msg);
}
```

### 11.2 状态消费者模板：只要最新值

```cpp
void MotorMonitorStep() {
  static auto consumer = motorOutputChannel.SubscribeLatest();

  if (!consumer.IsAttached()) {
    return;
  }

  MotorOutputMsg msg;
  const auto status = consumer.TryReadLatest(msg);
  if (status == iFly::ConsumeStatus::kNoData) {
    return;
  }

  // 使用 msg，它是当前最新电机输出
}
```

### 11.3 事件消费者模板：尽量按序处理

```cpp
void MotorLogStep() {
  static auto consumer = motorOutputChannel.SubscribeFromNext();

  if (!consumer.IsAttached()) {
    return;
  }

  MotorOutputMsg msg;
  while (true) {
    const auto status = consumer.TryRead(msg);
    if (status == iFly::ConsumeStatus::kNoData) {
      break;
    }

    if (status == iFly::ConsumeStatus::kOverflowed) {
      // 记录日志：日志任务处理太慢，部分历史被覆盖
    }

    // 写日志或送到下一级处理
  }
}
```

### 11.4 Hub 模板

```cpp
using FlightObserverHub = iFly::ObserverHub<
    iFly::ObserverChannel<AttitudeMsg, 8U, 4U>,
    iFly::ObserverChannel<BatteryMsg, 4U, 2U>>;

FlightObserverHub observerHub;

void PublishFlightState() {
  observerHub.Publish(AttitudeMsg {1.0F, 2.0F, 3.0F});
  observerHub.Publish(BatteryMsg {16.2F, 3.4F, 82U});
}

void ConsumeBattery() {
  static auto reader = observerHub.BindLatest([](const BatteryMsg &msg) {
    // 使用 msg
  });

  reader.PollLatest();
}
```

## 12. 设计边界和注意事项

### 12.1 一个通道只允许一个发布者

同一个 `ObserverChannel` 不要被多个线程同时调用 `Publish`。

原因：

- `nextSequence` 是通过 `publishedSequence_.load() + 1` 计算的。
- 多个发布者可能拿到同一个旧序号。
- 当前代码没有用 `compare_exchange` 分配唯一发布序号。

如果业务必须多发布者，建议在外层加互斥、任务串行化，或改造通道的发布序号分配逻辑。

### 12.2 一个消费者句柄不要被多个线程同时使用

`Consumer` 内部的 `nextSequence_` 是普通 `uint32_t`，不是原子变量。

所以：

- 多个线程可以各自持有不同的 `Consumer`。
- 不要多个线程共享并同时调用同一个 `Consumer` 的 `TryRead`。

### 12.3 通道对象要比消费者活得更久

`Consumer` 内部保存了：

```cpp
ObserverChannel *channel_;
```

如果通道对象先析构，消费者里会留下悬空指针。

推荐：

- 通道作为全局对象、静态对象或上层长期对象。
- 消费者作为任务内部静态对象或成员对象。
- 确保消费者不会超过通道生命周期。

### 12.4 回调不会自动执行

下面这段只创建了回调消费者：

```cpp
auto reader = hub.BindLatest([](const BatteryMsg &msg) {
  // ...
});
```

如果后面不调用：

```cpp
reader.PollOnce();
reader.PollLatest();
reader.Drain();
```

回调永远不会执行。

### 12.5 历史深度不是队列容量保证

`kHistoryDepth` 只表示“最多保留最近几条”。

当发布者比消费者快时，旧消息会被覆盖。

如果你必须处理每条事件：

- 增大 `kHistoryDepth`。
- 提高消费者频率。
- 使用 `TryRead` 和 `Drain`。
- 对 `kOverflowed` 做日志或告警。

如果消息本质是状态：

- 使用 `TryReadLatest`。
- 接受中间状态被跳过。

### 12.6 消息类型尽量轻量

发布和读取都会复制或赋值 `T`。

建议消息类型：

- 字段简单。
- 默认构造便宜。
- 拷贝赋值便宜。
- 不持有复杂资源。

不建议直接把大型容器、动态内存所有权或复杂对象放进通道。

### 12.7 标准 C++ 内存模型提醒

这个实现使用了 seqlock 风格的 `stamp` 校验来避免读到半更新快照。发布者写 `stamp` 和消费者读 `stamp` 使用了 release/acquire 顺序。

但 `slot.value` 本身不是原子对象，也没有互斥锁保护。对于严格按标准 C++ 内存模型分析的场景，发布者写 `slot.value` 与消费者读 `slot.value` 并发发生时，需要非常谨慎。

在嵌入式工程里，这类模式常用于简单可拷贝消息的快照传递。实际使用时建议：

- 保持单发布者。
- 保持消息类型简单。
- 避免在 `T` 里放自管理复杂资源。
- 如果目标平台、编译器或安全标准要求严格无数据竞争，需要重新评估该实现或给 `value` 访问加同步。

## 13. 常见问题排查

### 13.1 `TryRead` 一直返回 `kNoData`

按顺序检查：

1. 是否真的调用过 `Publish`。
2. 是否使用了 `SubscribeFromNext`，但订阅后还没有新发布。
3. 消费者是否 `IsAttached() == false`。
4. 通道对象是否已经析构或被错误移动。
5. 是否在错误的消息类型通道上订阅。

### 13.2 `SubscribeLatest` 后为什么读到旧消息

`SubscribeLatest` 的设计就是“如果已有消息，先读当前最新一条”。

如果你不想读订阅前的任何消息，应该使用：

```cpp
auto consumer = channel.SubscribeFromNext();
```

### 13.3 回调为什么没有被调用

`BindLatest` 和 `BindFromNext` 只绑定，不自动执行。

必须在任务循环里调用：

```cpp
reader.PollOnce();
```

或：

```cpp
reader.PollLatest();
```

或：

```cpp
reader.Drain();
```

### 13.4 为什么出现 `kOverflowed`

说明消费者想读的序号已经不在历史缓冲区里。

常见原因：

- 发布频率高。
- 消费频率低。
- `kHistoryDepth` 太小。
- 消费者长时间没有运行。

处理方法：

- 增大历史深度。
- 消费者每次运行时用 `Drain` 清空积压。
- 对状态类消息改用 `TryReadLatest`。
- 对事件类消息记录丢失并评估是否需要更可靠队列。

### 13.5 订阅失败

如果：

```cpp
auto consumer = channel.SubscribeLatest();
consumer.IsAttached(); // false
```

说明 `kMaxConsumers` 个槽位都被占用了。

处理方法：

- 增大 `kMaxConsumers`。
- 确认旧消费者对象是否已经析构。
- 避免在循环里反复创建消费者而不释放。
- 把消费者保存为任务成员或静态变量。

### 13.6 `ObserverHub` 编译报“requires exactly one channel”

原因是 Hub 里某个消息类型匹配不到通道，或匹配到了多个通道。

正确做法：

```cpp
using Hub = iFly::ObserverHub<
    iFly::ObserverChannel<AttitudeMsg>,
    iFly::ObserverChannel<BatteryMsg>>;
```

错误做法：

```cpp
using Hub = iFly::ObserverHub<
    iFly::ObserverChannel<BatteryMsg>,
    iFly::ObserverChannel<BatteryMsg>>;
```

同一个消息类型只能出现一次。

### 13.7 泛型 lambda 绑定失败

不建议这样写：

```cpp
auto reader = hub.BindLatest([](const auto &msg) {
  // ...
});
```

因为当前 `CallbackArgument` 需要从 `operator()` 推导出一个明确参数类型。

应该写成：

```cpp
auto reader = hub.BindLatest([](const BatteryMsg &msg) {
  // ...
});
```

## 14. 该怎么选择 API

按需求选择：

| 需求 | 推荐 API |
| --- | --- |
| 只传一种消息 | `ObserverChannel<T>` |
| 统一管理多种消息 | `ObserverHub<...>` |
| 发布已有对象 | `Publish(message)` |
| 用构造参数发布 | `Emplace<T>(args...)` 或 `channel.Emplace(args...)` |
| 新消费者先拿当前状态 | `SubscribeLatest()` |
| 新消费者只等之后的新消息 | `SubscribeFromNext()` |
| 每条消息尽量按序处理 | `TryRead()` 或 `PollOnce()` |
| 只关心当前最新状态 | `TryReadLatest()` 或 `PollLatest()` |
| 一轮处理完所有积压 | `Drain()` |
| 需要回调形式但自己轮询 | `MakeCallbackConsumer`、`BindLatest`、`BindFromNext` |

## 15. 推荐实践

推荐这样用：

1. 状态类数据使用 `SubscribeLatest` 加 `TryReadLatest`。
2. 事件类数据使用 `SubscribeFromNext` 加 `TryRead` 或 `Drain`。
3. 每个任务保存自己的消费者对象，不要每轮循环重新订阅。
4. 每次订阅后检查 `IsAttached()`。
5. 对 `kOverflowed` 做统计或日志，不要完全忽略。
6. 根据发布频率和消费者周期设置 `kHistoryDepth`。
7. 让通道对象生命周期长于所有消费者。
8. 保持同一个通道只有一个发布者。
9. 让消息类型简单、可默认构造、可拷贝赋值。
10. 使用 `ObserverHub` 时保证每种消息类型只出现一次。

## 16. 一句话总结

`observer_channel.hpp` 提供的是一个固定容量、单发布者、多消费者、主动轮询式的观察者快照通道。发布者只写入消息，消费者各自维护读取进度；如果消费者落后超过历史深度，旧数据会被覆盖；如果只关心最新状态，可以直接追最新；如果要统一管理多种消息，可以使用 `ObserverHub`。
