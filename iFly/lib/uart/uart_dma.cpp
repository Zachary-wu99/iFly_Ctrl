#include "uart_dma.hpp"

#include <string.h>

#include "usart.h"

namespace {

UART_HandleTypeDef *UartHandle(void *handle) {
  return static_cast<UART_HandleTypeDef *>(handle);
}

const UART_HandleTypeDef *UartHandle(const void *handle) {
  return static_cast<const UART_HandleTypeDef *>(handle);
}

constexpr uint8_t PortIndex(iFly::UartPortId port) {
  return static_cast<uint8_t>(port);
}

struct UartPortSlot final {
  /** 该端口绑定到哪个 HAL UART 句柄。 */
  void *huart = nullptr;
  /** 上层统一 RX 无锁队列，收到数据后最终写入这里。 */
  iFly::LockFreeQueueBase *appRxQueue = nullptr;
  /** 发送方向的字节无锁队列。 */
  iFly::LockFreeQueueBase *txQueue = nullptr;
  /** 发送方向使用的双缓冲 DMA staging。 */
  iFly::UartDmaTxDoubleBufferBase *txBuffers = nullptr;
  /** 接收方向 DMA 环形缓冲区首地址。 */
  uint8_t *rxDmaBuffer = nullptr;
  /** 接收方向 DMA 环形缓冲区大小。 */
  uint16_t rxDmaBufferSize = 0U;
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

class UartDmaServiceStorage final {
public:
  UartPortSlot slots[iFly::UartDmaService::kMaxPorts] {};
};

UartDmaServiceStorage &Storage() {
  static UartDmaServiceStorage storage;
  return storage;
}

void *DefaultHandleForPort(iFly::UartPortId port) {
  switch (port) {
    case iFly::UartPortId::kUart1:
      return &huart1;
    case iFly::UartPortId::kUart2:
      return &huart2;
    case iFly::UartPortId::kUart3:
      return &huart3;
    case iFly::UartPortId::kUart4:
      return &huart4;
    case iFly::UartPortId::kUart5:
      return &huart5;
    case iFly::UartPortId::kUart6:
      return &huart6;
    case iFly::UartPortId::kUart7:
    case iFly::UartPortId::kUart8:
    case iFly::UartPortId::kCount:
    default:
      return nullptr;
  }
}

UartPortSlot *FindSlot(void *huart) {
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

bool StartRx(UartPortSlot &slot) {
  UART_HandleTypeDef *huart = UartHandle(slot.huart);
  if ((huart == nullptr) || (huart->hdmarx == nullptr) ||
      (slot.rxDmaBuffer == nullptr) || (slot.rxDmaBufferSize == 0U)) {
    return false;
  }

  (void)HAL_UART_AbortReceive(huart);
  slot.rxLastPos = 0U;

  return HAL_UARTEx_ReceiveToIdle_DMA(huart, slot.rxDmaBuffer, slot.rxDmaBufferSize) == HAL_OK;
}

void PushRxRange(UartPortSlot &slot, const uint8_t *data, uint16_t length) {
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

void ProcessRxDelta(UartPortSlot &slot, uint16_t currentPos) {
  const uint16_t bufferSize = slot.rxDmaBufferSize;
  if ((slot.rxDmaBuffer == nullptr) || (bufferSize == 0U)) {
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

uint32_t LoadTxPacketToInactiveBuffer(UartPortSlot &slot) {
  if ((slot.txQueue == nullptr) || (slot.txBuffers == nullptr) ||
      !slot.txQueue->IsCreated() || !slot.txBuffers->IsCreated() ||
      slot.txBuffers->HasInactiveData()) {
    return 0U;
  }

  uint8_t *buffer = slot.txBuffers->InactiveBuffer();
  if (buffer == nullptr) {
    return 0U;
  }

  const uint32_t pulled = slot.txQueue->Dequeue(buffer, slot.txBuffers->PacketSize());
  if (pulled > 0U) {
    slot.txBuffers->SetInactiveLength(static_cast<uint16_t>(pulled));
  }

  return pulled;
}

void ServiceTxPathOnce(UartPortSlot &slot) {
  UART_HandleTypeDef *huart = UartHandle(slot.huart);
  if ((huart == nullptr) || (huart->hdmatx == nullptr) ||
      !slot.initialized.load(std::memory_order_acquire) ||
      (slot.txQueue == nullptr) || (slot.txBuffers == nullptr) ||
      !slot.txQueue->IsCreated() || !slot.txBuffers->IsCreated()) {
    return;
  }

  (void)LoadTxPacketToInactiveBuffer(slot);
  if (slot.txBusy.load(std::memory_order_acquire) || !slot.txBuffers->HasInactiveData()) {
    return;
  }

  uint8_t *data = slot.txBuffers->InactiveBuffer();
  const uint16_t length = slot.txBuffers->InactiveLength();
  if ((data == nullptr) || (length == 0U)) {
    return;
  }

  bool expectedBusy = false;
  if (!slot.txBusy.compare_exchange_strong(expectedBusy, true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    return;
  }

  if (HAL_UART_Transmit_DMA(huart, data, length) == HAL_OK) {
    slot.txBuffers->SwapBuffers();
    (void)LoadTxPacketToInactiveBuffer(slot);
    return;
  }

  slot.txBusy.store(false, std::memory_order_release);
}

void ServiceTxPath(UartPortSlot &slot) {
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

bool UartDmaTxDoubleBufferBase::Create(uint8_t *buffer0, uint8_t *buffer1, uint16_t bufferSize) {
  if ((buffer0 == nullptr) || (buffer1 == nullptr) || (bufferSize == 0U)) {
    Delete();
    return false;
  }

  buffers_[0] = buffer0;
  buffers_[1] = buffer1;
  bufferSize_ = bufferSize;
  ResetLengths();
  return true;
}

void UartDmaTxDoubleBufferBase::Delete() {
  buffers_[0] = nullptr;
  buffers_[1] = nullptr;
  bufferSize_ = 0U;
  ResetLengths();
}

void UartDmaTxDoubleBufferBase::Clear() {
  ResetLengths();
}

bool UartDmaTxDoubleBufferBase::IsCreated() const {
  return (buffers_[0] != nullptr) && (buffers_[1] != nullptr) && (bufferSize_ > 0U);
}

uint16_t UartDmaTxDoubleBufferBase::PacketSize() const {
  return bufferSize_;
}

uint8_t *UartDmaTxDoubleBufferBase::InactiveBuffer() {
  return IsCreated() ? buffers_[InactiveSlotIndex()] : nullptr;
}

void UartDmaTxDoubleBufferBase::SetInactiveLength(uint16_t length) {
  lengths_[InactiveSlotIndex()] = length;
}

uint16_t UartDmaTxDoubleBufferBase::InactiveLength() const {
  return lengths_[InactiveSlotIndex()];
}

void UartDmaTxDoubleBufferBase::ClearActive() {
  lengths_[activeSlot_] = 0U;
}

bool UartDmaTxDoubleBufferBase::HasInactiveData() const {
  return InactiveLength() != 0U;
}

void UartDmaTxDoubleBufferBase::SwapBuffers() {
  activeSlot_ ^= 0x01U;
}

void UartDmaTxDoubleBufferBase::ResetLengths() {
  lengths_[0] = 0U;
  lengths_[1] = 0U;
  activeSlot_ = 0U;
}

uint8_t UartDmaTxDoubleBufferBase::InactiveSlotIndex() const {
  return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
}

const char *ToString(UartPortId port) {
  switch (port) {
    case UartPortId::kUart1:
      return "USART1";
    case UartPortId::kUart2:
      return "USART2";
    case UartPortId::kUart3:
      return "USART3";
    case UartPortId::kUart4:
      return "UART4";
    case UartPortId::kUart5:
      return "UART5";
    case UartPortId::kUart6:
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

UartDmaService &UartDmaService::Instance() {
  static UartDmaService instance;
  return instance;
}

void UartDmaService::AttachHardware(UartPortId port, void *huart) {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.huart = huart;
  slot.initialized.store(false, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

bool UartDmaService::InitPort(UartPortId port,
                              LockFreeQueueBase *rxQueue,
                              LockFreeQueueBase *txQueue,
                              UartDmaTxDoubleBufferBase *txBuffers,
                              uint8_t *rxDmaBuffer,
                              uint16_t rxDmaBufferSize) {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  if (slot.huart == nullptr) {
    slot.huart = DefaultHandleForPort(port);
  }

  slot.initialized.store(false, std::memory_order_release);
  slot.appRxQueue = rxQueue;
  slot.txQueue = txQueue;
  slot.txBuffers = txBuffers;
  slot.rxDmaBuffer = rxDmaBuffer;
  slot.rxDmaBufferSize = rxDmaBufferSize;

  if ((slot.txQueue == nullptr) || (slot.txBuffers == nullptr) ||
      (slot.rxDmaBuffer == nullptr) || (slot.rxDmaBufferSize == 0U) ||
      !slot.txQueue->IsCreated() || !slot.txBuffers->IsCreated()) {
    return false;
  }

  if ((slot.appRxQueue != nullptr) && slot.appRxQueue->IsCreated()) {
    slot.appRxQueue->Clear();
  }

  slot.txQueue->Clear();
  slot.txBuffers->Clear();
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.rxLastPos = 0U;
  (void)memset(slot.rxDmaBuffer, 0, slot.rxDmaBufferSize);

  slot.initialized.store(StartRx(slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }

  return slot.initialized.load(std::memory_order_acquire);
}

void UartDmaService::DeinitPort(UartPortId port) {
  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.initialized.store(false, std::memory_order_release);
  UART_HandleTypeDef *huart = UartHandle(slot.huart);
  if (huart != nullptr) {
    (void)HAL_UART_AbortReceive(huart);
  }

  slot.appRxQueue = nullptr;
  if (slot.txQueue != nullptr) {
    slot.txQueue->Clear();
  }
  if (slot.txBuffers != nullptr) {
    slot.txBuffers->Clear();
  }
  if ((slot.rxDmaBuffer != nullptr) && (slot.rxDmaBufferSize > 0U)) {
    (void)memset(slot.rxDmaBuffer, 0, slot.rxDmaBufferSize);
  }
  slot.txQueue = nullptr;
  slot.txBuffers = nullptr;
  slot.rxDmaBuffer = nullptr;
  slot.rxDmaBufferSize = 0U;
  slot.rxLastPos = 0U;
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

uint32_t UartDmaService::Write(UartPortId port, const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

  UartPortSlot &slot = Storage().slots[PortIndex(port)];
  if (!slot.initialized.load(std::memory_order_acquire) ||
      (slot.txQueue == nullptr) || !slot.txQueue->IsCreated()) {
    return 0U;
  }

  const uint32_t accepted = slot.txQueue->Enqueue(data, len);
  ServiceTxPath(slot);
  return accepted;
}

uint32_t UartDmaService::TxFree(UartPortId port) const {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  return ((slot.txQueue != nullptr) && slot.txQueue->IsCreated()) ? slot.txQueue->FreeSize() : 0U;
}

uint32_t UartDmaService::TxUsed(UartPortId port) const {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  return ((slot.txQueue != nullptr) && slot.txQueue->IsCreated()) ? slot.txQueue->UsedSize() : 0U;
}

uint32_t UartDmaService::RxDropped(UartPortId port) const {
  return Storage().slots[PortIndex(port)].rxDropped.load(std::memory_order_acquire);
}

bool UartDmaService::IsReady(UartPortId port) const {
  const UartPortSlot &slot = Storage().slots[PortIndex(port)];
  const UART_HandleTypeDef *huart = UartHandle(static_cast<const void *>(slot.huart));
  return slot.initialized.load(std::memory_order_acquire) && (huart != nullptr) &&
         (huart->Instance != nullptr) &&
         (huart->hdmarx != nullptr) &&
         (huart->hdmatx != nullptr);
}

void UartDmaService::OnRxEvent(void *huart, uint16_t size) {
  UartPortSlot *slot = FindSlot(huart);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  ProcessRxDelta(*slot, size);
}

void UartDmaService::OnTxComplete(void *huart) {
  UartPortSlot *slot = FindSlot(huart);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  slot->txBusy.store(false, std::memory_order_release);
  if (slot->txBuffers != nullptr) {
    slot->txBuffers->ClearActive();
  }
  ServiceTxPath(*slot);
}

void UartDmaService::OnError(void *huart) {
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

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  iFly::UartDmaService::Instance().OnRxEvent(static_cast<void *>(huart), Size);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  iFly::UartDmaService::Instance().OnTxComplete(static_cast<void *>(huart));
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  iFly::UartDmaService::Instance().OnError(static_cast<void *>(huart));
}
