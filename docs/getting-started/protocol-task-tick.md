---
title: 协议、任务与时间接口
description: task_mage、tick、ObserverChannel、CRSF 与 SBUS 的使用示例
---

# 协议、任务与时间接口

## `task_mage`

`iFly/app/task_mage/task.*` 为上层提供任务语义接口，底层由 `SoftTimerService` 执行。

常用接口如下：

- `TaskCreatePeriodic`
- `TaskCreateOneShot`
- `TaskDelete`
- `TaskDelay`
- `TaskSuspend`
- `TaskResume`
- `TaskDispatch`

周期任务示例：

```cpp
#include "task.hpp"
#include "tick.hpp"

namespace {

void HeartbeatTask(void *context)
{
  uint32_t *counter = static_cast<uint32_t *>(context);
  ++(*counter);
}

} // namespace

void UseTaskModule()
{
  uint32_t counter = 0U;

  const iFly::TaskHandle handle =
      iFly::TaskCreatePeriodic(&HeartbeatTask,
                               &counter,
                               10U,
                               iFly::SoftTimerService::kLowestPriority,
                               0U,
                               "heartbeat");

  while (iFly::TaskIsAlive(handle)) {
    (void)iFly::TaskDispatch();
    iFly::tick::DelayMs(1U);

    if (counter >= 100U) {
      (void)iFly::TaskDelete(handle);
    }
  }
}
```

## `tick`

`iFly/app/tick/tick.*` 对外提供毫秒、微秒、纳秒时间读取，以及阻塞与非阻塞延时器。

```cpp
#include "tick.hpp"

void UseTickModule()
{
  const uint32_t start_ms = iFly::tick::NowMs();
  iFly::tick::DelayMs(10U);

  if (iFly::tick::ElapsedMs(start_ms) >= 10U) {
    iFly::tick::DelayUs(20U);
  }

  iFly::tick::NonBlockingDelayMs period {};
  period.Start(50U);

  while (!period.ConsumeIfExpired()) {
  }
}
```

## `ObserverChannel`

`ObserverChannel` 适合单发布者、多消费者的快照分发场景。每个消费者都维护独立的读取序号。

```cpp
#include "observer_channel.hpp"

struct AttitudeSample final {
  float roll = 0.0f;
  float pitch = 0.0f;
};

void UseObserverChannel()
{
  iFly::ObserverChannel<AttitudeSample, 8U, 2U> channel;
  auto consumer = channel.SubscribeFromNext();

  channel.Publish({1.5f, -0.3f});

  AttitudeSample latest {};
  const iFly::ConsumeStatus status = consumer.TryReadLatest(latest);
  if (status != iFly::ConsumeStatus::kNoData) {
    const float roll = latest.roll;
    (void)roll;
  }
}
```

## `CrsfProtocol`

`CrsfProtocol` 既支持单帧编解码，也支持字节流解析。

```cpp
#include "crsf_protocol.hpp"

void UseCrsfProtocol()
{
  iFly::CrsfRcChannelsPacked tx_channels {};
  tx_channels.channels[0] = 992U;
  tx_channels.channels[1] = 992U;
  tx_channels.channels[2] = 172U;
  tx_channels.channels[3] = 1811U;

  uint8_t raw_frame[iFly::CrsfProtocol::kMaxPacketSize] {};
  uint32_t written_length = 0U;

  (void)iFly::CrsfProtocol::EncodeRcChannelsPacked(
      0xC8U, tx_channels, raw_frame, sizeof(raw_frame), &written_length);

  iFly::CrsfFrame frame {};
  if (iFly::CrsfProtocol::TryDecodeFrame(raw_frame, written_length, &frame)) {
    iFly::CrsfRcChannelsPacked rx_channels {};
    if (iFly::CrsfProtocol::DecodeRcChannelsPacked(frame, &rx_channels)) {
      const uint16_t throttle = rx_channels.channels[2];
      (void)throttle;
    }
  }
}
```

流式解析示例：

```cpp
iFly::CrsfProtocol protocol;
iFly::CrsfFrame frames[4] {};
const uint32_t frame_count = protocol.Parse(rx_bytes, rx_length, frames, 4U);
```

## `SbusProtocol`

`SbusProtocol` 提供 SBUS 单帧和字节流两类接口。

```cpp
#include "sbus_protocol.hpp"

void UseSbusProtocol()
{
  iFly::SbusProtocol protocol;

  iFly::SbusFrame tx_frame {};
  tx_frame.channels[0] = 1024U;
  tx_frame.channels[1] = 1024U;
  tx_frame.channels[2] = 172U;
  tx_frame.channel17 = true;

  uint8_t raw_frame[iFly::SbusProtocol::kFrameSize] {};
  (void)protocol.Encode(tx_frame, raw_frame);

  iFly::SbusFrame rx_frame {};
  if (iFly::SbusProtocol::TryDecodeFrame(raw_frame, sizeof(raw_frame), &rx_frame)) {
    const uint16_t throttle = rx_frame.channels[2];
    (void)throttle;
  }
}
```
