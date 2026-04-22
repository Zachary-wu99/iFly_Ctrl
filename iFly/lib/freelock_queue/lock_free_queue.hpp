/**
 * @file lock_free_queue.hpp
 * @brief 无锁字节队列接口与封装。
 */
#ifndef IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP
#define IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP

#include <atomic>
#include <new>
#include <stddef.h>
#include <stdint.h>

#include "usermath.hpp"

namespace iFly {

/**
 * @brief 单生产者多消费者无锁字节队列基类。
 *
 * @details
 * - 队列底层使用环形缓冲区实现。
 * - 存储区由调用方提供或由派生类封装。
 * - 内部固定保留 1 字节作为判空与判满哨兵。
 */
class LockFreeQueueBase {
public:
  LockFreeQueueBase() = default;
  ~LockFreeQueueBase() = default;

  LockFreeQueueBase(const LockFreeQueueBase &) = delete;
  LockFreeQueueBase &operator=(const LockFreeQueueBase &) = delete;

  /**
   * @brief 使用外部缓冲区创建队列。
   *
   * @param buffer 调用方提供的字节缓冲区首地址。
   * @param bufferSize 缓冲区总大小，单位为字节。
   * @return 创建成功返回 `true`。
   */
  bool Create(uint8_t *buffer, uint32_t bufferSize);

  /**
   * @brief 删除队列并解除与外部缓冲区的绑定关系。
   */
  void Delete();

  /**
   * @brief 清空队列中的全部数据。
   */
  void Clear();

  /**
   * @brief 向队列中写入数据。
   *
   * @param data 待入队的数据首地址。
   * @param length 期望写入的字节数。
   * @return 实际成功写入的字节数。
   */
  uint32_t Enqueue(const uint8_t *data, uint32_t length);

  /**
   * @brief 从队列中读取数据。
   *
   * @param data 用于接收数据的缓冲区首地址。
   * @param length 期望读取的字节数。
   * @return 实际成功读取的字节数。
   */
  uint32_t Dequeue(uint8_t *data, uint32_t length);

  /**
   * @brief 获取队列当前已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t UsedSize() const;

  /**
   * @brief 获取队列当前剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t FreeSize() const;

  /**
   * @brief 获取队列可用容量。
   *
   * @return 可用容量，单位为字节。
   */
  uint32_t Capacity() const;

  /**
   * @brief 获取底层缓冲区总大小。
   *
   * @return 缓冲区总大小，单位为字节。
   */
  uint32_t StorageSize() const;

  /**
   * @brief 判断队列是否已创建成功。
   *
   * @return 已创建返回 `true`。
   */
  bool IsCreated() const;

  /**
   * @brief 判断队列是否为空。
   *
   * @return 为空返回 `true`。
   */
  bool IsEmpty() const;

  /**
   * @brief 判断队列是否已满。
   *
   * @return 已满返回 `true`。
   */
  bool IsFull() const;

  /**
   * @brief 判断当前平台上的索引原子操作是否为无锁实现。
   *
   * @return 无锁返回 `true`。
   */
  bool IsLockFree() const;

protected:
  /**
   * @brief 获取底层缓冲区可写指针。
   *
   * @return 缓冲区首地址。
   */
  uint8_t *Buffer();

  /**
   * @brief 获取底层缓冲区只读指针。
   *
   * @return 缓冲区首地址。
   */
  const uint8_t *Buffer() const;

private:
  /**
   * @brief 计算环形缓冲区中 head 到 tail 之间的有效数据长度。
   *
   * @param head 当前写指针。
   * @param tail 当前读指针。
   * @param size 环形缓冲区总大小。
   * @return 当前已使用的数据量。
   */
  static uint32_t Distance(uint32_t head, uint32_t tail, uint32_t size);

  uint8_t *storage_ = nullptr; /**< 调用方提供的底层缓冲区首地址。 */
  uint32_t storageSize_ = 0U; /**< 底层缓冲区总大小。 */
  std::atomic<uint32_t> head_ {0U}; /**< 写指针，由生产者推进。 */
  std::atomic<uint32_t> tail_ {0U}; /**< 读指针，由消费者推进。 */
};

/**
 * @brief 带内部静态存储区的无锁队列封装。
 *
 * @tparam kStorageSize 底层缓冲区总大小。
 */
template <uint32_t kStorageSize>
class StaticLockFreeQueue : public LockFreeQueueBase {
public:
  static_assert(kStorageSize >= 2U, "kStorageSize must be at least 2 bytes.");

  /**
   * @brief 构造时自动绑定内部静态缓冲区。
   */
  StaticLockFreeQueue() {
    (void)Create(storage_, kStorageSize);
  }

  /**
   * @brief 重新初始化队列并绑定内部静态缓冲区。
   */
  void Recreate() {
    (void)Create(storage_, kStorageSize);
  }

private:
  uint8_t storage_[kStorageSize] {}; /**< 对象自带的静态缓冲区。 */
};

/**
 * @brief 带内部动态存储区的无锁队列封装。
 */
class DynamicLockFreeQueue : public LockFreeQueueBase {
public:
  /**
   * @brief 默认构造，此时尚未创建缓冲区。
   */
  DynamicLockFreeQueue() = default;

  /**
   * @brief 构造时按指定大小创建缓冲区。
   *
   * @param storageSize 底层缓冲区总大小。
   */
  explicit DynamicLockFreeQueue(uint32_t storageSize);

  /**
   * @brief 析构时释放内部动态缓冲区。
   */
  ~DynamicLockFreeQueue();

  DynamicLockFreeQueue(const DynamicLockFreeQueue &) = delete;
  DynamicLockFreeQueue &operator=(const DynamicLockFreeQueue &) = delete;

  /**
   * @brief 重新创建队列并重新申请底层缓冲区。
   *
   * @param storageSize 底层缓冲区总大小。
   * @return 创建成功返回 `true`。
   */
  bool Recreate(uint32_t storageSize);

private:
  /**
   * @brief 释放当前持有的动态缓冲区。
   */
  void ReleaseStorage();

  uint8_t *ownedStorage_ = nullptr; /**< 当前对象持有的动态缓冲区首地址。 */
};

} // namespace iFly

#endif /* IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP */
