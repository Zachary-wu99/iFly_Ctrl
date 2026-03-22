#include "lock_free_queue.hpp"

#include <new>
#include <string.h>

namespace iFly {

/*
 * 返回两个无符号整数中的较小值。
 * 这里单独封装一个小函数，避免在各处重复书写同样的比较逻辑。
 */
uint32_t LockFreeQueueBase::MinU32(uint32_t left, uint32_t right) noexcept {
  return (left < right) ? left : right;
}

/*
 * 计算环形缓冲区当前已存放的数据量。
 *
 * 当 head >= tail 时，说明有效数据区间连续，长度就是 head - tail。
 * 当 head < tail 时，说明写指针已经回卷，数据分布在缓冲区尾部和头部两段，
 * 此时有效长度为 size - (tail - head)。
 */
uint32_t LockFreeQueueBase::Distance(uint32_t head, uint32_t tail, uint32_t size) noexcept {
  if (head >= tail) {
    return head - tail;
  }

  return size - (tail - head);
}

/*
 * 绑定调用方提供的缓冲区，并将队列状态复位到初始状态。
 *
 * bufferSize 至少为 2：
 * - 1 字节用于实际存储数据；
 * - 1 字节作为环形队列判空/判满的保留空间。
 */
bool LockFreeQueueBase::Create(uint8_t *buffer, uint32_t bufferSize) noexcept {
  if ((buffer == nullptr) || (bufferSize < 2U)) {
    Delete();
    return false;
  }

  storage_ = buffer;
  storageSize_ = bufferSize;
  head_.store(0U, std::memory_order_relaxed);
  tail_.store(0U, std::memory_order_relaxed);
  return true;
}

/*
 * 删除队列。
 *
 * 这里的“删除”仅表示逻辑上解除队列与外部缓冲区的关联，
 * 并不会释放 buffer 指向的内存，因为该内存不归本类所有。
 */
void LockFreeQueueBase::Delete() noexcept {
  head_.store(0U, std::memory_order_relaxed);
  tail_.store(0U, std::memory_order_relaxed);
  storage_ = nullptr;
  storageSize_ = 0U;
}

/*
 * 清空队列。
 *
 * 只需要把读写索引都复位到 0，即可让队列重新变成“空”。
 * 缓冲区中的旧字节虽然还在物理内存里，但从逻辑上已经不可见。
 */
void LockFreeQueueBase::Clear() noexcept {
  if (!IsCreated()) {
    return;
  }

  head_.store(0U, std::memory_order_relaxed);
  tail_.store(0U, std::memory_order_relaxed);
}

/*
 * 入队流程：
 * 1. 读取当前 head/tail，计算已用空间和剩余空间；
 * 2. 依据剩余空间决定本次最多可写入多少字节；
 * 3. 如果写入跨越了缓冲区末尾，则拆成两段 memcpy；
 * 4. 数据全部写完后，再以 release 语义发布新的 head。
 *
 * 这样可保证消费者在观察到新的 head 之前，数据内容已经完整落入缓冲区。
 */
uint32_t LockFreeQueueBase::Enqueue(const uint8_t *data, uint32_t length) noexcept {
  if ((!IsCreated()) || (data == nullptr) || (length == 0U)) {
    return 0U;
  }

  const uint32_t size = storageSize_;
  // 生产者自己推进 head，因此读取 head 使用 relaxed 即可。
  const uint32_t head = head_.load(std::memory_order_relaxed);
  // tail 由消费者更新，需要以 acquire 语义读取其最新值。
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  const uint32_t used = Distance(head, tail, size);
  // 保留 1 字节作为哨兵，因此满队列时 free 为 0，而不是 1。
  const uint32_t free = (size - 1U) - used;
  const uint32_t writeLength = MinU32(length, free);

  if (writeLength == 0U) {
    return 0U;
  }

  // 第一段：从当前 head 连续写到缓冲区末尾。
  const uint32_t firstLength = MinU32(writeLength, size - head);
  (void)memcpy(storage_ + head, data, firstLength);

  // 第二段：若发生回卷，则从缓冲区起始位置继续写入剩余数据。
  const uint32_t secondLength = writeLength - firstLength;
  if (secondLength > 0U) {
    (void)memcpy(storage_, data + firstLength, secondLength);
  }

  const uint32_t nextHead = (head + writeLength) % size;
  // 发布新的 head，通知消费者这些数据已经可读。
  head_.store(nextHead, std::memory_order_release);
  return writeLength;
}

/*
 * 出队流程与入队对称：
 * 1. 读取当前 tail/head，计算当前可读数据量；
 * 2. 依据可读数据量决定本次最多可取出多少字节；
 * 3. 如遇缓冲区回卷，同样分两段 memcpy；
 * 4. 数据复制到用户缓冲区后，再以 release 语义推进 tail。
 */
uint32_t LockFreeQueueBase::Dequeue(uint8_t *data, uint32_t length) noexcept {
  if ((!IsCreated()) || (data == nullptr) || (length == 0U)) {
    return 0U;
  }

  const uint32_t size = storageSize_;
  while (true) {
    // Multiple consumers may race on tail_. Copy first, then claim the range
    // with CAS. If CAS fails, another consumer committed first, so retry.
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t head = head_.load(std::memory_order_acquire);
    const uint32_t used = Distance(head, tail, size);
    const uint32_t readLength = MinU32(length, used);

    if (readLength == 0U) {
      return 0U;
    }

    const uint32_t firstLength = MinU32(readLength, size - tail);
    (void)memcpy(data, storage_ + tail, firstLength);

    const uint32_t secondLength = readLength - firstLength;
    if (secondLength > 0U) {
      (void)memcpy(data + firstLength, storage_, secondLength);
    }

    const uint32_t nextTail = (tail + readLength) % size;
    uint32_t expectedTail = tail;
    if (tail_.compare_exchange_weak(expectedTail, nextTail,
                                    std::memory_order_acq_rel,
                                    std::memory_order_acquire)) {
      return readLength;
    }
  }
}

/*
 * UsedSize 返回当前逻辑上可读的数据量。
 * 这里同时以 acquire 语义读取 head 和 tail，确保观察到一致的已发布状态。
 */
uint32_t LockFreeQueueBase::UsedSize() const noexcept {
  if (!IsCreated()) {
    return 0U;
  }

  const uint32_t head = head_.load(std::memory_order_acquire);
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  return Distance(head, tail, storageSize_);
}

/*
 * 剩余空间 = 可用容量 - 已使用空间。
 * 可用容量不是 storageSize_，而是 storageSize_ - 1，
 * 因为内部始终保留一个哨兵字节用于判空/判满。
 */
uint32_t LockFreeQueueBase::FreeSize() const noexcept {
  const uint32_t capacity = Capacity();
  const uint32_t used = UsedSize();
  return (used < capacity) ? (capacity - used) : 0U;
}

/*
 * 返回用户真正可以使用的容量。
 * 例如底层缓冲区大小为 256，则实际最多只能存放 255 字节。
 */
uint32_t LockFreeQueueBase::Capacity() const noexcept {
  return (storageSize_ > 0U) ? (storageSize_ - 1U) : 0U;
}

/* 返回底层原始缓冲区大小。 */
uint32_t LockFreeQueueBase::StorageSize() const noexcept {
  return storageSize_;
}

/* 只要缓冲区存在且大小合法，就认为队列已创建。 */
bool LockFreeQueueBase::IsCreated() const noexcept {
  return (storage_ != nullptr) && (storageSize_ >= 2U);
}

/* 已用空间为 0 时，说明队列为空。 */
bool LockFreeQueueBase::IsEmpty() const noexcept {
  return UsedSize() == 0U;
}

/* 剩余空间为 0 时，说明队列已满。 */
bool LockFreeQueueBase::IsFull() const noexcept {
  return FreeSize() == 0U;
}

/*
 * 查询当前平台上 std::atomic<uint32_t> 是否采用无锁实现。
 * 该函数更偏向平台能力探测，便于上层决定是否接受当前实现方式。
 */
bool LockFreeQueueBase::IsLockFree() const noexcept {
  return head_.is_lock_free() && tail_.is_lock_free();
}

/* 返回可写底层缓冲区指针。 */
uint8_t *LockFreeQueueBase::Buffer() noexcept {
  return storage_;
}

/* 返回只读底层缓冲区指针。 */
const uint8_t *LockFreeQueueBase::Buffer() const noexcept {
  return storage_;
}

DynamicLockFreeQueue::DynamicLockFreeQueue(uint32_t storageSize) noexcept {
  (void)Recreate(storageSize);
}

DynamicLockFreeQueue::~DynamicLockFreeQueue() {
  ReleaseStorage();
}

bool DynamicLockFreeQueue::Recreate(uint32_t storageSize) noexcept {
  if (storageSize < 2U) {
    ReleaseStorage();
    return false;
  }

  uint8_t *newStorage = new (std::nothrow) uint8_t[storageSize] {};
  if (newStorage == nullptr) {
    ReleaseStorage();
    return false;
  }

  ReleaseStorage();
  ownedStorage_ = newStorage;
  return Create(ownedStorage_, storageSize);
}

void DynamicLockFreeQueue::ReleaseStorage() noexcept {
  Delete();
  delete[] ownedStorage_;
  ownedStorage_ = nullptr;
}

} // namespace iFly
