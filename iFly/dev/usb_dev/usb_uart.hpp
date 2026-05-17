/**
 * @file usb_uart.hpp
 * @brief USB CDC 串口封装接口。
 */
#ifndef IFLY_USB_UART_HPP
#define IFLY_USB_UART_HPP

#include <stdint.h>

#include "serial_io_base.hpp"
#include "usb_cdc.hpp"

namespace iFly {

/**
 * @brief 面向上层的 USB CDC 串口对象。
 *
 * @details
 * 该类与 `HardwareUart` 一样继承 `SerialIoBase`，从而让 USB CDC
 * 与硬件串口在上层具备一致的读写接口和接收队列访问方式。
 *
 * @tparam kEndpointBufferSize USB CDC 端点双缓冲区单槽大小。
 * @tparam kTxQueueStorageSize 发送队列总容量。
 * @tparam kRxQueueStorageSize 接收队列总容量。
 */
template <uint16_t kEndpointBufferSize = kDefaultUsbEndpointBufferSize,
          uint32_t kTxQueueStorageSize = kDefaultUsbTxQueueStorageSize,
          uint32_t kRxQueueStorageSize = SerialIoBase::kDefaultRxQueueStorageSize>
class UsbUart final : public SerialIoBase {
public:
  static_assert(kEndpointBufferSize >= kDefaultUsbEndpointPacketSize,
                "kEndpointBufferSize must hold at least one endpoint packet.");
  static_assert(kTxQueueStorageSize >= 2U, "kTxQueueStorageSize must be at least 2 bytes.");
  static_assert(kRxQueueStorageSize >= 2U, "kRxQueueStorageSize must be at least 2 bytes.");

  /**
   * @brief 构造一个 USB CDC 串口对象。
   *
   * @param rxQueueStorageSize 接收队列总容量，默认使用模板参数。
   */
  explicit UsbUart(uint32_t rxQueueStorageSize = kRxQueueStorageSize)
      : SerialIoBase(rxQueueStorageSize) {
  }

  /**
   * @brief 初始化 USB CDC 链路。
   */
  void Init() override {
    if (!EnsureRxQueueCreated()) {
      return;
    }

    txQueue_.Recreate();
    rxEndpointBuffer_.Recreate();
    txEndpointBuffer_.Recreate();
    Device().AttachStorage(&txQueue_, &rxEndpointBuffer_, &txEndpointBuffer_);
    Device().AttachRxQueue(RxQueue());
    Device().Init();
  }

  /**
   * @brief 写入待发送数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际写入的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len) override {
    return Device().Write(data, len);
  }

  /**
   * @brief 获取发送队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t TxFree() const override {
    return Device().TxFree();
  }

  /**
   * @brief 获取发送队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t TxUsed() const override {
    return Device().TxUsed();
  }

  /**
   * @brief 获取累计丢弃的接收字节数。
   *
   * @return 累计丢弃字节数。
   */
  uint32_t RxDropped() const override {
    return Device().RxDropped();
  }

  /**
   * @brief 判断 USB CDC 是否已经可用。
   *
   * @return 已完成枚举并可通信时返回 `true`。
   */
  bool IsConnected() const override {
    return Device().IsConfigured();
  }

private:
  /**
   * @brief 获取底层 USB CDC 单例。
   *
   * @return `UsbCdcAcm` 单例引用。
   */
  static UsbCdcAcm &Device() {
    return UsbCdcAcm::Instance();
  }

  /**
   * @brief 在读取前推动底层 CDC 服务。
   */
  void BeforeRead() override {
    Device().Service();
  }

  StaticLockFreeQueue<kTxQueueStorageSize> txQueue_ {}; /**< 发送方向字节队列。 */
  UsbEndpointDoubleBuffer<kEndpointBufferSize> rxEndpointBuffer_ {}; /**< OUT 端点双缓冲区。 */
  UsbEndpointDoubleBuffer<kEndpointBufferSize> txEndpointBuffer_ {}; /**< IN 端点双缓冲区。 */
};

using usb_uart = UsbUart<>;

} // namespace iFly

#endif /* IFLY_USB_UART_HPP */

