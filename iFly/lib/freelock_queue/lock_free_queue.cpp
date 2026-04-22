#include "lock_free_queue.hpp"

#include <new>
#include <string.h>

namespace iFly {

uint32_t LockFreeQueueBase::Distance(uint32_t head, uint32_t tail, uint32_t size) {
  if (head >= tail) {
    return head - tail;
  }

  return size - (tail - head);
}

bool LockFreeQueueBase::Create(uint8_t *buffer, uint32_t bufferSize) {
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

void LockFreeQueueBase::Delete() {
  head_.store(0U, std::memory_order_relaxed);
  tail_.store(0U, std::memory_order_relaxed);
  storage_ = nullptr;
  storageSize_ = 0U;
}

void LockFreeQueueBase::Clear() {
  if (!IsCreated()) {
    return;
  }

  head_.store(0U, std::memory_order_relaxed);
  tail_.store(0U, std::memory_order_relaxed);
}

uint32_t LockFreeQueueBase::Enqueue(const uint8_t *data, uint32_t length) {
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
  const uint32_t writeLength = usermath::Min<uint32_t>(length, free);

  if (writeLength == 0U) {
    return 0U;
  }

  // 第一段：从当前 head 连续写到缓冲区末尾。
  const uint32_t firstLength = usermath::Min<uint32_t>(writeLength, size - head);
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

uint32_t LockFreeQueueBase::Dequeue(uint8_t *data, uint32_t length) {
  if ((!IsCreated()) || (data == nullptr) || (length == 0U)) {
    return 0U;
  }

  const uint32_t size = storageSize_;
  while (true) {
    // 多个消费者可能同时竞争 tail_。
    // 先把数据复制出来，再用 CAS 认领本次读取区间；如果失败，说明别的消费者先提交了，继续重试即可。
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    const uint32_t head = head_.load(std::memory_order_acquire);
    const uint32_t used = Distance(head, tail, size);
    const uint32_t readLength = usermath::Min<uint32_t>(length, used);

    if (readLength == 0U) {
      return 0U;
    }

    const uint32_t firstLength = usermath::Min<uint32_t>(readLength, size - tail);
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

uint32_t LockFreeQueueBase::UsedSize() const {
  if (!IsCreated()) {
    return 0U;
  }

  const uint32_t head = head_.load(std::memory_order_acquire);
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  return Distance(head, tail, storageSize_);
}

uint32_t LockFreeQueueBase::FreeSize() const {
  const uint32_t capacity = Capacity();
  const uint32_t used = UsedSize();
  return (used < capacity) ? (capacity - used) : 0U;
}

uint32_t LockFreeQueueBase::Capacity() const {
  return (storageSize_ > 0U) ? (storageSize_ - 1U) : 0U;
}

uint32_t LockFreeQueueBase::StorageSize() const {
  return storageSize_;
}

bool LockFreeQueueBase::IsCreated() const {
  return (storage_ != nullptr) && (storageSize_ >= 2U);
}

bool LockFreeQueueBase::IsEmpty() const {
  return UsedSize() == 0U;
}

bool LockFreeQueueBase::IsFull() const {
  return FreeSize() == 0U;
}

bool LockFreeQueueBase::IsLockFree() const {
  return head_.is_lock_free() && tail_.is_lock_free();
}

uint8_t *LockFreeQueueBase::Buffer() {
  return storage_;
}

const uint8_t *LockFreeQueueBase::Buffer() const {
  return storage_;
}

DynamicLockFreeQueue::DynamicLockFreeQueue(uint32_t storageSize) {
  (void)Recreate(storageSize);
}

DynamicLockFreeQueue::~DynamicLockFreeQueue() {
  ReleaseStorage();
}

bool DynamicLockFreeQueue::Recreate(uint32_t storageSize) {
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

void DynamicLockFreeQueue::ReleaseStorage() {
  Delete();
  delete[] ownedStorage_;
  ownedStorage_ = nullptr;
}

} // namespace iFly
