// USB CDC 串口封装接口。
// 向上提供与硬件 UART 一致的串口抽象。
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
 * 这个类现在和 `HardwareUart` 一样，统一继承 `SerialIoBase`。
 *
 * 这样做以后：
 * - USB CDC 和硬件 UART 都拥有同样的读写接口；
 * - 两者都把“收到的数据”落到统一的 RX 无锁队列；
 * - 上层应用可以只依赖同一套队列式串口 API，不再关心底层传输介质。
 */
class UsbUart final : public SerialIoBase {
public:
  /** @brief 构造一个 USB CDC 串口对象，并记录 RX 队列大小。 */
  explicit UsbUart(uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize);

  /** @brief 初始化 USB CDC，并把本对象的 RX 队列挂接到 CDC 底层。 */
  void Init() override;
  /** @brief 向 USB CDC 发送路径写入一段数据。 */
  uint32_t Write(const uint8_t *data, uint32_t len) override;
  /** @brief 返回 CDC 底层发送队列剩余空间。 */
  uint32_t TxFree() const override;
  /** @brief 返回 CDC 底层发送队列已用空间。 */
  uint32_t TxUsed() const override;
  /** @brief 返回 CDC 接收链路累计丢弃的字节数。 */
  uint32_t RxDropped() const override;
  /** @brief 查询 CDC 是否已经完成枚举并处于可用状态。 */
  bool IsConnected() const override;

private:
  /** @brief 获取底层 USB CDC 单例。 */
  static UsbCdcAcm &Device();

  /** @brief 在真正 `Read()` 前先推动 CDC 链路服务，把暂存数据上抛到 RX 队列。 */
  void BeforeRead() override;
};

using usb_uart = UsbUart;

} // namespace iFly

#endif /* IFLY_USB_UART_HPP */
