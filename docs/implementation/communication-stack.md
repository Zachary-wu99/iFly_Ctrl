---
title: 通信与缓冲实现
description: 队列、双缓冲、SerialIoBase、UART DMA、USB CDC 与 Shell 的实现说明
---

# 通信与缓冲实现

## 分层关系

当前通信相关模块的关系如下：

```text
Shell
  -> SerialIoBase
    -> HardwareUart
      -> UartDmaService
    -> UsbUart
      -> UsbCdcAcm

UartDmaService / UsbCdcAcm
  -> LockFreeQueueBase
  -> StaticByteDoubleBuffer
  -> HAL 回调
```

## `LockFreeQueueBase`

`LockFreeQueueBase` 使用环形缓冲区保存字节流，内部保留 1 字节空位区分“空”和“满”。

关键代码如下：

```cpp
const uint32_t used = Distance(head, tail, size);
const uint32_t free = (size - 1U) - used;
const uint32_t writeLength = usermath::Min<uint32_t>(length, free);

const uint32_t nextHead = (head + writeLength) % size;
head_.store(nextHead, std::memory_order_release);
```

代码含义如下：

- `Distance(head, tail, size)` 计算当前已使用字节数。
- `size - 1U` 表示真实可用容量，最后 1 字节永远不写入。
- 生产者只推进 `head_`，消费者只推进 `tail_`。
- `Dequeue()` 使用 `compare_exchange_weak` 更新 `tail_`，用于竞争多个消费者。

## `StaticByteDoubleBuffer`

`double_buffer.hpp` 提供活动槽与备用槽两份缓冲。发送路径使用活动槽提交给 DMA 或 USB 端点，备用槽继续装载下一包数据。

这一结构在两个模块中直接使用：

- `UartDmaService` 的 `txBuffers`
- `UsbCdcAcm` 的 `txEndpointBuffer_` 与 `rxEndpointBuffer_`

## `SerialIoBase`

`SerialIoBase` 是 `HardwareUart` 与 `UsbUart` 的共同基类，统一了以下接口：

- `Init()`
- `Write()`
- `Read()`
- `TxFree() / TxUsed()`
- `RxDropped()`
- `IsConnected()`

上层 `Shell` 不需要区分底层链路来自 `UART` 还是 `USB`，只依赖 `SerialIoBase`。

## `UartDmaService`

`UartDmaService` 使用 DMA 环形接收区加应用层 RX 队列的方式处理串口输入。

接收增量处理的核心逻辑如下：

```cpp
if (currentPos > previousPos) {
  PushRxRange(slot, slot.rxDmaBuffer + previousPos,
              static_cast<uint16_t>(currentPos - previousPos));
} else {
  PushRxRange(slot, slot.rxDmaBuffer + previousPos,
              static_cast<uint16_t>(bufferSize - previousPos));
  if (currentPos > 0U) {
    PushRxRange(slot, slot.rxDmaBuffer, currentPos);
  }
}
```

代码含义如下：

- `HAL_UARTEx_RxEventCallback()` 把本次 DMA 写入位置传给 `OnRxEvent()`。
- `ProcessRxDelta()` 根据 `currentPos` 与 `rxLastPos` 计算新增数据区间。
- 当 DMA 写指针回绕时，函数分两段搬运数据。
- 数据最终通过 `PushRxRange()` 写入上层 RX 队列。

发送路径会先把数据写入 `txQueue`，再由 `ServiceTxPath()` 装载到双缓冲并触发 `HAL_UART_Transmit_DMA(...)`。

## `UsbCdcAcm`

`UsbCdcAcm` 自己实现了 CDC ACM 协议层，包括：

- 设备描述符和配置描述符
- `EP0` 控制传输
- `Bulk IN/OUT` 数据端点
- `Interrupt IN` 命令端点
- 上层 RX 队列与 TX 队列管理

发送路径的核心逻辑如下：

```cpp
if (!txEndpointBuffer_.HasInactiveData()) {
  continue;
}

uint8_t *data = txEndpointBuffer_.InactiveBuffer();
const uint16_t length = txEndpointBuffer_.InactiveLength();

if (UsbPcd().Transmit(kEpCdcDataIn, data, length) == HAL_OK) {
  txEndpointBuffer_.SwapBuffers();
  (void)LoadTxPacketToInactiveBuffer();
  continue;
}
```

代码含义如下：

- 发送数据先进入 `txQueue_`。
- `LoadTxPacketToInactiveBuffer()` 将一包数据装载到备用缓冲。
- `Transmit()` 成功后，备用缓冲切换为活动缓冲。
- `HAL_PCD_DataInStageCallback()` 在传输完成后清空活动缓冲并继续推进发送链路。

## `Shell`

`Shell` 位于通信栈最上层，内部维护 `SessionState` 状态机：

- `kDisconnected`
- `kActivationPrompt`
- `kSessionAnimation`
- `kPasswordPrompt`
- `kReady`

`Poll()` 的行为如下：

- 检查底层链路是否已经连接
- 读取 `SerialIoBase` 中的输入字节
- 按字节处理退格、回车、可打印字符
- 在 `kReady` 状态下执行内建命令、用户命令、函数调用和参数读写

当前 `FlightCtrlCli` 就是通过 `Shell` 注册参数和函数后，再把 `Shell::Poll()` 放入 `cli_poll` 周期任务中执行。
