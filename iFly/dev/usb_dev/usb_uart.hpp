#ifndef IFLY_USB_UART_HPP
#define IFLY_USB_UART_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"
#include "usb_cdc.hpp"

namespace iFly {

/**
 * @brief 面向业务层的 USB 虚拟串口对象。
 *
 * @details
 * - 该类公开继承 `DynamicLockFreeQueue`，因此对象自身就是一个“接收无锁队列”。
 * - USB CDC 模块把收到的数据写入当前对象继承而来的队列；应用层则从该队列读取。
 * - 发送方向仍然保持“串口风格”接口：上层调用 `Write()`，底层自动推进 USB IN 发送。
 * - 这样做以后，上层应用既可以继续使用 `Read()/Available()` 这样的串口接口，
 *   也可以在需要时直接调用基类的 `Dequeue()/UsedSize()/FreeSize()` 等无锁队列接口。
 * - 队列缓冲区在构造阶段一次性动态申请，运行过程中不再申请内存。
 */
class UsbUart final : public DynamicLockFreeQueue {
public:
  /** @brief 默认接收队列总存储大小，实际可用容量为该值减 1。 */
  static constexpr uint32_t kDefaultRxQueueStorageSize = 2049U;

  /**
   * @brief 构造时创建应用层接收队列。
   *
   * @param rxQueueStorageSize 接收队列底层总存储大小，至少为 2。
   */
  explicit UsbUart(uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize) noexcept;

  /**
   * @brief 初始化 USB CDC 链路，并把当前对象注册为上层接收队列。
   *
   * @note
   * - 调用前应先完成底层 `MX_USB_OTG_FS_PCD_Init()`。
   * - 若构造阶段的队列申请失败，本函数会再次尝试创建默认大小的接收队列。
   */
  void Init();

  /**
   * @brief 向 USB CDC 发送通道写入数据。
   *
   * @param data 待发送数据首地址。
   * @param len 期望发送的字节数。
   * @return 实际成功写入底层发送队列的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len) const;

  /**
   * @brief 以串口风格从当前对象继承的接收队列中读取数据。
   *
   * @param data 输出缓冲区首地址。
   * @param len 希望读取的最大字节数。
   * @return 实际读出的字节数。
   */
  uint32_t Read(uint8_t *data, uint32_t len);

  /** @brief 返回当前接收队列中的可读字节数。 */
  uint32_t Available() const;
  /** @brief 返回底层 USB CDC 发送队列剩余空间。 */
  uint32_t TxFree() const;
  /** @brief 返回底层 USB CDC 发送队列已用空间。 */
  uint32_t TxUsed() const;
  /** @brief 返回当前接收队列剩余空间。 */
  uint32_t RxFree() const;
  /** @brief 返回当前接收队列已用空间。 */
  uint32_t RxUsed() const;
  /** @brief 返回从 USB PCD 到应用接收链路累计丢弃的字节数。 */
  uint32_t RxDropped() const;
  /** @brief 查询 USB CDC 是否已经被主机完成配置。 */
  bool IsConnected() const;

private:
  /** @brief 统一获取底层 USB CDC 单例，避免重复书写。 */
  static UsbCdcAcm &Device();
};

/**
 * @brief 与用户原始命名保持兼容的别名。
 *
 * @details
 * 业务层既可以写 `iFly::UsbUart`，也可以继续写 `iFly::usb_uart`。
 */
using usb_uart = UsbUart;

} // namespace iFly

#endif /* IFLY_USB_UART_HPP */
