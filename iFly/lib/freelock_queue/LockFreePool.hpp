/**
 * @file LockFreePool.hpp
 * @brief 无锁对象队列池接口与封装。
 */
#ifndef IFLY_FREELOCK_QUEUE_LOCK_FREE_POOL_HPP
#define IFLY_FREELOCK_QUEUE_LOCK_FREE_POOL_HPP

#include <atomic>
#include <stdint.h>

namespace iFly {

/**
 * @brief 单生产者多消费者无锁对象队列池基类。
 *
 * @tparam T 队列池中保存的对象类型。
 */
template <typename T>
class LockFreePoolBase {
public:
  LockFreePoolBase() = default;
  ~LockFreePoolBase() = default;

  LockFreePoolBase(const LockFreePoolBase &) = delete;
  LockFreePoolBase &operator=(const LockFreePoolBase &) = delete;

  /**
   * @brief 使用外部对象存储区创建队列池。
   *
   * @param buffer 调用方提供的对象存储区首地址。
   * @param capacity 对象存储区可保存的对象个数。
   * @return 创建成功返回 `true`。
   */
  bool Create(T *buffer, uint32_t capacity) {
    if ((buffer == nullptr) || (capacity == 0U)) {
      Delete();
      return false;
    }

    storage_ = buffer;
    capacity_ = capacity;
    head_.store(0U, std::memory_order_relaxed);
    tail_.store(0U, std::memory_order_relaxed);
    return true;
  }

  /**
   * @brief 删除队列池并解除与外部存储区的绑定关系。
   */
  void Delete() {
    head_.store(0U, std::memory_order_relaxed);
    tail_.store(0U, std::memory_order_relaxed);
    storage_ = nullptr;
    capacity_ = 0U;
  }

  /**
   * @brief 清空队列池中的全部对象。
   */
  void Clear() {
    if (!IsCreated()) {
      return;
    }

    head_.store(0U, std::memory_order_relaxed);
    tail_.store(0U, std::memory_order_relaxed);
  }

  /**
   * @brief 向队列池中写入一个对象，满时覆盖最旧对象。
   *
   * @param object 待写入的对象。
   * @return 写入成功返回 `true`。
   */
  bool Push(const T &object) {
    if (!IsCreated()) {
      return false;
    }

    const uint32_t capacity = capacity_;
    uint32_t head = head_.load(std::memory_order_relaxed);
    while ((head - tail_.load(std::memory_order_acquire)) >= capacity) {
      uint32_t tail = tail_.load(std::memory_order_acquire);
      (void)tail_.compare_exchange_weak(tail,
                                        tail + 1U,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire);
    }

    storage_[head % capacity] = object;
    head_.store(head + 1U, std::memory_order_release);
    return true;
  }

  /**
   * @brief 从队列池中读取一个对象。
   *
   * @param object 用于接收对象的地址。
   * @return 读取成功返回 `true`。
   */
  bool Pop(T *object) {
    if ((!IsCreated()) || (object == nullptr)) {
      return false;
    }

    const uint32_t capacity = capacity_;
    while (true) {
      const uint32_t tail = tail_.load(std::memory_order_acquire);
      const uint32_t head = head_.load(std::memory_order_acquire);
      if (tail == head) {
        return false;
      }

      const T value = storage_[tail % capacity];
      uint32_t expectedTail = tail;
      if (tail_.compare_exchange_weak(expectedTail,
                                      tail + 1U,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire)) {
        *object = value;
        return true;
      }
    }
  }

  /**
   * @brief 获取队列池当前已用对象个数。
   *
   * @return 已保存对象个数。
   */
  uint32_t UsedSize() const {
    if (!IsCreated()) {
      return 0U;
    }

    const uint32_t head = head_.load(std::memory_order_acquire);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t used = head - tail;
    return (used <= capacity_) ? used : capacity_;
  }

  /**
   * @brief 获取队列池当前剩余对象个数。
   *
   * @return 剩余可写对象个数。
   */
  uint32_t FreeSize() const {
    const uint32_t used = UsedSize();
    return (used < capacity_) ? (capacity_ - used) : 0U;
  }

  /**
   * @brief 获取队列池容量。
   *
   * @return 可保存对象个数。
   */
  uint32_t Capacity() const {
    return capacity_;
  }

  /**
   * @brief 判断队列池是否已创建成功。
   *
   * @return 已创建返回 `true`。
   */
  bool IsCreated() const {
    return (storage_ != nullptr) && (capacity_ > 0U);
  }

  /**
   * @brief 判断队列池是否为空。
   *
   * @return 为空返回 `true`。
   */
  bool IsEmpty() const {
    return UsedSize() == 0U;
  }

  /**
   * @brief 判断队列池是否已满。
   *
   * @return 已满返回 `true`。
   */
  bool IsFull() const {
    return FreeSize() == 0U;
  }

  /**
   * @brief 判断当前平台上的索引原子操作是否为无锁实现。
   *
   * @return 无锁返回 `true`。
   */
  bool IsLockFree() const {
    return head_.is_lock_free() && tail_.is_lock_free();
  }

private:
  T *storage_ = nullptr; /**< 调用方提供的对象存储区首地址。 */
  uint32_t capacity_ = 0U; /**< 对象存储区容量。 */
  std::atomic<uint32_t> head_ {0U}; /**< 写指针，由生产者推进。 */
  std::atomic<uint32_t> tail_ {0U}; /**< 读指针，由消费者推进。 */
};

/**
 * @brief 带内部静态存储区的无锁对象队列池封装。
 *
 * @tparam T 队列池中保存的对象类型。
 * @tparam kCapacity 对象存储区容量。
 */
template <typename T, uint32_t kCapacity>
class StaticLockFreePool : public LockFreePoolBase<T> {
public:
  static_assert(kCapacity > 0U, "kCapacity must be greater than 0.");

  /**
   * @brief 构造时自动绑定内部静态存储区。
   */
  StaticLockFreePool() {
    (void)this->Create(storage_, kCapacity);
  }

  /**
   * @brief 重新初始化队列池并绑定内部静态存储区。
   */
  void Recreate() {
    (void)this->Create(storage_, kCapacity);
  }

private:
  T storage_[kCapacity] {}; /**< 对象自带的静态存储区。 */
};

} // namespace iFly

#endif /* IFLY_FREELOCK_QUEUE_LOCK_FREE_POOL_HPP */

