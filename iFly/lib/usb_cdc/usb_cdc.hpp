#ifndef IFLY_USB_CDC_HPP
#define IFLY_USB_CDC_HPP

#include <stddef.h>
#include <stdint.h>

#include "double_buffer.hpp"
#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief USB 数据端点使用的双缓冲区。
 * @details
 * - 参考 `E:\project\DragonFly_Project\myfly\usb` 中端点拥有缓冲区的链路组织方式；
 * - USB PCD 直接面向槽位内存进行收发，减少 `usb_cdc` 内部中转拷贝；
 * - 发送方向可在当前包传输时，向另一个槽位预装下一包数据；
 * - 接收方向可在处理当前包时，立即把另一个槽位重新挂给 OUT 端点。
 */
class UsbEndpointDoubleBuffer final : public StaticByteDoubleBuffer<200U> {
public:
  UsbEndpointDoubleBuffer() noexcept = default;
  explicit UsbEndpointDoubleBuffer(uint16_t packetSize) noexcept
      : StaticByteDoubleBuffer<200U>(packetSize) {
  }

private:
  using StaticByteDoubleBuffer<200U>::StaticByteDoubleBuffer;
};

/**
 * @brief 轻量级 USB CDC ACM 设备协议层。
 * @details
 * - 底层发送链路：`应用写入 -> usb_cdc 发送队列 -> USB IN 端点双缓冲 -> USB PCD`
 * - 底层接收链路：`USB PCD OUT 端点 -> USB OUT 端点双缓冲 -> 上层无锁队列`
 * - 对外仍保持字节流接口，便于继续通过 `usb_uart` 使用。
 */
class UsbCdcAcm final {
public:
  static UsbCdcAcm &Instance();

  void Init();
  void AttachRxQueue(LockFreeQueueBase *queue);
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
  static constexpr uint32_t kTxQueueStorageSize = 200U;

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
  void ServiceTxPath();
  uint32_t LoadTxPacketToInactiveBuffer() noexcept;
  uint32_t UpperRxUsed() const;
  uint32_t UpperRxFree() const;

private:
  LockFreeQueueBase *appRxQueue_ = nullptr;

  std::atomic<bool> initialized_ {false};
  std::atomic<bool> configured_ {false};
  std::atomic<bool> suspended_ {false};
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

  UsbEndpointDoubleBuffer rxEndpointBuffer_ {};
  StaticLockFreeQueue<kTxQueueStorageSize> txQueue_ {};
  UsbEndpointDoubleBuffer txEndpointBuffer_ {};

  std::atomic<bool> txBusy_ {false};
  std::atomic<uint32_t> rxDropped_ {0U};
  std::atomic<uint32_t> txServiceRequests_ {0U};
  std::atomic<bool> txServiceRunning_ {false};
};

} // namespace iFly

#endif /* IFLY_USB_CDC_HPP */
