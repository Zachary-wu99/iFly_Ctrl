/**
 * @file usb_cdc.hpp
 * @brief USB CDC ACM 协议层接口。
 */
#ifndef IFLY_USB_CDC_HPP
#define IFLY_USB_CDC_HPP

#include <stddef.h>
#include <stdint.h>

#include "double_buffer.hpp"
#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief USB 数据端点使用的双缓冲区。
 */
class UsbEndpointDoubleBuffer final : public StaticByteDoubleBuffer<200U> {
public:
  UsbEndpointDoubleBuffer() = default;
  explicit UsbEndpointDoubleBuffer(uint16_t packetSize)
      : StaticByteDoubleBuffer<200U>(packetSize) {
  }

private:
  using StaticByteDoubleBuffer<200U>::StaticByteDoubleBuffer;
};

/**
 * @brief 轻量级 USB CDC ACM 设备协议层。
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
  UsbCdcAcm();

  struct SetupPacket {
    uint8_t bmRequestType; /**< 控制请求类型。 */
    uint8_t bRequest; /**< 控制请求编号。 */
    uint16_t wValue; /**< 控制请求参数值。 */
    uint16_t wIndex; /**< 控制请求索引值。 */
    uint16_t wLength; /**< 控制请求数据长度。 */
  };

  struct LineCoding {
    uint32_t baudrate; /**< 波特率。 */
    uint8_t stopBits; /**< 停止位配置。 */
    uint8_t parityType; /**< 校验位配置。 */
    uint8_t dataBits; /**< 数据位宽。 */
  };

  enum class Ep0OutState : uint8_t {
    kIdle = 0U, /**< 空闲状态。 */
    kSetLineCoding = 1U /**< 正在接收设置串口格式请求。 */
  };

  static constexpr uint8_t kEp0Mps = 64U; /**< 控制端点最大包长。 */
  static constexpr uint8_t kEpCdcDataIn = 0x81U; /**< CDC 数据 IN 端点地址。 */
  static constexpr uint8_t kEpCdcDataOut = 0x01U; /**< CDC 数据 OUT 端点地址。 */
  static constexpr uint8_t kEpCdcCmdIn = 0x82U; /**< CDC 命令 IN 端点地址。 */
  static constexpr uint16_t kEpDataMps = 64U; /**< 数据端点最大包长。 */
  static constexpr uint16_t kEpCmdMps = 8U; /**< 命令端点最大包长。 */
  static constexpr uint32_t kTxQueueStorageSize = 200U; /**< 发送队列容量。 */

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
  uint32_t LoadTxPacketToInactiveBuffer();
  uint32_t UpperRxUsed() const;
  uint32_t UpperRxFree() const;

  LockFreeQueueBase *appRxQueue_ = nullptr; /**< 上层接收队列。 */

  std::atomic<bool> initialized_ {false}; /**< 协议层是否已初始化。 */
  std::atomic<bool> configured_ {false}; /**< 是否已被主机配置。 */
  std::atomic<bool> suspended_ {false}; /**< 当前是否处于挂起状态。 */
  volatile uint8_t currentConfig_ = 0U; /**< 当前 USB 配置值。 */
  volatile uint8_t currentInterface_ = 0U; /**< 当前接口号。 */
  LineCoding lineCoding_ {115200U, 0U, 0U, 8U}; /**< 当前串口格式。 */
  volatile uint8_t controlLineState_ = 0U; /**< 主机设置的控制线状态。 */

  Ep0OutState ep0OutState_ = Ep0OutState::kIdle; /**< 控制端点 OUT 状态机状态。 */
  uint8_t ep0OutBuffer_[kEp0Mps] {}; /**< 控制端点 OUT 缓冲区。 */
  uint16_t ep0OutExpectedLen_ = 0U; /**< 控制端点 OUT 期望接收长度。 */

  const uint8_t *ep0InPtr_ = nullptr; /**< 控制端点 IN 当前发送指针。 */
  uint16_t ep0InRemaining_ = 0U; /**< 控制端点 IN 剩余发送长度。 */
  uint16_t ep0InRequestLen_ = 0U; /**< 控制请求声明的数据长度。 */
  uint8_t ep0ZlpDummy_ = 0U; /**< 零长度包占位字节。 */
  uint8_t lineCodingBuffer_[7] {}; /**< LineCoding 临时编码缓冲区。 */

  UsbEndpointDoubleBuffer rxEndpointBuffer_ {}; /**< OUT 端点双缓冲。 */
  StaticLockFreeQueue<kTxQueueStorageSize> txQueue_ {}; /**< 发送队列。 */
  UsbEndpointDoubleBuffer txEndpointBuffer_ {}; /**< IN 端点双缓冲。 */

  std::atomic<bool> txBusy_ {false}; /**< 当前是否正在发送数据。 */
  std::atomic<uint32_t> rxDropped_ {0U}; /**< 接收链路累计丢弃字节数。 */
  std::atomic<uint32_t> txServiceRequests_ {0U}; /**< 待处理发送服务请求计数。 */
  std::atomic<bool> txServiceRunning_ {false}; /**< 发送服务是否正在运行。 */
};

} // namespace iFly

#endif /* IFLY_USB_CDC_HPP */
