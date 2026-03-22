#ifndef IFLY_USB_CDC_HPP
#define IFLY_USB_CDC_HPP

#include <stddef.h>
#include <stdint.h>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 仅用于 USB 传输层内部的字节环形缓冲区。
 *
 * @details
 * - 该类位于 `usb pcd -> usb_cdc` 之间，承担“接收缓冲整形”职责。
 * - USB OUT 中断先把收到的数据压入这里，再由 `UsbCdcAcm` 转存到上层无锁队列。
 * - 底层缓冲区在创建阶段一次性动态申请，运行过程中不再申请内存。
 * - 该类本身不负责并发保护，调用方需要在中断/前后台切换处自行保证访问串行化。
 */
class ByteRingBuffer final {
public:
  ByteRingBuffer() noexcept = default;
  explicit ByteRingBuffer(uint32_t storageSize) noexcept;
  ~ByteRingBuffer();

  ByteRingBuffer(const ByteRingBuffer &) = delete;
  ByteRingBuffer &operator=(const ByteRingBuffer &) = delete;

  bool Recreate(uint32_t storageSize) noexcept;
  void Clear() noexcept;
  uint32_t Push(const uint8_t *data, uint32_t length) noexcept;
  uint32_t Pop(uint8_t *data, uint32_t length) noexcept;
  uint32_t Peek(uint8_t *data, uint32_t length) const noexcept;
  uint32_t Discard(uint32_t length) noexcept;

  uint32_t UsedSize() const noexcept;
  uint32_t FreeSize() const noexcept;
  uint32_t Capacity() const noexcept;
  bool IsCreated() const noexcept;
  bool IsEmpty() const noexcept;

private:
  static uint32_t MinU32(uint32_t left, uint32_t right) noexcept;
  static uint32_t Distance(uint32_t head, uint32_t tail, uint32_t size) noexcept;

private:
  uint8_t *storage_ = nullptr;
  uint32_t storageSize_ = 0U;
  uint32_t head_ = 0U;
  uint32_t tail_ = 0U;
};

/**
 * @brief 仅用于 USB IN 发送链路内部的双缓冲区。
 *
 * @details
 * - 该模块位于 `usb_cdc -> usb pcd` 之间。
 * - 上层字节流会先进入 `UsbCdcAcm` 内部发送队列，再被拆包装载到这里的两个物理槽位。
 * - 一个槽位交给 USB PCD 发送时，另一个槽位仍可继续预取下一包，从而减少空窗时间。
 * - 双缓冲存储空间在创建阶段一次性动态申请，运行中不再申请内存。
 */
class UsbTxDoubleBuffer final {
public:
  UsbTxDoubleBuffer() noexcept = default;
  explicit UsbTxDoubleBuffer(uint16_t packetSize) noexcept;
  ~UsbTxDoubleBuffer();

  UsbTxDoubleBuffer(const UsbTxDoubleBuffer &) = delete;
  UsbTxDoubleBuffer &operator=(const UsbTxDoubleBuffer &) = delete;

  bool Recreate(uint16_t packetSize) noexcept;
  void Clear() noexcept;
  uint32_t LoadFromQueue(LockFreeQueueBase &queue) noexcept;
  bool PeekReadyPacket(uint8_t &slotIndex, uint8_t *&data, uint16_t &length) noexcept;
  void MarkTransferStarted(uint8_t slotIndex) noexcept;
  void CompleteTransfer() noexcept;
  bool HasReadyPacket() const noexcept;
  bool IsCreated() const noexcept;

private:
  static constexpr uint8_t kSlotCount = 2U;
  static constexpr uint8_t kInvalidSlot = 0xFFU;

  struct Slot {
    uint16_t length = 0U;
    bool ready = false;
  };

  uint8_t *SlotBuffer(uint8_t slotIndex) noexcept;
  const uint8_t *SlotBuffer(uint8_t slotIndex) const noexcept;

private:
  uint8_t *storage_ = nullptr;
  uint16_t packetSize_ = 0U;
  Slot slots_[kSlotCount] {};
  uint8_t activeSlot_ = kInvalidSlot;
  uint8_t nextLoadSlot_ = 0U;
  uint8_t nextSendSlot_ = 0U;
};

/**
 * @brief 轻量级 USB CDC ACM 设备协议层。
 *
 * @details
 * - 该类只负责 USB CDC 协议与数据流编排，不把上层业务和底层 HAL PCD 直接耦合在一起。
 * - 底层发送链路：`应用写入 -> usb_cdc 发送队列 -> 发送双缓冲区 -> USB PCD IN 端点`。
 * - 底层接收链路：`USB PCD OUT 端点 -> 接收环形缓冲区 -> usb_cdc -> 上层无锁队列`。
 * - `UsbCdcAcm` 对上层只暴露字节流接口；上层无需关心 PCD 句柄、端点和回调细节。
 * - 以后若移植到别的平台，只需要替换 `.cpp` 中的底层 PCD 适配部分，不需要改动业务层。
 */
class UsbCdcAcm final {
public:
  static UsbCdcAcm &Instance();

  /** @brief 初始化 USB CDC 协议层和底层传输缓冲区。 */
  void Init();
  /** @brief 把上层接收无锁队列挂接到当前 USB CDC 实例。 */
  void AttachRxQueue(LockFreeQueueBase *queue);
  /** @brief 手动触发一次链路服务，用于把底层环形缓冲区中的数据上抛并推进发送。 */
  void Service();

  uint32_t Write(const uint8_t *data, uint32_t len);
  uint32_t Read(uint8_t *data, uint32_t len);
  uint32_t Available() const;
  uint32_t TxUsed() const;
  uint32_t TxFree() const;
  uint32_t RxUsed() const;
  uint32_t RxFree() const;
  uint32_t RxDropped() const;
  bool IsConfigured() const;

  /** @brief 以下入口由底层 HAL 回调桥接调用。 */
  void OnReset();
  void OnSetupStage();
  void OnDataInStage(uint8_t epnum);
  void OnDataOutStage(uint8_t epnum);
  void OnSuspend();
  void OnResume();

private:
  UsbCdcAcm() noexcept;

  struct SetupPacket {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
  };

  struct LineCoding {
    uint32_t baudrate;
    uint8_t stopBits;
    uint8_t parityType;
    uint8_t dataBits;
  };

  enum class Ep0OutState : uint8_t {
    kIdle = 0U,
    kSetLineCoding = 1U
  };

  static constexpr uint8_t kEp0Mps = 64U;
  static constexpr uint8_t kEpCdcDataIn = 0x81U;
  static constexpr uint8_t kEpCdcDataOut = 0x01U;
  static constexpr uint8_t kEpCdcCmdIn = 0x82U;
  static constexpr uint16_t kEpDataMps = 64U;
  static constexpr uint16_t kEpCmdMps = 8U;
  static constexpr uint32_t kTxQueueStorageSize = 1025U;
  static constexpr uint32_t kTransportRxRingStorageSize = 2049U;

  void ResetRuntimeState();
  void OpenControlEndpoints();
  void OpenDataEndpoints();
  void CloseDataEndpoints();
  void PrimeOutEndpoint();

  void HandleStandardRequest(const SetupPacket &setup);
  void HandleClassRequest(const SetupPacket &setup);
  void HandleGetDescriptor(const SetupPacket &setup);

  void StartControlInTransfer(const uint8_t *data, uint16_t len, uint16_t requestLen);
  void ContinueControlInTransfer();
  void SendControlStatus();
  void StallControlEndpoint();

  void PushReceivedPacket(const uint8_t *data, uint32_t len);
  void ServiceRxPath();
  void ServiceTxPath();
  uint32_t UpperRxUsed() const;
  uint32_t UpperRxFree() const;

private:
  LockFreeQueueBase *appRxQueue_ = nullptr;

  bool initialized_ = false;
  volatile bool configured_ = false;
  volatile bool suspended_ = false;
  volatile uint8_t currentConfig_ = 0U;
  volatile uint8_t currentInterface_ = 0U;
  LineCoding lineCoding_ {115200U, 0U, 0U, 8U};
  volatile uint8_t controlLineState_ = 0U;

  Ep0OutState ep0OutState_ = Ep0OutState::kIdle;
  uint8_t ep0OutBuffer_[kEp0Mps] {};
  uint16_t ep0OutExpectedLen_ = 0U;

  const uint8_t *ep0InPtr_ = nullptr;
  uint16_t ep0InRemaining_ = 0U;
  uint16_t ep0InRequestLen_ = 0U;
  uint8_t ep0ZlpDummy_ = 0U;
  uint8_t lineCodingBuffer_[7] {};

  uint8_t rxPacketBuffer_[kEpDataMps] {};
  uint8_t rxDrainBuffer_[kEpDataMps] {};

  ByteRingBuffer transportRxRing_ {};
  DynamicLockFreeQueue txQueue_ {};
  UsbTxDoubleBuffer txDoubleBuffer_ {};

  volatile bool txBusy_ = false;
  volatile uint32_t rxDropped_ = 0U;
};

} // namespace iFly

#endif /* IFLY_USB_CDC_HPP */
