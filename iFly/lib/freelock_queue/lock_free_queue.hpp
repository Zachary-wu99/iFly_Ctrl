// 无锁队列接口与模板封装。
// 提供静态/动态字节队列，作为串口和协议模块的基础缓冲组件。
#ifndef IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP
#define IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP

#include <atomic>
#include <new>
#include <stddef.h>
#include <stdint.h>

namespace iFly {

// 并发约束：
// - `Enqueue()` 仅保证单生产者安全。
// - `Dequeue()` 允许多个消费者并发竞争。
// - 每次成功出队都会独占一段不重复的字节区间。
/**
 * @brief 单生产者/单消费者无锁字节队列基类。
 *
 * @details
 * - 队列底层使用环形缓冲区实现，头指针负责写入，尾指针负责读取。
 * - 存储空间完全由调用方提供，不在类内部申请或释放动态内存。
 * - 为了区分“队列为空”和“队列已满”两种状态，内部固定保留 1 字节哨兵空间。
 * - 该实现面向单生产者、单消费者并发场景，例如“中断写入 + 主循环读取”或
 *   “主循环写入 + 中断读取”。
 * - 通过 std::atomic 维护读写索引，不依赖操作系统锁，也不依赖特定 MCU 接口。
 */
class LockFreeQueueBase {
public:
  LockFreeQueueBase() = default;
  ~LockFreeQueueBase() = default;

  /** @brief 禁止拷贝，避免多个对象误共享同一组队列状态。 */
  LockFreeQueueBase(const LockFreeQueueBase &) = delete;
  /** @brief 禁止赋值，避免外部无意覆盖原子索引和缓冲区绑定关系。 */
  LockFreeQueueBase &operator=(const LockFreeQueueBase &) = delete;

  /**
   * @brief 使用外部缓冲区创建队列。
   *
   * @param buffer 由调用方提供的静态或全局字节缓冲区首地址。
   * @param bufferSize 缓冲区总大小，单位为字节，至少为 2。
   * @return 创建成功返回 true，参数非法时返回 false。
   *
   * @note
   * - 本函数不会分配内存，只会记录缓冲区地址并重置读写索引。
   * - 由于会保留 1 字节用于判空/判满，真实可用容量为 bufferSize - 1。
   */
  bool Create(uint8_t *buffer, uint32_t bufferSize);
  /**
   * @brief 删除队列并解除与外部缓冲区的绑定关系。
   *
   * @note
   * - 该函数不会释放任何内存，因为缓冲区所有权始终属于调用方。
   * - 调用后对象回到“未创建”状态，后续需重新调用 Create 才能继续使用。
   */
  void Delete();
  /**
   * @brief 清空队列中的所有数据。
   *
   * @note
   * - 仅重置头尾索引，不擦除底层缓冲区原有内容。
   * - 清空后队列逻辑上为空，后续写入会从索引 0 重新开始。
   */
  void Clear();

  /**
   * @brief 向队列中写入数据。
   *
   * @param data 待入队的数据首地址。
   * @param length 期望写入的字节数。
   * @return 实际成功写入的字节数。
   *
   * @note
   * - 当剩余空间不足时，函数会执行“尽力写入”，只写入可容纳的部分。
   * - 若队列未创建、data 为 nullptr 或 length 为 0，则直接返回 0。
   */
  uint32_t Enqueue(const uint8_t *data, uint32_t length);
  /**
   * @brief 从队列中读出数据。
   *
   * @param data 用于接收出队数据的缓冲区首地址。
   * @param length 期望读出的字节数。
   * @return 实际成功读出的字节数。
   *
   * @note
   * - 当队列内数据不足时，函数会执行“尽力读取”，只返回当前已有的数据。
   * - 若队列未创建、data 为 nullptr 或 length 为 0，则直接返回 0。
   */
  // 允许多个消费者并发读取，内部通过 CAS 竞争推进 tail。
  uint32_t Dequeue(uint8_t *data, uint32_t length);

  /** @brief 返回当前已使用的空间大小，单位为字节。 */
  uint32_t UsedSize() const;
  /** @brief 返回当前剩余可写空间大小，单位为字节。 */
  uint32_t FreeSize() const;
  /** @brief 返回队列可用容量，等于底层缓冲区大小减 1。 */
  uint32_t Capacity() const;
  /** @brief 返回底层缓冲区总大小，包含内部保留的 1 字节。 */
  uint32_t StorageSize() const;

  /** @brief 判断队列当前是否已经创建成功。 */
  bool IsCreated() const;
  /** @brief 判断队列是否为空。 */
  bool IsEmpty() const;
  /** @brief 判断队列是否已满。 */
  bool IsFull() const;
  /**
   * @brief 判断当前平台上的索引原子操作是否为真正无锁实现。
   *
   * @note
   * - 返回 true 表示底层原子索引在当前工具链/架构上可无锁执行。
   * - 返回 false 不代表功能不可用，只表示编译器可能退化为内部辅助实现。
   */
  bool IsLockFree() const;

protected:
  /** @brief 获取底层缓冲区指针，供派生类按需访问。 */
  uint8_t *Buffer();
  /** @brief 获取只读底层缓冲区指针，供派生类按需访问。 */
  const uint8_t *Buffer() const;

private:
  /** @brief 返回两个 32 位无符号整数中的较小值。 */
  static uint32_t MinU32(uint32_t left, uint32_t right);
  /**
   * @brief 计算环形缓冲区中 head 到 tail 之间的有效数据长度。
   *
   * @param head 当前写指针。
   * @param tail 当前读指针。
   * @param size 环形缓冲区总大小。
   * @return 当前已使用的数据量。
   */
  static uint32_t Distance(uint32_t head, uint32_t tail, uint32_t size);

private:
  /** @brief 调用方提供的底层缓冲区首地址。 */
  uint8_t *storage_ = nullptr;
  /** @brief 底层缓冲区总大小。 */
  uint32_t storageSize_ = 0U;
  /** @brief 写指针，由生产者推进。 */
  std::atomic<uint32_t> head_ {0U};
  /** @brief 读指针，由消费者推进。 */
  std::atomic<uint32_t> tail_ {0U};
};

/**
 * @brief 带内部静态存储的无锁队列封装。
 *
 * @tparam kStorageSize 底层字节缓冲区总大小。
 *
 * @details
 * - 该模板把缓冲区直接放在对象内部，适合裸机或资源受限场景。
 * - 构造时会自动完成 Create，因此对象创建后即可直接使用。
 */
template <uint32_t kStorageSize>
class StaticLockFreeQueue : public LockFreeQueueBase {
public:
  static_assert(kStorageSize >= 2U, "kStorageSize must be at least 2 bytes.");

  /** @brief 构造时自动绑定内部静态缓冲区。 */
  StaticLockFreeQueue() {
    (void)Create(storage_, kStorageSize);
  }

  /** @brief 重新初始化队列状态，并重新绑定内部缓冲区。 */
  void Recreate() {
    (void)Create(storage_, kStorageSize);
  }

private:
  /** @brief 对象自带的静态字节缓冲区。 */
  uint8_t storage_[kStorageSize] {};
};

/**
 * @brief 带内部动态存储的无锁队列封装。
 *
 * @details
 * - 该类会在创建阶段一次性申请底层字节缓冲区，后续收发过程中不再申请内存。
 * - 适合“容量希望在构造或初始化时配置，但运行期必须零分配”的场景。
 * - 若动态申请失败，则对象保持未创建状态，调用方可稍后再次调用 Recreate 重试。
 */
class DynamicLockFreeQueue : public LockFreeQueueBase {
public:
  /** @brief 默认构造，此时尚未申请底层缓冲区。 */
  DynamicLockFreeQueue() = default;

  /**
   * @brief 构造时按指定大小申请底层缓冲区。
   *
   * @param storageSize 底层总存储大小，至少为 2。
   */
  explicit DynamicLockFreeQueue(uint32_t storageSize);

  /** @brief 析构时释放对象持有的底层缓冲区。 */
  ~DynamicLockFreeQueue();

  /** @brief 禁止拷贝，避免多个对象共享同一块动态缓冲区。 */
  DynamicLockFreeQueue(const DynamicLockFreeQueue &) = delete;
  /** @brief 禁止赋值，避免破坏缓冲区所有权。 */
  DynamicLockFreeQueue &operator=(const DynamicLockFreeQueue &) = delete;

  /**
   * @brief 重新创建队列并重新申请底层缓冲区。
   *
   * @param storageSize 底层总存储大小，至少为 2。
   * @return 创建成功返回 true，申请失败或参数非法返回 false。
   *
   * @note
   * - 若对象之前已经持有旧缓冲区，会先释放旧缓冲区再申请新缓冲区。
   * - 该函数设计用于初始化阶段调用，运行中不建议频繁重建。
   */
  bool Recreate(uint32_t storageSize);

private:
  /** @brief 释放当前持有的动态缓冲区，并把对象恢复为未创建状态。 */
  void ReleaseStorage();

private:
  /** @brief 当前对象持有的动态缓冲区首地址。 */
  uint8_t *ownedStorage_ = nullptr;
};

} // namespace iFly

#endif /* IFLY_FREELOCK_QUEUE_LOCK_FREE_QUEUE_HPP */
