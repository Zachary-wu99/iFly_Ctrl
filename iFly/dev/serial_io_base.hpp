/**
 * @file serial_io_base.hpp
 * @brief 串行 IO 抽象基类。
 */
#ifndef IFLY_SERIAL_IO_BASE_HPP
#define IFLY_SERIAL_IO_BASE_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 统一的串行字节流设备基类。
 *
 * @details
 * 该类统一抽象 USB、UART、CAN 等上层关心的通用能力，包括：
 * - 链路初始化；
 * - 字节流写入；
 * - 接收队列读取；
 * - 收发队列状态查询。
 */
class SerialIoBase : public DynamicLockFreeQueue {
public:
  static constexpr uint32_t kDefaultRxQueueStorageSize = 120U; /**< 默认接收队列总容量。 */

  /**
   * @brief 构造串行 IO 对象。
   *
   * @param rxQueueStorageSize 期望创建的接收队列总容量。
   */
  explicit SerialIoBase(uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize)
      : DynamicLockFreeQueue(),
        rxQueueStorageSize_((rxQueueStorageSize >= 2U) ? rxQueueStorageSize
                                                      : kDefaultRxQueueStorageSize) {
  }

  virtual ~SerialIoBase() = default;

  SerialIoBase(const SerialIoBase &) = delete;
  SerialIoBase &operator=(const SerialIoBase &) = delete;

  /**
   * @brief 初始化底层链路。
   */
  virtual void Init() = 0;

  /**
   * @brief 向底层链路写入待发送数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际写入的字节数。
   */
  virtual uint32_t Write(const uint8_t *data, uint32_t len) = 0;

  /**
   * @brief 获取底层发送缓冲剩余空间。
   *
   * @return 剩余可写字节数。
   */
  virtual uint32_t TxFree() const = 0;

  /**
   * @brief 获取底层发送缓冲已用空间。
   *
   * @return 已使用字节数。
   */
  virtual uint32_t TxUsed() const = 0;

  /**
   * @brief 获取接收链路累计丢弃字节数。
   *
   * @return 累计丢弃字节数。
   */
  virtual uint32_t RxDropped() const = 0;

  /**
   * @brief 判断链路是否处于可用状态。
   *
   * @return 可用返回 `true`。
   */
  virtual bool IsConnected() const = 0;

  /**
   * @brief 从统一接收队列中读取数据。
   *
   * @param data 输出缓冲区首地址。
   * @param len 期望读取长度，单位为字节。
   * @return 实际读取的字节数。
   */
  uint32_t Read(uint8_t *data, uint32_t len) {
    BeforeRead();
    return Dequeue(data, len);
  }

  /**
   * @brief 获取当前可读字节数。
   *
   * @return 接收队列已用空间。
   */
  uint32_t Available() const {
    return UsedSize();
  }

  /**
   * @brief 获取接收队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t RxFree() const {
    return FreeSize();
  }

  /**
   * @brief 获取接收队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t RxUsed() const {
    return UsedSize();
  }

protected:
  /**
   * @brief 确保接收队列已创建成功。
   *
   * @return 队列存在返回 `true`。
   */
  bool EnsureRxQueueCreated() {
    if (!IsCreated()) {
      (void)Recreate(rxQueueStorageSize_);
    }
    return IsCreated();
  }

  /**
   * @brief 以基类视角暴露接收队列指针。
   *
   * @return 接收队列基类指针。
   */
  LockFreeQueueBase *RxQueue() {
    return this;
  }

  /**
   * @brief 在执行读取前补做底层服务。
   *
   * @details
   * 默认不做任何动作。派生类可重写该接口，把底层暂存区内的数据
   * 推送到统一接收队列。
   */
  virtual void BeforeRead() {
  }

private:
  uint32_t rxQueueStorageSize_ = kDefaultRxQueueStorageSize; /**< 接收队列期望容量。 */
};

} // namespace iFly

#endif /* IFLY_SERIAL_IO_BASE_HPP */
