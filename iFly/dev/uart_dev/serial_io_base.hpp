#ifndef IFLY_SERIAL_IO_BASE_HPP
#define IFLY_SERIAL_IO_BASE_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 统一的串口/字节流设备基类。
 *
 * @details
 * 这个类的目标是把“上层真正关心的那部分接口”抽出来统一：
 * - `Init()`：初始化底层链路
 * - `Write()`：写入一段待发送数据
 * - `Read()`：从接收无锁队列中取数据
 * - `Available()/RxFree()/RxUsed()`：查询当前接收队列状态
 * - `TxFree()/TxUsed()/RxDropped()/IsConnected()`：查询底层收发链路状态
 *
 * 设计上的关键点是：
 * 1. 这个基类直接继承 `StaticLockFreeQueue<200>`，因此“对象自身”就是用户层 RX 队列；
 * 2. USB CDC 和硬件 UART 都把收到的数据写入这条队列；
 * 3. 上层应用永远只面对同一套读写接口，不需要关心底层究竟是 USB 还是硬件串口。
 *
 * 也就是说，统一接口的核心不是把所有底层细节都塞到一个类里，
 * 而是统一“面向上层”的那层队列式字节流抽象。
 */
class SerialIoBase : public DynamicLockFreeQueue {
public:
  /** @brief 默认 RX 队列底层总存储大小，实际可用容量为该值减 1。 */
  static constexpr uint32_t kDefaultRxQueueStorageSize = 200U;

  /**
   * @brief 构造时记录期望的 RX 队列大小。
   *
   * @details
   * 这里并不强制要求构造阶段就一定创建成功，
   * 因为嵌入式环境下动态分配可能失败，所以后续 `Init()` 里还会再次兜底检查。
   */
  explicit SerialIoBase(uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize) noexcept
      : DynamicLockFreeQueue(),
        rxQueueStorageSize_((rxQueueStorageSize >= 2U) ? rxQueueStorageSize : kDefaultRxQueueStorageSize) {
  }

  virtual ~SerialIoBase() = default;

  SerialIoBase(const SerialIoBase &) = delete;
  SerialIoBase &operator=(const SerialIoBase &) = delete;

  /** @brief 初始化底层链路。由具体派生类实现。 */
  virtual void Init() = 0;
  /** @brief 向底层发送方向写入一段字节流。由具体派生类实现。 */
  virtual uint32_t Write(const uint8_t *data, uint32_t len) = 0;
  /** @brief 查询底层发送缓冲剩余空间。 */
  virtual uint32_t TxFree() const = 0;
  /** @brief 查询底层发送缓冲已用空间。 */
  virtual uint32_t TxUsed() const = 0;
  /** @brief 查询接收链路累计丢弃的字节数。 */
  virtual uint32_t RxDropped() const = 0;
  /** @brief 查询当前链路是否已经可用。 */
  virtual bool IsConnected() const = 0;

  /**
   * @brief 从统一的 RX 无锁队列中读取数据。
   *
   * @details
   * `Read()` 先调用 `BeforeRead()`，给派生类一个机会把底层暂存数据
   * 继续上抛到当前 RX 队列，然后再真正执行 `Dequeue()`。
   *
   * 对 USB CDC 来说，这一步会触发 `Device().Service()`；
   * 对硬件 UART 来说，DMA 回调已经直接把数据塞进队列，所以通常不需要额外动作。
   */
  uint32_t Read(uint8_t *data, uint32_t len) {
    BeforeRead();
    return Dequeue(data, len);
  }

  /** @brief 返回当前 RX 队列中的可读字节数。 */
  uint32_t Available() const noexcept {
    return UsedSize();
  }

  /** @brief 返回当前 RX 队列剩余可写空间。 */
  uint32_t RxFree() const noexcept {
    return FreeSize();
  }

  /** @brief 返回当前 RX 队列已用空间。 */
  uint32_t RxUsed() const noexcept {
    return UsedSize();
  }

protected:
  /**
   * @brief 确保统一 RX 队列已经创建成功。
   *
   * @details
   * 如果构造阶段创建失败，这里会按记录下来的默认大小再次尝试。
   */
  bool EnsureRxQueueCreated() noexcept {
    if (!IsCreated()) {
      (void)Recreate(rxQueueStorageSize_);
    }
    return IsCreated();
  }

  /**
   * @brief 以基类视角暴露 RX 队列指针。
   *
   * @details
   * 底层传输层并不需要知道当前对象的具体类型，
   * 只需要拿到一个 `LockFreeQueueBase*`，把收到的数据塞进去即可。
   */
  LockFreeQueueBase *RxQueue() noexcept {
    return this;
  }

  /**
   * @brief 在真正执行 `Read()` 前的钩子函数。
   *
   * @details
   * 默认什么都不做。派生类如果有“底层暂存区 -> 统一 RX 队列”的服务动作，
   * 可以在这里补上。
   */
  virtual void BeforeRead() {
  }

private:
  // 记录期望的 RX 队列总存储大小，真正分配放到 Init() 阶段兜底执行。
  // 这样派生类传下来的 rxQueueStorageSize 就不会再被忽略。
  uint32_t rxQueueStorageSize_ = kDefaultRxQueueStorageSize;

};

} // namespace iFly

#endif /* IFLY_SERIAL_IO_BASE_HPP */
