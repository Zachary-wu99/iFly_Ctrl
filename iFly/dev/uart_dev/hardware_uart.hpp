// 硬件 UART 设备封装接口。
// 将具体 UART 端口包装成统一的串口字节流对象。
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
 * 这个类本身很薄，职责刻意保持简单：
 * - 负责绑定一个逻辑串口号 `UartPortId`
 * - 负责持有统一 RX 队列（来自 `SerialIoBase`）
 * - 把上层的 `Init()/Write()/Read()` 请求转交给 `UartDmaService`
 *
 * 真正的 DMA 双缓冲发送、DMA 环形接收、HAL 回调桥接，都在底层
 * `iFly/lib/uart/uart_dma.*` 里做。
 */
class HardwareUart final : public SerialIoBase {
public:
  /**
   * @brief 构造一个逻辑硬件串口对象。
   *
   * @param port 逻辑端口号。软件统一支持 8 路。
   * @param rxQueueStorageSize 用户层 RX 队列大小。
   */
  explicit HardwareUart(UartPortId port,
                        uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize);

  /** @brief 初始化当前端口的 UART DMA 链路，并把本对象的 RX 队列挂接到底层。 */
  void Init() override;
  /** @brief 把一段待发送数据写入当前端口的发送无锁队列。 */
  uint32_t Write(const uint8_t *data, uint32_t len) override;
  /** @brief 返回当前端口底层 TX 队列剩余空间。 */
  uint32_t TxFree() const override;
  /** @brief 返回当前端口底层 TX 队列已用空间。 */
  uint32_t TxUsed() const override;
  /** @brief 返回当前端口接收链路累计丢弃的字节数。 */
  uint32_t RxDropped() const override;
  /** @brief 当前端口是否已完成底层 DMA 接收初始化。 */
  bool IsConnected() const override;

  /** @brief 返回当前对象绑定的逻辑端口号。 */
  UartPortId Port() const;

private:
  /** @brief 获取硬件 UART DMA 单例服务。 */
  static UartDmaService &Device();

private:
  UartPortId port_;
};

using uart_port = HardwareUart;

} // namespace iFly

#endif /* IFLY_HARDWARE_UART_HPP */
