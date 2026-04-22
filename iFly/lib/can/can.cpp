#include "can.hpp"

#include <atomic>
#include <string.h>

#include "double_buffer.hpp"
#include "platform_handle.hpp"
#include "usermath.hpp"

namespace {

CAN_HandleTypeDef *CanHandle(void *handle) {
  return iFly::platform::AsCanHandle(handle);
}

const CAN_HandleTypeDef *CanHandle(const void *handle) {
  return iFly::platform::AsCanHandle(handle);
}

constexpr uint8_t PortIndex(iFly::CanPortId port) {
  return static_cast<uint8_t>(port);
}

class CanTxDoubleBuffer final : public iFly::StaticObjectDoubleBuffer<iFly::CanFramePacket> {
public:
  using iFly::StaticObjectDoubleBuffer<iFly::CanFramePacket>::StaticObjectDoubleBuffer;

  iFly::CanFramePacket &ActivePacket() {
    return ActiveObject();
  }

  iFly::CanFramePacket &InactivePacket() {
    return InactiveObject();
  }

  const iFly::CanFramePacket &InactivePacket() const {
    return InactiveObject();
  }

  void SetInactivePacket(const iFly::CanFramePacket &packet) {
    SetInactiveObject(packet);
  }
};

struct CanPortSlot final {
  void *hcan = nullptr;
  iFly::LockFreeQueueBase *appRxQueue = nullptr;
  iFly::StaticLockFreeQueue<iFly::CanService::kFixedTxQueueStorageSize> txQueue {};
  CanTxDoubleBuffer txBuffers {};
  std::atomic<uint32_t> rxDropped {0U};
  std::atomic<bool> initialized {false};
  std::atomic<uint32_t> txServiceRequests {0U};
  std::atomic<bool> txServiceRunning {false};
};

class CanServiceStorage final {
public:
  CanPortSlot slots[iFly::CanService::kMaxPorts] {};
};

CanServiceStorage &Storage() {
  static CanServiceStorage storage;
  return storage;
}

void *DefaultHandleForPort(iFly::CanPortId port) {
  switch (port) {
    case iFly::CanPortId::kCan1:
      return &hcan1;
    case iFly::CanPortId::kCan2:
    case iFly::CanPortId::kCount:
    default:
      return nullptr;
  }
}

CanPortSlot *FindSlot(void *hcan) {
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

iFly::CanPortId FindPortId(const CanPortSlot *target) {
  for (uint8_t index = 0U; index < iFly::CanService::kMaxPorts; ++index) {
    if (&Storage().slots[index] == target) {
      return static_cast<iFly::CanPortId>(index);
    }
  }

  return iFly::CanPortId::kCount;
}

uint32_t FilterBankForPort(iFly::CanPortId port) {
  return (port == iFly::CanPortId::kCan2) ? 14U : 0U;
}

void ConfigureTransmitRequestOrder(CanPortSlot &slot) {
  CAN_HandleTypeDef *hcan = CanHandle(slot.hcan);
  if (hcan == nullptr) {
    return;
  }

  // 打开 TXFP，让 bxCAN 按软件提交顺序发，而不是按报文优先级重排。
  // 这样才能在吃满 3 个邮箱的同时，保持回环和业务观察到的帧序稳定。
  hcan->Init.TransmitFifoPriority = ENABLE;
  SET_BIT(hcan->Instance->MCR, CAN_MCR_TXFP);
}

bool StartPort(iFly::CanPortId port, CanPortSlot &slot) {
  CAN_HandleTypeDef *hcan = CanHandle(slot.hcan);
  if (hcan == nullptr) {
    return false;
  }

  (void)HAL_CAN_Stop(hcan);
  ConfigureTransmitRequestOrder(slot);

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

  if (HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK) {
    return false;
  }

  if (HAL_CAN_Start(hcan) != HAL_OK) {
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

  return HAL_CAN_ActivateNotification(hcan, kNotifications) == HAL_OK;
}

void FillTxHeader(const iFly::CanFramePacket &packet, CAN_TxHeaderTypeDef *header) {
  if (header == nullptr) {
    return;
  }

  const bool isExtendedId = (packet.flags & iFly::kCanFrameFlagExtendedId) != 0U;
  header->IDE = isExtendedId ? CAN_ID_EXT : CAN_ID_STD;
  header->RTR = ((packet.flags & iFly::kCanFrameFlagRemoteFrame) != 0U) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  header->DLC = iFly::usermath::Min<uint8_t>(packet.dlc, 8U);
  header->TransmitGlobalTime = DISABLE;
  header->StdId = isExtendedId ? 0U : (packet.id & 0x7FFU);
  header->ExtId = isExtendedId ? (packet.id & 0x1FFFFFFFU) : 0U;
}

iFly::CanFramePacket BuildRxPacket(const CAN_RxHeaderTypeDef &header,
                                   const uint8_t *data,
                                   uint32_t fifo) {
  iFly::CanFramePacket packet {};
  packet.id = (header.IDE == CAN_ID_EXT) ? header.ExtId : header.StdId;
  packet.dlc = iFly::usermath::Min<uint8_t>(static_cast<uint8_t>(header.DLC), 8U);
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

uint32_t LoadTxPacketToInactiveBuffer(CanPortSlot &slot) {
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

bool PromoteInactiveToActive(CanPortSlot &slot) {
  if (slot.txBuffers.HasActiveData()) {
    return true;
  }

  if (!slot.txBuffers.HasInactiveData()) {
    (void)LoadTxPacketToInactiveBuffer(slot);
  }

  if (!slot.txBuffers.HasInactiveData()) {
    return false;
  }

  slot.txBuffers.SwapBuffers();
  return true;
}

bool TryQueueOneTxPacket(CanPortSlot &slot) {
  CAN_HandleTypeDef *hcan = CanHandle(slot.hcan);
  if ((hcan == nullptr) ||
      !slot.initialized.load(std::memory_order_acquire) ||
      (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U)) {
    return false;
  }

  if (!PromoteInactiveToActive(slot)) {
    return false;
  }

  CAN_TxHeaderTypeDef header {};
  FillTxHeader(slot.txBuffers.ActivePacket(), &header);

  uint32_t txMailbox = 0U;
  if (HAL_CAN_AddTxMessage(hcan,
                           &header,
                           slot.txBuffers.ActivePacket().data,
                           &txMailbox) != HAL_OK) {
    return false;
  }

  slot.txBuffers.ClearActive();
  (void)LoadTxPacketToInactiveBuffer(slot);
  return true;
}

void ServiceTxPathOnce(CanPortSlot &slot) {
  (void)LoadTxPacketToInactiveBuffer(slot);
  while (TryQueueOneTxPacket(slot)) {
  }
}

void ServiceTxPath(CanPortSlot &slot) {
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

bool PushRxPacketToAppQueue(CanPortSlot &slot, const iFly::CanFramePacket &packet) {
  if ((slot.appRxQueue == nullptr) || !slot.appRxQueue->IsCreated()) {
    (void)slot.rxDropped.fetch_add(iFly::CanService::kCanFramePacketSize,
                                   std::memory_order_relaxed);
    return false;
  }

  if (slot.appRxQueue->FreeSize() < iFly::CanService::kCanFramePacketSize) {
    (void)slot.rxDropped.fetch_add(iFly::CanService::kCanFramePacketSize,
                                   std::memory_order_relaxed);
    return false;
  }

  const uint32_t pushed = slot.appRxQueue->Enqueue(reinterpret_cast<const uint8_t *>(&packet),
                                                   iFly::CanService::kCanFramePacketSize);
  if (pushed == iFly::CanService::kCanFramePacketSize) {
    return true;
  }

  (void)slot.rxDropped.fetch_add(iFly::CanService::kCanFramePacketSize - pushed,
                                 std::memory_order_relaxed);
  return false;
}

void DrainHardwareRxFifoToAppQueue(CanPortSlot &slot, uint32_t fifo) {
  CAN_HandleTypeDef *hcan = CanHandle(slot.hcan);
  if (hcan == nullptr) {
    return;
  }

  while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) > 0U) {
    CAN_RxHeaderTypeDef header {};
    uint8_t data[8] {};
    if (HAL_CAN_GetRxMessage(hcan, fifo, &header, data) != HAL_OK) {
      return;
    }

    const iFly::CanFramePacket packet = BuildRxPacket(header, data, fifo);
    (void)PushRxPacketToAppQueue(slot, packet);
  }
}

void HandleTxFinished(CanPortSlot &slot) {
  ServiceTxPath(slot);
}

void HandleError(iFly::CanPortId port, CanPortSlot &slot) {
  slot.initialized.store(StartPort(port, slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }
}

} // namespace

namespace iFly {

const char *ToString(CanPortId port) {
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

CanService &CanService::Instance() {
  static CanService instance;
  return instance;
}

void CanService::AttachHardware(CanPortId port, void *hcan) {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.hcan = hcan;
  slot.initialized.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

bool CanService::InitPort(CanPortId port, LockFreeQueueBase *rxQueue) {
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
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);

  slot.initialized.store(StartPort(port, slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }

  return slot.initialized.load(std::memory_order_acquire);
}

void CanService::DeinitPort(CanPortId port) {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.initialized.store(false, std::memory_order_release);
  CAN_HandleTypeDef *hcan = CanHandle(slot.hcan);
  if (hcan != nullptr) {
    (void)HAL_CAN_Stop(hcan);
  }

  slot.appRxQueue = nullptr;
  slot.txQueue.Recreate();
  slot.txQueue.Clear();
  slot.txBuffers.Clear();
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

uint32_t CanService::Write(CanPortId port, const uint8_t *data, uint32_t len) {
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
      iFly::usermath::Min<uint32_t>(frameCountRequested, frameCountFree);
  const uint32_t acceptedBytes = frameCountAccepted * kCanFramePacketSize;
  if (acceptedBytes == 0U) {
    return 0U;
  }

  const uint32_t queued = slot.txQueue.Enqueue(data, acceptedBytes);
  ServiceTxPath(slot);
  return queued - (queued % kCanFramePacketSize);
}

bool CanService::WriteFrame(CanPortId port, const CanFramePacket &frame) {
  return Write(port,
               reinterpret_cast<const uint8_t *>(&frame),
               sizeof(frame)) == sizeof(frame);
}

uint32_t CanService::TxFree(CanPortId port) const {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.FreeSize() : 0U;
}

uint32_t CanService::TxUsed(CanPortId port) const {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  return slot.txQueue.IsCreated() ? slot.txQueue.UsedSize() : 0U;
}

uint32_t CanService::RxDropped(CanPortId port) const {
  return Storage().slots[PortIndex(port)].rxDropped.load(std::memory_order_acquire);
}

bool CanService::IsReady(CanPortId port) const {
  const CanPortSlot &slot = Storage().slots[PortIndex(port)];
  const CAN_HandleTypeDef *hcan = CanHandle(static_cast<const void *>(slot.hcan));
  return slot.initialized.load(std::memory_order_acquire) && (hcan != nullptr) &&
         (hcan->Instance != nullptr);
}

void CanService::ServiceRxPath(CanPortId port) {
  (void)port;
}

void CanService::OnRxFifoPending(void *hcan, uint32_t fifo) {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire) || (slot->hcan == nullptr)) {
    return;
  }

  DrainHardwareRxFifoToAppQueue(*slot, fifo);
}

void CanService::OnRxFifoFull(void *hcan, uint32_t fifo) {
  OnRxFifoPending(hcan, fifo);
}

void CanService::OnTxComplete(void *hcan) {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

void CanService::OnTxAbort(void *hcan) {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

void CanService::OnError(void *hcan) {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || (slot->hcan == nullptr)) {
    return;
  }

  HandleError(FindPortId(slot), *slot);
}

} // namespace iFly

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoPending(static_cast<void *>(hcan), CAN_RX_FIFO0);
}

extern "C" void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoFull(static_cast<void *>(hcan), CAN_RX_FIFO0);
}

extern "C" void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoPending(static_cast<void *>(hcan), CAN_RX_FIFO1);
}

extern "C" void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnRxFifoFull(static_cast<void *>(hcan), CAN_RX_FIFO1);
}

extern "C" void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxComplete(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnTxAbort(static_cast<void *>(hcan));
}

extern "C" void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {
  iFly::CanService::Instance().OnError(static_cast<void *>(hcan));
}
