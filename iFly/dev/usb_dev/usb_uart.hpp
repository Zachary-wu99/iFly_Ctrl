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
 */
class UsbUart final : public SerialIoBase {
public:
  /**
   * @brief 构造一个 USB CDC 串口对象。
   *
   * @param rxQueueStorageSize 接收队列总容量。
   */
  explicit UsbUart(uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize);

  /**
   * @brief 初始化 USB CDC 链路。
   */
  void Init() override;

  /**
   * @brief 写入待发送数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际写入的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len) override;

  /**
   * @brief 获取发送队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t TxFree() const override;

  /**
   * @brief 获取发送队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t TxUsed() const override;

  /**
   * @brief 获取累计丢弃的接收字节数。
   *
   * @return 累计丢弃字节数。
   */
  uint32_t RxDropped() const override;

  /**
   * @brief 判断 USB CDC 是否已经可用。
   *
   * @return 已完成枚举并可通信时返回 `true`。
   */
  bool IsConnected() const override;

private:
  /**
   * @brief 获取底层 USB CDC 单例。
   *
   * @return `UsbCdcAcm` 单例引用。
   */
  static UsbCdcAcm &Device();

  /**
   * @brief 在读取前推动底层 CDC 服务。
   */
  void BeforeRead() override;
};

using usb_uart = UsbUart;

} // namespace iFly

#endif /* IFLY_USB_UART_HPP */
