#ifndef IFLY_MSG_HPP
#define IFLY_MSG_HPP

#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 轻量级、面向裸机的 uORB 风格 Topic。
 *
 * @tparam MessageType      消息类型。
 * @tparam kMaxSubscribers  该 Topic 最多允许多少个订阅者同时挂接。
 * @tparam kQueueDepth      每个订阅者本地缓存多少条消息样本。
 *
 * @details
 * 这份实现借鉴了 PX4 uORB 的“按 Topic 发布 / 订阅”思想，但为了适配当前
 * STM32 裸机工程，做了几项工程化取舍：
 *
 * 1. 不做全局设备节点注册，而是直接通过 C++ 静态对象定义 Topic。
 * 2. 不依赖动态内存。订阅者表和每个订阅者的消息队列都在编译期定长。
 * 3. 每个订阅者拥有独立的无锁队列，因此同一条消息会被 fan-out 到每个订阅者。
 * 4. 队列底层仍复用现有 `StaticLockFreeQueue`，但 Topic 的发布与挂接过程
 *    会用一个很短的自旋保护来串行化，避免破坏底层队列“单生产者”约束。
 *
 * 换句话说：
 * - “每个订阅者自己的队列”这一层是基于无锁队列工作的；
 * - “Topic 统一分发到多个订阅者”这一层不是纯 lock-free，而是短临界区串行化。
 *
 * 这种结构很适合当前工程：
 * - 无 RTOS；
 * - 无堆内存依赖；
 * - 需要 ISR / 主循环之间较轻量地传递结构化消息；
 * - 希望接口风格接近 uORB，而不是简单的字节流 FIFO。
 */
template <typename MessageType, uint8_t kMaxSubscribers = 8U, uint32_t kQueueDepth = 4U>
class MsgTopic final {
  static_assert(kMaxSubscribers > 0U, "kMaxSubscribers must be greater than 0.");
  static_assert(kQueueDepth > 0U, "kQueueDepth must be greater than 0.");
  static_assert(std::is_standard_layout<MessageType>::value,
                "MessageType must be standard-layout.");
  static_assert(std::is_trivially_copyable<MessageType>::value,
                "MessageType must be trivially copyable.");
  static_assert(std::is_default_constructible<MessageType>::value,
                "MessageType must be default constructible.");

public:
  /**
   * @brief 队列内部实际存放的样本格式。
   *
   * @details
   * 每次发布时，不是只把 `MessageType` 本体放进队列，而是额外附带一个
   * `generation` 序号。这样订阅者就能：
   *
   * - 判断是否有新数据；
   * - 获取当前读取到的是第几代消息；
   * - 模拟 uORB 里“更新计数”的能力。
   */
  struct Sample final {
    uint32_t generation = 0U;
    MessageType message {};
  };

  /**
   * @brief 订阅端对象。
   *
   * @details
   * 一个 `Subscription` 在任意时刻最多只挂接一个 Topic。
   * 它内部维护：
   *
   * - 自己独立的消息队列；
   * - 最近一次“收到”的 generation；
   * - 最近一次“消费”的 generation；
   * - 因队列满而丢失的样本计数。
   *
   * 因为每个订阅者有自己的队列，所以多个订阅者之间不会互相抢消息。
   */
  class Subscription final {
  public:
    /** @brief 默认构造。此时尚未订阅任何 Topic。 */
    Subscription(MsgTopic &topic) noexcept{

    }

    /**
     * @brief 析构时自动退订。
     *
     * @details
     * 这样可避免对象生命周期结束后，Topic 中仍残留悬空指针。
     */
    ~Subscription() {
      Unsubscribe();
    }

    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;

    /**
     * @brief 订阅指定 Topic。
     * @param topic 目标 Topic。
     * @return 订阅成功返回 true；如果订阅槽已满则返回 false。
     *
     * @details
     * 若当前已经订阅了别的 Topic，会先退订旧 Topic，再尝试挂到新 Topic。
     */
    bool Subscribe(MsgTopic &topic) noexcept {
      MsgTopic *const current = topic_.load(std::memory_order_acquire);
      if (current == &topic) {
        return true;
      }

      if (current != nullptr) {
        Unsubscribe();
      }

      return topic.AttachSubscription(*this);
    }

    /**
     * @brief 退订当前 Topic。
     *
     * @details
     * 退订后会清空本地缓存、清零 generation 记录和丢包计数。
     */
    void Unsubscribe() noexcept {
      MsgTopic *const current = topic_.load(std::memory_order_acquire);
      if (current != nullptr) {
        current->DetachSubscription(*this);
      }
    }

    /** @brief 判断当前是否已经成功订阅某个 Topic。 */
    bool IsSubscribed() const noexcept {
      return topic_.load(std::memory_order_acquire) != nullptr;
    }

    /**
     * @brief 判断是否存在“尚未被当前订阅者消费”的新消息。
     *
     * @details
     * 这里比较的是：
     * - 最近一次发布到该订阅者队列的 generation
     * - 最近一次被该订阅者成功 Copy 出去的 generation
     */
    bool Updated() const noexcept {
      return LastPublishedGeneration() != LastConsumedGeneration();
    }

    /** @brief 读取一条最旧的待处理消息。 */
    bool Copy(MessageType &message) noexcept {
      return Copy(message, nullptr);
    }

    /**
     * @brief 读取一条最旧的待处理消息，并返回其 generation。
     * @param message    输出消息。
     * @param generation 若非空，返回该消息所属的 generation。
     * @return 成功取到一条消息返回 true；无数据返回 false。
     *
     * @details
     * 该接口等价于“顺序消费队列头部的一条消息”。
     * 如果业务希望逐条处理每一份样本，用这个接口最合适。
     */
    bool Copy(MessageType &message, uint32_t *generation) noexcept {
      Sample sample {};
      if (!TryDequeueSample(sample)) {
        return false;
      }

      message = sample.message;
      lastConsumedGeneration_.store(sample.generation, std::memory_order_release);

      if (generation != nullptr) {
        *generation = sample.generation;
      }

      return true;
    }

    /** @brief 连续丢弃旧消息，只保留并返回最新一条。 */
    bool CopyLatest(MessageType &message) noexcept {
      return CopyLatest(message, nullptr);
    }

    /**
     * @brief 读取当前缓存中的最新一条消息，并清空更旧的待处理样本。
     * @param message    输出最新消息。
     * @param generation 若非空，返回最新消息所属的 generation。
     * @return 成功取到消息返回 true；无数据返回 false。
     *
     * @details
     * 对于姿态、传感器等“只关心最新状态”的业务，这个接口比 `Copy()` 更方便。
     */
    bool CopyLatest(MessageType &message, uint32_t *generation) noexcept {
      Sample latest {};
      bool gotSample = false;

      while (TryDequeueSample(latest)) {
        gotSample = true;
      }

      if (!gotSample) {
        return false;
      }

      message = latest.message;
      lastConsumedGeneration_.store(latest.generation, std::memory_order_release);

      if (generation != nullptr) {
        *generation = latest.generation;
      }

      return true;
    }

    /**
     * @brief 返回当前本地队列里还积压了多少条完整消息。
     *
     * @details
     * 队列底层按字节计数，这里换算成“样本条数”返回给业务层。
     */
    uint32_t PendingCount() const noexcept {
      return queue_.UsedSize() / kSampleSize;
    }

    /** @brief 返回该订阅者因队列满而丢失的样本数量。 */
    uint32_t LostCount() const noexcept {
      return lostCount_.load(std::memory_order_acquire);
    }

    /** @brief 返回最近一次成功推送到该订阅者的 generation。 */
    uint32_t LastPublishedGeneration() const noexcept {
      return lastPublishedGeneration_.load(std::memory_order_acquire);
    }

    /** @brief 返回最近一次被该订阅者成功消费的 generation。 */
    uint32_t LastConsumedGeneration() const noexcept {
      return lastConsumedGeneration_.load(std::memory_order_acquire);
    }

  private:
    friend class MsgTopic;

    /** @brief 单个样本在队列中占用的字节数。 */
    static constexpr uint32_t kSampleSize = sizeof(Sample);
    /**
     * @brief 底层字节队列总长度。
     *
     * @details
     * `StaticLockFreeQueue` 内部会保留 1 字节用于区分“空/满”，所以这里要额外 +1。
     */
    static constexpr uint32_t kQueueStorageSize = (kSampleSize * kQueueDepth) + 1U;

    /**
     * @brief 向当前订阅者的本地队列压入一个样本。
     * @return 成功压入返回 true。
     *
     * @details
     * 如果队列已满，这里采用“丢最旧、保最新”的策略：
     *
     * 1. 先尝试直接入队；
     * 2. 若失败，则先弹掉一条最旧样本；
     * 3. 然后再次尝试把最新样本写进去。
     *
     * 这样比较符合姿态/传感器等实时主题“宁可丢旧数据，也尽量保留最新数据”的习惯。
     */
    bool PushSample(const MessageType &message, uint32_t generation) noexcept {
      Sample sample {};
      sample.generation = generation;
      sample.message = message;

      if (!TryEnqueueSample(sample)) {
        Sample dropped {};
        if (TryDequeueSample(dropped)) {
          (void)lostCount_.fetch_add(1U, std::memory_order_relaxed);
        }

        if (!TryEnqueueSample(sample)) {
          return false;
        }
      }

      lastPublishedGeneration_.store(generation, std::memory_order_release);
      return true;
    }

    /**
     * @brief 将订阅者运行时状态重置到初始状态。
     *
     * @details
     * 退订、重新订阅时都会调用这里，确保不会残留旧 Topic 的数据和计数。
     */
    void ResetRuntimeState() noexcept {
      queue_.Clear();
      lastPublishedGeneration_.store(0U, std::memory_order_release);
      lastConsumedGeneration_.store(0U, std::memory_order_release);
      lostCount_.store(0U, std::memory_order_release);
    }

    /**
     * @brief 尝试把一个完整样本压入字节队列。
     *
     * @details
     * 这里先检查 `FreeSize()`，确保剩余空间足够一个完整样本，避免出现
     * “样本只写进去一半”的情况。
     */
    bool TryEnqueueSample(const Sample &sample) noexcept {
      if (queue_.FreeSize() < kSampleSize) {
        return false;
      }

      return queue_.Enqueue(reinterpret_cast<const uint8_t *>(&sample), kSampleSize) == kSampleSize;
    }

    /**
     * @brief 尝试从字节队列取出一个完整样本。
     *
     * @details
     * 这里先检查 `UsedSize()`，确保队列里至少有一整条样本，避免出现半样本读取。
     */
    bool TryDequeueSample(Sample &sample) noexcept {
      if (queue_.UsedSize() < kSampleSize) {
        return false;
      }

      return queue_.Dequeue(reinterpret_cast<uint8_t *>(&sample), kSampleSize) == kSampleSize;
    }

  private:
    /** @brief 当前订阅者自己的本地消息队列。 */
    StaticLockFreeQueue<kQueueStorageSize> queue_ {};
    /** @brief 当前订阅者挂接到哪个 Topic；空指针表示未订阅。 */
    std::atomic<MsgTopic *> topic_ {nullptr};
    /** @brief 最近一次成功发布到该订阅者的 generation。 */
    std::atomic<uint32_t> lastPublishedGeneration_ {0U};
    /** @brief 最近一次成功被该订阅者消费的 generation。 */
    std::atomic<uint32_t> lastConsumedGeneration_ {0U};
    /** @brief 队列满时被挤掉的旧样本计数。 */
    std::atomic<uint32_t> lostCount_ {0U};
  };

  /**
   * @brief 发布端包装。
   *
   * @details
   * 这个类的作用主要是让业务代码写起来更像 uORB：
   * - 持有一个 `Publication`
   * - 然后调用 `Publish(message)`
   */
  class Publication final {
  public:
    /** @brief 构造时绑定到一个 Topic。 */
    explicit Publication(MsgTopic &topic) noexcept : topic_(topic) {
    }

    /** @brief 发布一条消息。 */
    bool Publish(const MessageType &message) const noexcept {
      return topic_.Publish(message);
    }

    /** @brief 返回当前 Topic 已经发布到第几代。 */
    uint32_t Generation() const noexcept {
      return topic_.Generation();
    }

    /** @brief 返回 Topic 名称，仅用于调试或日志。 */
    const char *Name() const noexcept {
      return topic_.Name();
    }

  private:
    MsgTopic &topic_;
  };

  /**
   * @brief 构造一个 Topic。
   * @param name Topic 名称，可空，仅用于调试展示。
   */
  explicit MsgTopic(const char *name = nullptr) noexcept : name_(name) {
    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      subscribers_[index] = nullptr;
    }
  }

  MsgTopic(const MsgTopic &) = delete;
  MsgTopic &operator=(const MsgTopic &) = delete;

  /**
   * @brief 向该 Topic 发布一条消息。
   * @param message 待发布的消息。
   * @return 成功返回 true；如果发布路径正忙返回 false。
   *
   * @details
   * 发布流程如下：
   *
   * 1. 先尝试进入一个很短的串行化保护区；
   * 2. 生成新的 generation；
   * 3. 遍历所有订阅者，把样本 fan-out 到每个订阅者自己的队列；
   * 4. 退出保护区。
   *
   * 这里如果保护区已被占用，会直接失败并增加 `busyPublishCount_`，
   * 而不是阻塞等待。这样更适合中断或主循环里的轻量发布场景。
   */
  bool Publish(const MessageType &message) noexcept {
    if (!TryAcquireGuard()) {
      (void)busyPublishCount_.fetch_add(1U, std::memory_order_relaxed);
      return false;
    }

    uint32_t nextGeneration = generation_.load(std::memory_order_relaxed) + 1U;
    if (nextGeneration == 0U) {
      nextGeneration = 1U;
    }
    generation_.store(nextGeneration, std::memory_order_release);

    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      Subscription *const subscriber = subscribers_[index];
      if (subscriber != nullptr) {
        (void)subscriber->PushSample(message, nextGeneration);
      }
    }

    ReleaseGuard();
    return true;
  }

  /** @brief 返回 Topic 名称。 */
  const char *Name() const noexcept {
    return name_;
  }

  /** @brief 返回当前 Topic 最近一次发布的 generation。 */
  uint32_t Generation() const noexcept {
    return generation_.load(std::memory_order_acquire);
  }

  /**
   * @brief 返回当前有效订阅者数量。
   *
   * @details
   * 由于订阅表可能在别处被修改，这里进入短保护区后再统计。
   */
  uint8_t SubscriberCount() const noexcept {
    AcquireGuard();

    uint8_t count = 0U;
    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      if (subscribers_[index] != nullptr) {
        ++count;
      }
    }

    ReleaseGuard();
    return count;
  }

  /**
   * @brief 返回“发布时 Topic 正忙而失败”的累计次数。
   *
   * @details
   * 如果业务看到这里的计数不断增长，说明存在多个上下文同时发布同一 Topic，
   * 或者某个发布路径过于频繁，需要重新审视调度方式。
   */
  uint32_t BusyPublishCount() const noexcept {
    return busyPublishCount_.load(std::memory_order_acquire);
  }

private:
  /**
   * @brief 把一个订阅者挂到当前 Topic。
   * @return 成功返回 true，订阅槽已满返回 false。
   */
  bool AttachSubscription(Subscription &subscription) noexcept {
    AcquireGuard();

    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      if (subscribers_[index] == &subscription) {
        ReleaseGuard();
        return true;
      }
    }

    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      if (subscribers_[index] == nullptr) {
        subscription.ResetRuntimeState();
        subscription.topic_.store(this, std::memory_order_release);
        subscribers_[index] = &subscription;
        ReleaseGuard();
        return true;
      }
    }

    ReleaseGuard();
    return false;
  }

  /**
   * @brief 将一个订阅者从当前 Topic 中移除。
   *
   * @details
   * 移除后会把该订阅者状态彻底清理，避免残留旧数据影响后续重新订阅。
   */
  void DetachSubscription(Subscription &subscription) noexcept {
    AcquireGuard();

    for (uint8_t index = 0U; index < kMaxSubscribers; ++index) {
      if (subscribers_[index] == &subscription) {
        subscribers_[index] = nullptr;
        break;
      }
    }

    subscription.topic_.store(nullptr, std::memory_order_release);
    subscription.ResetRuntimeState();
    ReleaseGuard();
  }

  /**
   * @brief 尝试获取 Topic 保护标志。
   * @return 获取成功返回 true；已被占用返回 false。
   */
  bool TryAcquireGuard() const noexcept {
    return !guard_.test_and_set(std::memory_order_acquire);
  }

  /**
   * @brief 自旋等待直到进入保护区。
   *
   * @details
   * 该保护区很短，只包围订阅表修改和一次发布分发。
   */
  void AcquireGuard() const noexcept {
    while (guard_.test_and_set(std::memory_order_acquire)) {
    }
  }

  /** @brief 释放保护区。 */
  void ReleaseGuard() const noexcept {
    guard_.clear(std::memory_order_release);
  }

private:
  /** @brief Topic 名称，仅用于调试和日志。 */
  const char *name_ = nullptr;
  /** @brief 订阅者表，空指针表示该槽位空闲。 */
  Subscription *subscribers_[kMaxSubscribers] {};
  /** @brief Topic 全局发布序号。每发布一条消息就递增一次。 */
  std::atomic<uint32_t> generation_ {0U};
  /** @brief 因 Topic 正忙导致发布失败的次数。 */
  std::atomic<uint32_t> busyPublishCount_ {0U};
  /** @brief 短临界区保护标志。 */
  mutable std::atomic_flag guard_ = ATOMIC_FLAG_INIT;
};

} // namespace iFly

#endif /* IFLY_MSG_HPP */
