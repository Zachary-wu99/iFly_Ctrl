#include "uart_dma.hpp"

#include <string.h>

#include "double_buffer.hpp"
#include "main.h"
#include "usart.h"

namespace {

/* 把枚举端口号转换成数组下标，便于统一管理 8 路软件槽位。 */
constexpr uint8_t PortIndex(iFly::UartPortId port) noexcept {
  return static_cast<uint8_t>(port);
}

/*
 * 简单的关中断保护对象。
 *
 * 这里的保护范围主要覆盖：
 * - 串口端口运行时状态表
 * - TX 双缓冲切换
 * - RX DMA 写指针解析与用户队列上抛
 *
 * 目的不是“把所有事都放到中断里做”，而是避免主循环与 HAL 回调
 * 同时操作同一份状态时互相打断。
 */
/*
 * UART TX 双缓冲区。
 *
 * 发送方向的数据流不是“上层一写就直接 DMA 发走”，而是：
 * 1. 上层先写入 TX 无锁队列；
 * 2. 底层从队列里取一包，装入 inactive 槽位；
 * 3. 当前 DMA 空闲时，把 inactive 槽位切成 active 并发出；
 * 4. 另一块槽位可以同时预装下一包数据。
 *
 * 这样做的好处是：
 * - 不需要让 DMA 直接引用上层传进来的瞬时缓冲区
 * - 当前包发送时，可以并行准备下一包
 * - 逻辑上与 USB CDC 的双缓冲发送模型保持一致
 */
class UartTxDoubleBuffer final : public iFly::StaticByteDoubleBuffer<iFly::UartDmaService::kFixedTxDmaBufferSize> {
public:
  UartTxDoubleBuffer() noexcept
      : iFly::StaticByteDoubleBuffer<iFly::UartDmaService::kFixedTxDmaBufferSize>(
            iFly::UartDmaService::kFixedTxDmaBufferSize) {
  }

  void Recreate() noexcept {
    (void)iFly::StaticByteDoubleBuffer<iFly::UartDmaService::kFixedTxDmaBufferSize>::Recreate(
        iFly::UartDmaService::kFixedTxDmaBufferSize);
  }

  bool IsCreated() const noexcept {
    return iFly::StaticByteDoubleBuffer<iFly::UartDmaService::kFixedTxDmaBufferSize>::IsCreated();
  }
};

struct UartPortSlot final {
  /** 该端口绑定到哪个 HAL UART 句柄。 */
  UART_HandleTypeDef *huart = nullptr;
  /** 上层统一 RX 无锁队列，收到数据后最终写入这里。 */
  iFly::LockFreeQueueBase *appRxQueue = nullptr;
  /** 发送方向的字节无锁队列。 */
  iFly::StaticLockFreeQueue<iFly::UartDmaService::kFixedTxQueueStorageSize> txQueue {};
  /** 发送方向使用的双缓冲 DMA staging。 */
  UartTxDoubleBuffer txBuffers {};
  /** 接收方向 DMA 环形缓冲区首地址。 */
  uint8_t rxDmaBuffer[iFly::UartDmaService::kFixedRxDmaBufferSize] {};
  /** 接收方向 DMA 环形缓冲区大小。 */
  uint16_t rxDmaBufferSize = iFly::UartDmaService::kFixedRxDmaBufferSize;
  /** 上一次已经处理到 RX DMA 环形缓冲区的哪个位置。 */
  uint16_t rxLastPos = 0U;
  /** 上抛到用户层 RX 队列时累计丢弃的字节数。 */
  std::atomic<uint32_t> rxDropped {0U};
  /** 当前端口是否已经初始化完成。 */
  std::atomic<bool> initialized {false};
  /** 当前是否有一个 TX DMA 正在发送。 */
  std::atomic<bool> txBusy {false};
  /** 上层写入或回调完成时，可以多次提交 TX 推进请求。 */
  std::atomic<uint32_t> txServiceRequests {0U};
  /** 同一时刻只允许一个执行流真正推进 TX 状态机。 */
  std::atomic<bool> txServiceRunning {false};
};

/* 软件层统一的 8 路端口状态表。 */
class UartDmaServiceStorage final {
public:
  UartPortSlot slots[iFly::UartDmaService::kMaxPorts] {};
};

/* 返回全局唯一的端口状态表。 */
UartDmaServiceStorage &Storage() noexcept {
  static UartDmaServiceStorage storage;
  return storage;
}

/*
 * 默认端口映射。
 *
 */
UART_HandleTypeDef *DefaultHandleForPort(iFly::UartPortId port) noexcept {
  switch (port) {
    case iFly::UartPortId::kUsart1:
      return &huart1;
    case iFly::UartPortId::kUsart2:
      return &huart2;
    case iFly::UartPortId::kUsart3:
      return &huart3;
    case iFly::UartPortId::kUart4:
      return &huart4;
    case iFly::UartPortId::kUart5:
      return &huart5;
    case iFly::UartPortId::kUsart6:
      return &huart6;
    case iFly::UartPortId::kUart7:
    case iFly::UartPortId::kUart8:
    case iFly::UartPortId::kCount:
    default:
      return nullptr;
  }
}

/* 通过 HAL 句柄反查它属于哪一个软件端口槽位。 */
UartPortSlot *FindSlot(UART_HandleTypeDef *huart) noexcept {
  if (huart == nullptr) {
    return nullptr;
  }

  for (uint8_t index = 0U; index < iFly::UartDmaService::kMaxPorts; ++index) {
    UartPortSlot &slot = Storage().slots[index];
    if (slot.huart == huart) {
      return &slot;
    }
  }

  return nullptr;
}

/*
 * 确保 RX DMA 环形缓冲区已经创建且大小正确。
 *
 * 如果大小变化，就重新申请；同时把“已处理位置”归零，
 * 让新的 DMA 接收从头开始计数。
 */
/*
 * 启动“DMA + IDLE”接收。
 *
 * 这里使用 HAL 的 `HAL_UARTEx_ReceiveToIdle_DMA()`：
 * - DMA 一直把字节写入环形缓冲区
 * - 半满 / 全满 / 空闲帧间隔 时触发 RxEvent 回调
 * - 回调里再把“新增的那段数据”搬到上层 RX 队列
 */
bool StartRx(UartPortSlot &slot) noexcept {
  if ((slot.huart == nullptr) || (slot.huart->hdmarx == nullptr) || (slot.rxDmaBufferSize == 0U)) {
    return false;
  }

  (void)HAL_UART_AbortReceive(slot.huart);
  slot.rxLastPos = 0U;

  return HAL_UARTEx_ReceiveToIdle_DMA(slot.huart, slot.rxDmaBuffer, slot.rxDmaBufferSize) == HAL_OK;
}

/* 把一段连续收到的数据尽力写入用户层 RX 队列，并统计丢字节数。 */
void PushRxRange(UartPortSlot &slot, const uint8_t *data, uint16_t length) noexcept {
  if ((data == nullptr) || (length == 0U)) {
    return;
  }

  uint32_t pushed = 0U;
  if ((slot.appRxQueue != nullptr) && slot.appRxQueue->IsCreated()) {
    pushed = slot.appRxQueue->Enqueue(data, length);
  }

  if (pushed < length) {
    (void)slot.rxDropped.fetch_add(length - pushed, std::memory_order_relaxed);
  }
}

/*
 * 根据当前 DMA 写入位置，计算“自上次处理后新增了哪一段数据”。
 *
 * 分两种情况：
 * 1. `currentPos > previousPos`
 *    说明 DMA 还没绕回，直接处理 `[previousPos, currentPos)`。
 * 2. `currentPos < previousPos`
 *    说明 DMA 已经回卷，要拆成两段：
 *    `[previousPos, bufferEnd)` + `[0, currentPos)`。
 */
void ProcessRxDelta(UartPortSlot &slot, uint16_t currentPos) noexcept {
  const uint16_t bufferSize = slot.rxDmaBufferSize;
  if (bufferSize == 0U) {
    return;
  }

  if (currentPos > bufferSize) {
    currentPos = bufferSize;
  }

  const uint16_t previousPos = slot.rxLastPos;
  if (currentPos == previousPos) {
    return;
  }

  if (currentPos > previousPos) {
    PushRxRange(slot, slot.rxDmaBuffer + previousPos, static_cast<uint16_t>(currentPos - previousPos));
  } else {
    PushRxRange(slot, slot.rxDmaBuffer + previousPos, static_cast<uint16_t>(bufferSize - previousPos));
    if (currentPos > 0U) {
      PushRxRange(slot, slot.rxDmaBuffer, currentPos);
    }
  }

  slot.rxLastPos = (currentPos == bufferSize) ? 0U : currentPos;
}

/*
 * 尝试把下一包待发送数据从 TX 无锁队列取出，并装载到 inactive 槽位。
 *
 * 如果 inactive 槽位里已经有包了，就不重复装。
 */
uint32_t LoadTxPacketToInactiveBuffer(UartPortSlot &slot) noexcept {
  if (!slot.txQueue.IsCreated() || !slot.txBuffers.IsCreated() || slot.txBuffers.HasInactiveData()) {
    return 0U;
  }

  uint8_t *buffer = slot.txBuffers.InactiveBuffer();
  if (buffer == nullptr) {
    return 0U;
  }

  const uint32_t pulled = slot.txQueue.Dequeue(buffer, slot.txBuffers.PacketSize());
  if (pulled > 0U) {
    slot.txBuffers.SetInactiveLength(static_cast<uint16_t>(pulled));
  }

  return pulled;
}

/*
 * 推进 TX 状态机。
 *
 * 它做两件事：
 * 1. 先尽量把 inactive 槽位装满下一包数据；
 * 2. 如果当前 DMA 空闲且 inactive 有包，就立刻发出。
 *
 * 这样，无论是上层刚写入新数据，还是上一个 DMA 包刚发完，
 * 都可以统一调用这个函数继续把链路往前推。
 */
void ServiceTxPathOnce(UartPortSlot &slot) noexcept {
  if ((slot.huart == nullptr) || (slot.huart->hdmatx == nullptr) ||
      !slot.initialized.load(std::memory_order_acquire) ||
      !slot.txQueue.IsCreated() || !slot.txBuffers.IsCreated()) {
    return;
  }

  (void)LoadTxPacketToInactiveBuffer(slot);
  if (slot.txBusy.load(std::memory_order_acquire) || !slot.txBuffers.HasInactiveData()) {
    return;
  }

  uint8_t *data = slot.txBuffers.InactiveBuffer();
  const uint16_t length = slot.txBuffers.InactiveLength();
  if ((data == nullptr) || (length == 0U)) {
    return;
  }

  bool expectedBusy = false;
  if (!slot.txBusy.compare_exchange_strong(expectedBusy, true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    return;
  }

  if (HAL_UART_Transmit_DMA(slot.huart, data, length) == HAL_OK) {
    slot.txBuffers.SwapBuffers();
    (void)LoadTxPacketToInactiveBuffer(slot);
    return;
  }

  slot.txBusy.store(false, std::memory_order_release);
}

void ServiceTxPath(UartPortSlot &slot) noexcept {
  (void)slot.txServiceRequests.fetch_add(1U, std::memory_order_acq_rel);
  if (slot.txServiceRunning.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  for (;;) {
    while (slot.txServiceRequests.exchange(0U, std::memory_order_acq_rel) != 0U) {
      ServiceTxPathOnce(slot);
    }

    slot.txServiceRunning.store(false, std::memory_order_release);
    if (slot.txServiceRequests.load(std::memory_order_acquire) == 0U) {
      return;
    }

    if (slot.txServiceRunning.exchange(true, std::memory_order_acq_rel)) {
      return;
    }
  }
}

} // namespace

namespace iFly {

/* 端口名转换工具，主要给日志或调试用。 */
const char *ToString(UartPortId port) noexcept {
  switch (port) {
    case UartPortId::kUsart1:
      return "USART1";
    case UartPortId::kUsart2:
      return "USART2";
    case UartPortId::kUsart3:
      return "USART3";
    case UartPortId::kUart4:
      return "UART4";
    case UartPortId::kUart5:
      return "UART5";
    case UartPortId::kUsart6:
      return "USART6";
    case UartPortId::kUart7:
      return "UART7";
    case UartPortId::kUart8:
      return "UART8";
    case UartPortId::kCount:
    default:
      return "UART";
  }
}

/* 单例服务入口。 */
UartDmaService &UartDmaService::Instance() noexcept {
  static UartDmaService instance;
  return instance;
}

/*
 * 手动绑定某个逻辑端口到 конкрет HAL UART 句柄。
 *
 * 正常情况下不一定需要显式调用，因为 `InitPort()` 会按默认映射自动填充。
 */
void UartDmaService::AttachHardware(UartPortId port, UART_HandleTypeDef *huart) noexcept {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.huart = huart;
  slot.initialized.store(false, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

/*
 * 初始化某个端口的完整流程：
 * 1. 获取或确定底层 HAL UART 句柄；
 * 2. 挂接上层统一 RX 队列；
 * 3. 创建/重建 TX 无锁队列；
 * 4. 创建/重建 TX 双缓冲；
 * 5. 创建/重建 RX DMA 环形缓冲；
 * 6. 启动 `ReceiveToIdle DMA` 接收；
 * 7. 尝试推进一次发送状态机。
 */
bool UartDmaService::InitPort(UartPortId port, LockFreeQueueBase *rxQueue) noexcept {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  if (slot.huart == nullptr) {
    slot.huart = DefaultHandleForPort(port);
  }

  slot.appRxQueue = rxQueue;
  if ((slot.appRxQueue != nullptr) && slot.appRxQueue->IsCreated()) {
    slot.appRxQueue->Clear();
  }

  slot.txQueue.Recreate();
  slot.txQueue.Clear();
  slot.txBuffers.Recreate();
  slot.txBuffers.Clear();
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.rxDmaBufferSize = kFixedRxDmaBufferSize;
  slot.rxLastPos = 0U;
  (void)memset(slot.rxDmaBuffer, 0, sizeof(slot.rxDmaBuffer));

  slot.initialized.store(StartRx(slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }

  return slot.initialized.load(std::memory_order_acquire);
}

/* 停止某个端口并释放动态申请的 DMA/RX/TX 运行时资源。 */
void UartDmaService::DeinitPort(UartPortId port) noexcept {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.initialized.store(false, std::memory_order_release);
  if (slot.huart != nullptr) {
    (void)HAL_UART_AbortReceive(slot.huart);
  }

  slot.appRxQueue = nullptr;
  slot.txQueue.Recreate();
  slot.txQueue.Clear();
  slot.txBuffers.Clear();
  (void)memset(slot.rxDmaBuffer, 0, sizeof(slot.rxDmaBuffer));
  slot.rxDmaBufferSize = kFixedRxDmaBufferSize;
  slot.rxLastPos = 0U;
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

/*
 * 写入发送数据。
 *
 * 这里不会阻塞等待 DMA 发送完成，而是：
 * - 先把数据尽力写入 TX 无锁队列
 * - 然后立即调用 `ServiceTxPath()`，尝试把链路往前推进
 */
uint32_t UartDmaService::Write(UartPortId port, const uint8_t *data, uint32_t len) noexcept {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  if (!slot.initialized.load(std::memory_order_acquire) || !slot.txQueue.IsCreated()) {
    return 0U;
  }

  const uint32_t accepted = slot.txQueue.Enqueue(data, len);
  ServiceTxPath(slot);
  return accepted;
}

/* 查询发送队列剩余空间。 */
uint32_t UartDmaService::TxFree(UartPortId port) const noexcept {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.FreeSize() : 0U;
}

/* 查询发送队列已用空间。 */
uint32_t UartDmaService::TxUsed(UartPortId port) const noexcept {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.UsedSize() : 0U;
}

/* 查询 RX 上抛过程累计丢弃的字节数。 */
uint32_t UartDmaService::RxDropped(UartPortId port) const noexcept {
  return Storage().slots[PortIndex(port)].rxDropped.load(std::memory_order_acquire);
}

/* 当前端口是否已具备 HAL UART 句柄和 DMA RX/TX 资源。 */
bool UartDmaService::IsReady(UartPortId port) const noexcept {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.initialized.load(std::memory_order_acquire) && (slot.huart != nullptr) &&
         (slot.huart->hdmarx != nullptr) &&
         (slot.huart->hdmatx != nullptr);
}

/* HAL 在“半满 / 满 / IDLE”等接收事件发生时回调到这里。 */
void UartDmaService::OnRxEvent(UART_HandleTypeDef *huart, uint16_t size) noexcept {
  UartPortSlot *slot = FindSlot(huart);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  ProcessRxDelta(*slot, size);
}

/* HAL 在一包 DMA 发送完成后回调到这里，继续推进下一包。 */
void UartDmaService::OnTxComplete(UART_HandleTypeDef *huart) noexcept {
  UartPortSlot *slot = FindSlot(huart);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  slot->txBusy.store(false, std::memory_order_release);
  slot->txBuffers.ClearActive();
  ServiceTxPath(*slot);
}

/* 出错后尽量重启接收，并继续尝试发送。 */
void UartDmaService::OnError(UART_HandleTypeDef *huart) noexcept {
  UartPortSlot *slot = FindSlot(huart);
  if ((slot == nullptr) || (slot->huart == nullptr)) {
    return;
  }

  slot->initialized.store(StartRx(*slot), std::memory_order_release);
  if (slot->initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(*slot);
  }
}

} // namespace iFly

/* HAL 弱符号回调桥接到 C++ 单例。 */
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  iFly::UartDmaService::Instance().OnRxEvent(huart, Size);
}

/* HAL 弱符号回调桥接到 C++ 单例。 */
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  iFly::UartDmaService::Instance().OnTxComplete(huart);
}

/* HAL 弱符号回调桥接到 C++ 单例。 */
extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  iFly::UartDmaService::Instance().OnError(huart);
}
