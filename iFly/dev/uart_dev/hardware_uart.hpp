/**
 * @file hardware_uart.hpp
 * @brief 硬件 UART 设备封装接口。
 */
#ifndef IFLY_HARDWARE_UART_HPP
#define IFLY_HARDWARE_UART_HPP

#include <stdint.h>

#include "serial_io_base.hpp"
#include "uart_dma.hpp"

namespace iFly {

/**
 * @brief 面向上层的硬件串口对象。
 *
 * @details
 * 该类负责把某一路逻辑串口包装成统一的字节流设备接口，
 * 具体的 DMA 双缓冲发送、DMA 环形接收和 HAL 回调桥接
 * 由 `UartDmaService` 实现。
 */
class HardwareUart final : public SerialIoBase {
public:
  /**
   * @brief 构造一个逻辑串口对象。
   *
   * @param port 逻辑串口编号。
   * @param rxQueueStorageSize 接收队列总容量。
   */
  explicit HardwareUart(UartPortId port,
                        uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize);

  /**
   * @brief 初始化当前串口。
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
   * @brief 获取底层发送队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t TxFree() const override;

  /**
   * @brief 获取底层发送队列已用空间。
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
   * @brief 判断当前串口是否已完成初始化。
   *
   * @return 可用返回 `true`。
   */
  bool IsConnected() const override;

  /**
   * @brief 获取当前绑定的逻辑串口号。
   *
   * @return 逻辑串口编号。
   */
  UartPortId Port() const;

private:
  /**
   * @brief 获取底层 UART DMA 服务单例。
   *
   * @return `UartDmaService` 单例引用。
   */
  static UartDmaService &Device();

  UartPortId port_; /**< 当前对象绑定的逻辑串口号。 */
};

using uart_port = HardwareUart;

} // namespace iFly

#endif /* IFLY_HARDWARE_UART_HPP */
