#include "can.hpp"

#include <atomic>
#include <string.h>

#include "double_buffer.hpp"

namespace {

// 把枚举端口号转成数组下标，便于访问 Storage().slots[]。
constexpr uint8_t PortIndex(iFly::CanPortId port) noexcept {
  return static_cast<uint8_t>(port);
}

// CAN 发送用的双缓冲。
//
// 这里继承公共双缓冲基类，只是为了让语义更清晰：
// 1. inactive buffer：准备发送的“下一帧”
// 2. active buffer：已经交给 HAL，正在等待邮箱发送完成的“当前帧”
//
// 这样设计的好处是：
// 1. 上层线程只负责把完整帧放进无锁队列
// 2. 底层服务逻辑从队列取一帧，先装进 inactive
// 3. 硬件空闲时再一次性交给 HAL 发送，然后交换 active/inactive
//
// 也就是说，双缓冲负责“贴近硬件的最后一级缓存”，
// 无锁队列负责“上层和驱动层之间的解耦”。
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

// Runtime state for one CAN port.
struct CanPortSlot final {
  CAN_HandleTypeDef *hcan = nullptr;
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

// 全局静态存储，整个工程只保留一份 CAN 服务状态。
CanServiceStorage &Storage() noexcept {
  static CanServiceStorage storage;
  return storage;
}

// 如果上层没有主动 AttachHardware()，这里提供默认句柄映射。
// 当前工程只明确支持 2 路软件端口，但底层硬件句柄是否真的存在，
// 仍要看芯片、CubeMX 配置以及链接进来的全局 hcan 变量。
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

// 根据 HAL 回调给出的 hcan，反查它属于哪一个软件端口 slot。
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

// 反查 slot 对应的逻辑端口号，错误时返回 kCount。
iFly::CanPortId FindPortId(const CanPortSlot *target) noexcept {
  for (uint8_t index = 0U; index < iFly::CanService::kMaxPorts; ++index) {
    if (&Storage().slots[index] == target) {
      return static_cast<iFly::CanPortId>(index);
    }
  }

  return iFly::CanPortId::kCount;
}

// 双 CAN 控制器时，滤波器 bank 需要分配。
// 这里给 CAN2 预留后半段 bank，避免和 CAN1 冲突。
uint32_t FilterBankForPort(iFly::CanPortId port) noexcept {
  return (port == iFly::CanPortId::kCan2) ? 14U : 0U;
}

void ConfigureTransmitRequestOrder(CanPortSlot &slot) noexcept {
  if (slot.hcan == nullptr) {
    return;
  }

  // 打开 TXFP，让 bxCAN 按软件提交顺序发，而不是按报文优先级重排。
  // 这样才能在吃满 3 个邮箱的同时，保持回环和业务观察到的帧序稳定。
  slot.hcan->Init.TransmitFifoPriority = ENABLE;
  SET_BIT(slot.hcan->Instance->MCR, CAN_MCR_TXFP);
}

// 启动一口 CAN。
//
// 这里做的事情有：
// 1. 先停 CAN，避免反复初始化时状态混乱
// 2. 配置滤波器
// 3. 启动 CAN 外设
// 4. 打开发送、接收、错误相关中断通知
//
// 当前滤波器配置为“全接收”：
// mask 全 0，表示不过滤 ID，所有报文都进 FIFO0。
// 这对调试最友好；后续如果需要按 ID 分流，再在这里细化。
bool StartPort(iFly::CanPortId port, CanPortSlot &slot) noexcept {
  if (slot.hcan == nullptr) {
    return false;
  }

  (void)HAL_CAN_Stop(slot.hcan);
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

// 把软件层的 CanFramePacket 转成 HAL 发送头。
//
// CAN 和 UART 最大的区别之一是：
// UART 是字节流；
// CAN 是“按帧发送”的，每一帧都有独立的 ID、长度、类型。
//
// 这里主要处理几个核心字段：
// 1. IDE：标准帧 ID(11 位) 还是扩展帧 ID(29 位)
// 2. RTR：数据帧还是远程帧
// 3. DLC：本帧有效载荷长度，经典 CAN 最大 8 字节
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

// 把 HAL 收到的 RX 头和数据，重新封装为统一的 CanFramePacket。
//
// 这样做的意义是：
// 1. 上层不需要依赖 HAL 的结构体
// 2. SerialIoBase / 无锁队列 只需要处理固定大小的 16 字节对象
// 3. 后续写日志、转发、缓存时都能复用同一种格式
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

// 从“上层发送无锁队列”取出 1 帧，装进双缓冲的 inactive 槽位。
//
// 只有 inactive 为空时才会继续装，避免覆盖还没送去硬件的候选帧。
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

// 尝试真正启动一次硬件发送。
//
// 注意这里不是“把整个队列一次发完”，而是只负责：
// 1. 看当前是否满足启动条件
// 2. 如果满足，就把 inactive 这一帧交给 HAL
// 3. 发送成功后交换双缓冲，并顺手预取下一帧
//
// 真正连续发多帧，是靠：
// - 每次发送完成回调 OnTxComplete()
// - 回调里再次调用 ServiceTxPath()
// 一帧一帧往前推进。
bool PromoteInactiveToActive(CanPortSlot &slot) noexcept {
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

bool TryQueueOneTxPacket(CanPortSlot &slot) noexcept {
  if ((slot.hcan == nullptr) ||
      !slot.initialized.load(std::memory_order_acquire) ||
      (HAL_CAN_GetTxMailboxesFreeLevel(slot.hcan) == 0U)) {
    return false;
  }

  if (!PromoteInactiveToActive(slot)) {
    return false;
  }

  CAN_TxHeaderTypeDef header {};
  FillTxHeader(slot.txBuffers.ActivePacket(), &header);

  uint32_t txMailbox = 0U;
  if (HAL_CAN_AddTxMessage(slot.hcan,
                           &header,
                           slot.txBuffers.ActivePacket().data,
                           &txMailbox) != HAL_OK) {
    return false;
  }

  slot.txBuffers.ClearActive();
  (void)LoadTxPacketToInactiveBuffer(slot);
  return true;
}

// 执行一轮发送服务：
// 1. 先尝试把队列中的下一帧搬到 inactive
// 2. 再尝试启动硬件发送
void ServiceTxPathOnce(CanPortSlot &slot) noexcept {
  (void)LoadTxPacketToInactiveBuffer(slot);
  while (TryQueueOneTxPacket(slot)) {
  }
}

// 发送通路的统一入口。
//
// 为什么这里还要加 txServiceRequests / txServiceRunning？
// 因为 ServiceTxPath() 可能被多个时机触发：
// 1. 上层刚写入了新帧
// 2. 某个邮箱刚发送完成
// 3. 错误恢复后需要重启发送
//
// 我们希望它“可重入但不乱跑”，于是做成：
// 1. 先记一次 service request
// 2. 如果已经有人在服务，就直接返回
// 3. 当前线程负责把这几次 request 合并处理掉
//
// 这是一种轻量级的“合并唤醒”写法，避免重复进入复杂发送流程。
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

// Push one received CAN frame directly into the upper RX queue.
bool PushRxPacketToAppQueue(CanPortSlot &slot, const iFly::CanFramePacket &packet) noexcept {
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

// Drain the HAL RX FIFO and forward frames straight into the upper RX queue.
void DrainHardwareRxFifoToAppQueue(CanPortSlot &slot, uint32_t fifo) noexcept {
  while (HAL_CAN_GetRxFifoFillLevel(slot.hcan, fifo) > 0U) {
    CAN_RxHeaderTypeDef header {};
    uint8_t data[8] {};
    if (HAL_CAN_GetRxMessage(slot.hcan, fifo, &header, data) != HAL_OK) {
      return;
    }

    const iFly::CanFramePacket packet = BuildRxPacket(header, data, fifo);
    (void)PushRxPacketToAppQueue(slot, packet);
  }
}

// 一个邮箱发送结束或发送被终止后，统一走这里收尾。
// 清掉 busy 标志，释放 active 槽位，并继续推进下一帧。
void HandleTxFinished(CanPortSlot &slot) noexcept {
  ServiceTxPath(slot);
}

// 错误处理策略：
// 1. 先清掉当前发送状态
// 2. 重新启动该 CAN 口
// 3. 如果恢复成功，再继续发送队列里的剩余帧
//
// 这是“尽量自恢复”的策略，适合嵌入式长期运行场景。
void HandleError(iFly::CanPortId port, CanPortSlot &slot) noexcept {
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

// 允许外部把某个逻辑端口和具体 HAL 句柄绑定起来。
// 如果不调用，InitPort() 会尝试使用 DefaultHandleForPort()。
void CanService::AttachHardware(CanPortId port, CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot &slot = Storage().slots[PortIndex(port)];
  slot.hcan = hcan;
  slot.initialized.store(false, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

// Initialize one CAN port.
//
// RX is HAL FIFO -> upper RX queue.
// TX stays txQueue -> double buffer -> HAL mailbox.
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
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);

  slot.initialized.store(StartPort(port, slot), std::memory_order_release);
  if (slot.initialized.load(std::memory_order_acquire)) {
    ServiceTxPath(slot);
  }

  return slot.initialized.load(std::memory_order_acquire);
}

// 反初始化一路 CAN。
// 注意这里只清理软件状态，不会销毁全局 HAL 句柄对象本身。
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
  slot.rxDropped.store(0U, std::memory_order_release);
  slot.txServiceRequests.store(0U, std::memory_order_release);
  slot.txServiceRunning.store(false, std::memory_order_release);
}

// 按“固定帧封包格式”写入发送队列。
//
// 这里虽然接口长得像串口 Write(data, len)，
// 但 CAN 不是字节流，所以 len 必须是 CanFramePacket 的整数倍。
// 多出来的不完整字节不会发送。
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

// 写单帧的便捷接口，业务层更常用这个。
bool CanService::WriteFrame(CanPortId port, const CanFramePacket &frame) noexcept {
  return Write(port,
               reinterpret_cast<const uint8_t *>(&frame),
               sizeof(frame)) == sizeof(frame);
}

// 以下几个接口主要用于查询状态，便于上层做流控或调试。
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

// Kept for interface compatibility. RX frames are already queued in HAL callbacks.
void CanService::ServiceRxPath(CanPortId port) noexcept {
  (void)port;
}

// Called from HAL when RX FIFO has pending frames.
void CanService::OnRxFifoPending(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire) || (slot->hcan == nullptr)) {
    return;
  }

  DrainHardwareRxFifoToAppQueue(*slot, fifo);
}

// FIFO 满了时也一样先尽快取走。
void CanService::OnRxFifoFull(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept {
  OnRxFifoPending(hcan, fifo);
}

// 任意发送邮箱完成时，统一推进下一帧。
void CanService::OnTxComplete(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

// 邮箱发送被终止时，处理方式和发送完成一致：
// 释放当前 active，再看后续是否需要继续发。
void CanService::OnTxAbort(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || !slot->initialized.load(std::memory_order_acquire)) {
    return;
  }

  HandleTxFinished(*slot);
}

// 统一错误入口，由 HAL 错误回调调用。
void CanService::OnError(CAN_HandleTypeDef *hcan) noexcept {
  CanPortSlot *slot = FindSlot(hcan);
  if ((slot == nullptr) || (slot->hcan == nullptr)) {
    return;
  }

  HandleError(FindPortId(slot), *slot);
}

} // namespace iFly

// 下面这些 extern "C" 回调是 HAL 规定的固定名字。
// 它们的作用很简单：把 C 风格回调转发给 C++ 的 CanService 单例。

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
