#include "can.hpp"

#include <atomic>
#include <string.h>

#include "double_buffer.hpp"

namespace {

constexpr uint8_t PortIndex(iFly::CanPortId port) noexcept {
  return static_cast<uint8_t>(port);
}

class CanTxDoubleBuffer final : public iFly::StaticObjectDoubleBuffer<iFly::CanFramePacket> {
public:
  using iFly::StaticObjectDoubleBuffer<iFly::CanFramePacket>::StaticObjectDoubleBuffer;

  iFly::CanFramePacket &ActivePacket() noexcept {
    return ActiveObject();
  }

  iFly::CanFramePacket &InactivePacket() noexcept {
    return InactiveObject();
  }

  const iFly::CanFramePacket &InactivePacket() const noexcept {
    return InactiveObject();
  }

  void SetInactivePacket(const iFly::CanFramePacket &packet) noexcept {
    SetInactiveObject(packet);
  }
};

class CanRxRingBuffer final {
public:
  static constexpr uint8_t kStorageFrameCount = iFly::CanService::kFixedRxRingFrameCount + 1U;

  CanRxRingBuffer() noexcept = default;

  void Clear() noexcept {
    head_.store(0U, std::memory_order_relaxed);
    tail_.store(0U, std::memory_order_relaxed);
    for (uint8_t index = 0U; index < kStorageFrameCount; ++index) {
      storage_[index] = iFly::CanFramePacket {};
    }
  }

  bool Push(const iFly::CanFramePacket &packet) noexcept {
    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t nextHead = (head + 1U) % kStorageFrameCount;
    if (nextHead == tail) {
      return false;
    }

    storage_[head] = packet;
    head_.store(nextHead, std::memory_order_release);
    return true;
  }

  bool Pop(iFly::CanFramePacket *packet) noexcept {
    if (packet == nullptr) {
      return false;
    }

    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    const uint32_t head = head_.load(std::memory_order_acquire);
    if (tail == head) {
      return false;
    }

    *packet = storage_[tail];
    tail_.store((tail + 1U) % kStorageFrameCount, std::memory_order_release);
    return true;
  }

  uint32_t UsedCount() const noexcept {
    const uint32_t head = head_.load(std::memory_order_acquire);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    if (head >= tail) {
      return head - tail;
    }

    return kStorageFrameCount - (tail - head);
  }

private:
  iFly::CanFramePacket storage_[kStorageFrameCount] {};
  std::atomic<uint32_t> head_ {0U};
  std::atomic<uint32_t> tail_ {0U};
};

struct CanPortSlot final {
  CAN_HandleTypeDef *hcan = nullptr;
  iFly::LockFreeQueueBase *appRxQueue = nullptr;
  iFly::StaticLockFreeQueue<iFly::CanService::kFixedTxQueueStorageSize> txQueue {};
  CanTxDoubleBuffer txBuffers {};
  CanRxRingBuffer rxRing {};
  std::atomic<uint32_t> rxDropped {0U};
  std::atomic<bool> initialized {false};
  std::atomic<bool> txBusy {false};
  std::atomic<uint32_t> txServiceRequests {0U};
  std::atomic<bool> txServiceRunning {false};
};

class CanServiceStorage final {
public:
  CanPortSlot slots[iFly::CanService::kMaxPorts] {};
};

CanServiceStorage &Storage() noexcept {
  static CanServiceStorage storage;
  return storage;
}

CAN_HandleTypeDef *DefaultHandleForPort(iFly::CanPortId port) noexcept {
  switch (port) {
    case iFly::CanPortId::kCan1:
      return &hcan1;
    case iFly::CanPortId::kCan2:
    case iFly::CanPortId::kCount:
    default:
      return nullptr;
  }
}

CanPortSlot *FindSlot(CAN_HandleTypeDef *hcan) noexcept {
  if (hcan == nullptr) {
    return nullptr;
  }

  for (uint8_t index = 0U; index < iFly::CanService::kMaxPorts; ++index) {
    CanPortSlot &slot = Storage().slots[index];
    if (slot.hcan == hcan) {
      return &slot;
    }
  }

  return nullptr;
}

iFly::CanPortId FindPortId(const CanPortSlot *target) noexcept {
  for (uint8_t index = 0U; index < iFly::CanService::kMaxPorts; ++index) {
    if (&Storage().slots[index] == target) {
      return static_cast<iFly::CanPortId>(index);
    }
  }

  return iFly::CanPortId::kCount;
}

uint32_t FilterBankForPort(iFly::CanPortId port) noexcept {
  return (port == iFly::CanPortId::kCan2) ? 14U : 0U;
}

bool StartPort(iFly::CanPortId port, CanPortSlot &slot) noexcept {
  if (slot.hcan == nullptr) {
    return false;
  }

  (void)HAL_CAN_Stop(slot.hcan);

  CAN_FilterTypeDef filter {};
  filter.FilterBank = FilterBankForPort(port);
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0U;
  filter.FilterIdLow = 0U;
  filter.FilterMaskIdHigh = 0U;
  filter.FilterMaskIdLow = 0U;
  filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter.FilterActivation = CAN_FILTER_ENABLE;
  filter.SlaveStartFilterBank = 14U;

  if (HAL_CAN_ConfigFilter(slot.hcan, &filter) != HAL_OK) {
    return false;
  }

  if (HAL_CAN_Start(slot.hcan) != HAL_OK) {
    return false;
  }

  constexpr uint32_t kNotifications =
      CAN_IT_TX_MAILBOX_EMPTY |
      CAN_IT_RX_FIFO0_MSG_PENDING |
      CAN_IT_RX_FIFO0_FULL |
      CAN_IT_RX_FIFO0_OVERRUN |
      CAN_IT_RX_FIFO1_MSG_PENDING |
      CAN_IT_RX_FIFO1_FULL |
      CAN_IT_RX_FIFO1_OVERRUN |
      CAN_IT_ERROR |
      CAN_IT_BUSOFF |
      CAN_IT_ERROR_WARNING |
      CAN_IT_ERROR_PASSIVE |
      CAN_IT_LAST_ERROR_CODE;

  return HAL_CAN_ActivateNotification(slot.hcan, kNotifications) == HAL_OK;
}

void FillTxHeader(const iFly::CanFramePacket &packet, CAN_TxHeaderTypeDef *header) noexcept {
  if (header == nullptr) {
    return;
  }

  const bool isExtendedId = (packet.flags & iFly::kCanFrameFlagExtendedId) != 0U;
  header->IDE = isExtendedId ? CAN_ID_EXT : CAN_ID_STD;
  header->RTR = ((packet.flags & iFly::kCanFrameFlagRemoteFrame) != 0U) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  header->DLC = (packet.dlc <= 8U) ? packet.dlc : 8U;
  header->TransmitGlobalTime = DISABLE;
  header->StdId = isExtendedId ? 0U : (packet.id & 0x7FFU);
  header->ExtId = isExtendedId ? (packet.id & 0x1FFFFFFFU) : 0U;
}

iFly::CanFramePacket BuildRxPacket(const CAN_RxHeaderTypeDef &header,
                                   const uint8_t *data,
                                   uint32_t fifo) noexcept {
  iFly::CanFramePacket packet {};
  packet.id = (header.IDE == CAN_ID_EXT) ? header.ExtId : header.StdId;
  packet.dlc = (header.DLC <= 8U) ? static_cast<uint8_t>(header.DLC) : 8U;
  packet.flags = 0U;
  if (header.IDE == CAN_ID_EXT) {
    packet.flags |= iFly::kCanFrameFlagExtendedId;
  }
  if (header.RTR == CAN_RTR_REMOTE) {
    packet.flags |= iFly::kCanFrameFlagRemoteFrame;
  }
  if (fifo == CAN_RX_FIFO1) {
    packet.flags |= iFly::kCanFrameFlagRxFifo1;
  }
  packet.filterIndex = static_cast<uint8_t>(header.FilterMatchIndex);
  packet.reserved = 0U;
  if (data != nullptr) {
    (void)memcpy(packet.data, data, sizeof(packet.data));
  }
  return packet;
}

uint32_t LoadTxPacketToInactiveBuffer(CanPortSlot &slot) noexcept {
  if (!slot.txQueue.IsCreated() || slot.txBuffers.HasInactiveData()) {
    return 0U;
  }

  if (slot.txQueue.UsedSize() < iFly::CanService::kCanFramePacketSize) {
    return 0U;
  }

  iFly::CanFramePacket packet {};
  const uint32_t pulled = slot.txQueue.Dequeue(reinterpret_cast<uint8_t *>(&packet),
                                               iFly::CanService::kCanFramePacketSize);
  if (pulled == iFly::CanService::kCanFramePacketSize) {
    slot.txBuffers.SetInactivePacket(packet);
    return pulled;
  }

  return 0U;
}

void TryStartTx(CanPortSlot &slot) noexcept {
  if ((slot.hcan == nullptr) ||
      !slot.initialized.load(std::memory_order_acquire) ||
      slot.txBusy.load(std::memory_order_acquire) ||
      !slot.txBuffers.HasInactiveData()) {
    return;
  }

  if (HAL_CAN_GetTxMailboxesFreeLevel(slot.hcan) == 0U) {
    return;
  }

  bool expectedBusy = false;
  if (!slot.txBusy.compare_exchange_strong(expectedBusy, true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    return;
  }

  CAN_TxHeaderTypeDef header {};
  FillTxHeader(slot.txBuffers.InactivePacket(), &header);

  uint32_t txMailbox = 0U;
  if (HAL_CAN_AddTxMessage(slot.hcan,
                           &header,
                           slot.txBuffers.InactivePacket().data,
                           &txMailbox) == HAL_OK) {
    slot.txBuffers.SwapBuffers();
    (void)LoadTxPacketToInactiveBuffer(slot);
    return;
  }

  slot.txBusy.store(false, std::memory_order_release);
}

void ServiceTxPathOnce(CanPortSlot &slot) noexcept {
  (void)LoadTxPacketToInactiveBuffer(slot);
  TryStartTx(slot);
}

void ServiceTxPath(CanPortSlot &slot) noexcept {
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

void DrainRxRingToAppQueue(CanPortSlot &slot) noexcept {
  if ((slot.appRxQueue == nullptr) || !slot.appRxQueue->IsCreated()) {
    return;
  }

  while (slot.appRxQueue->FreeSize() >= iFly::CanService::kCanFramePacketSize) {
    iFly::CanFramePacket packet {};
    if (!slot.rxRing.Pop(&packet)) {
      return;
    }

    const uint32_t pushed = slot.appRxQueue->Enqueue(reinterpret_cast<const uint8_t *>(&packet),
                                                     iFly::CanService::kCanFramePacketSize);
    if (pushed != iFly::CanService::kCanFramePacketSize) {
      (void)slot.rxDropped.fetch_add(iFly::CanService::kCanFramePacketSize - pushed,
                                     std::memory_order_relaxed);
      return;
    }
  }
}

void DrainHardwareRxFifo(CanPortSlot &slot, uint32_t fifo) noexcept {
  while (HAL_CAN_GetRxFifoFillLevel(slot.hcan, fifo) > 0U) {
    CAN_RxHeaderTypeDef header {};
    uint8_t data[8] {};
    if (HAL_CAN_GetRxMessage(slot.hcan, fifo, &header, data) != HAL_OK) {
      return;
    }

    const iFly::CanFramePacket packet = BuildRxPacket(header, data, fifo);
    if (!slot.rxRing.Push(packet)) {
      (void)slot.rxDropped.fetch_add(iFly::CanService::kCanFramePacketSize,
                                     std::memory_order_relaxed);
      return;
    }
  }
}

void HandleTxFinished(CanPortSlot &slot) noexcept {
  slot.txBusy.store(false, std::memory_order_release);
  slot.txBuffers.ClearActive();
  ServiceTxPath(slot);
}

void HandleError(iFly::CanPortId port, CanPortSlot &slot) noexcept {
  slot.txBusy.store(false, std::memory_order_release);
  slot.txBuffers.ClearActive();
  slot.initialized.store(StartPort(port, slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }
}

} // namespace

namespace iFly {

const char *ToString(CanPortId port) noexcept {
  switch (port) {
    case CanPortId::kCan1:
      return "CAN1";
    case CanPortId::kCan2:
      return "CAN2";
    case CanPortId::kCount:
    default:
      return "CAN";
  }
}

CanService &CanService::Instance() noexcept {
  static CanService instance;
  return instance;
}

void CanService::AttachHardware(CanPortId port, CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.hcan = hcan;
  slot.initialized.store(false, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

bool CanService::InitPort(CanPortId port, LockFreeQueueBase *rxQueue) noexcept {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  if (slot.hcan == nullptr) {
    slot.hcan = DefaultHandleForPort(port);
  }

  slot.appRxQueue = rxQueue;
  if ((slot.appRxQueue != nullptr) && slot.appRxQueue->IsCreated()) {
    slot.appRxQueue->Clear();
  }

  slot.txQueue.Recreate();
  slot.txQueue.Clear();
  slot.txBuffers.Recreate();
  slot.rxRing.Clear();
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);

  slot.initialized.store(StartPort(port, slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    DrainRxRingToAppQueue(slot);
    ServiceTxPath(slot);
  }

  return slot.initialized.load(std::memory_order_acquire);
}

void CanService::DeinitPort(CanPortId port) noexcept {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.initialized.store(false, std::memory_order_release);
  if (slot.hcan != nullptr) {
    (void)HAL_CAN_Stop(slot.hcan);
  }

  slot.appRxQueue = nullptr;
  slot.txQueue.Recreate();
  slot.txQueue.Clear();
  slot.txBuffers.Clear();
  slot.rxRing.Clear();
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txBusy.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

uint32_t CanService::Write(CanPortId port, const uint8_t *data, uint32_t len) noexcept {
  if ((data == nullptr) || (len < kCanFramePacketSize)) {
    return 0U;
  }

  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  if (!slot.initialized.load(std::memory_order_acquire) || !slot.txQueue.IsCreated()) {
    return 0U;
  }

  const uint32_t frameCountRequested = len / kCanFramePacketSize;
  const uint32_t frameCountFree = slot.txQueue.FreeSize() / kCanFramePacketSize;
  const uint32_t frameCountAccepted =
      (frameCountRequested < frameCountFree) ? frameCountRequested : frameCountFree;
  const uint32_t acceptedBytes = frameCountAccepted * kCanFramePacketSize;
  if (acceptedBytes == 0U) {
    return 0U;
  }

  const uint32_t queued = slot.txQueue.Enqueue(data, acceptedBytes);
  ServiceTxPath(slot);
  return queued - (queued % kCanFramePacketSize);
}

bool CanService::WriteFrame(CanPortId port, const CanFramePacket &frame) noexcept {
  return Write(port,
               reinterpret_cast<const uint8_t *>(&frame),
               sizeof(frame)) == sizeof(frame);
}

uint32_t CanService::TxFree(CanPortId port) const noexcept {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.FreeSize() : 0U;
}

uint32_t CanService::TxUsed(CanPortId port) const noexcept {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.UsedSize() : 0U;
}

uint32_t CanService::RxDropped(CanPortId port) const noexcept {
  return Storage().slots[PortIndex(port)].rxDropped.load(std::memory_order_acquire);
}

bool CanService::IsReady(CanPortId port) const noexcept {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.initialized.load(std::memory_order_acquire) && (slot.hcan != nullptr);
}

void CanService::ServiceRxPath(CanPortId port) noexcept {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  DrainRxRingToAppQueue(slot);
}

void CanService::OnRxFifoPending(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire) || (slot->hcan == nullptr)) {
    return;
  }

  DrainHardwareRxFifo(*slot, fifo);
}

void CanService::OnRxFifoFull(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept {
  OnRxFifoPending(hcan, fifo);
}

void CanService::OnTxComplete(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

void CanService::OnTxAbort(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

void CanService::OnError(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || (slot->hcan == nullptr)) {
    return;
  }

  HandleError(FindPortId(slot), *slot);
}

} // namespace iFly

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoPending(hcan, CAN_RX_FIFO0);
}

extern "C" void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoFull(hcan, CAN_RX_FIFO0);
}

extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoPending(hcan, CAN_RX_FIFO1);
}

extern "C" void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoFull(hcan, CAN_RX_FIFO1);
}

extern "C" void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(hcan);
}

extern "C" void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(hcan);
}

extern "C" void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(hcan);
}

extern "C" void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(hcan);
}

extern "C" void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(hcan);
}

extern "C" void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(hcan);
}

extern "C" void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnError(hcan);
}
