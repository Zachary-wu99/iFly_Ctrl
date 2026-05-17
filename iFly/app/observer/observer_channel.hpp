/**
 * @file observer_channel.hpp
 * @brief 观察者通道模板接口。
 */
#ifndef IFLY_APP_OBSERVER_OBSERVER_CHANNEL_HPP
#define IFLY_APP_OBSERVER_OBSERVER_CHANNEL_HPP

#include <array>
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <type_traits>
#include <utility>

namespace iFly {

/**
 * @brief 订阅者读取消息时的结果状态。
 */
enum class ConsumeStatus : uint8_t {
  kNoData = 0U, /**< 当前没有可读数据。 */
  kOk, /**< 成功读取到一条有效数据。 */
  kOverflowed, /**< 读取时发生历史覆盖。 */
};

/**
 * @brief 面向单发布者、多消费者场景的无锁快照通道。
 *
 * @tparam T 消息类型。
 * @tparam kHistoryDepth 通道保留的历史条数。
 * @tparam kMaxConsumers 同时允许存在的消费者数量。
 */
template <typename T, uint32_t kHistoryDepth = 4U, uint32_t kMaxConsumers = 4U>
class ObserverChannel {
public:
  using value_type = T; /**< 通道承载的消息类型。 */
  static constexpr uint32_t HistoryDepth = kHistoryDepth; /**< 历史消息保留深度。 */
  static constexpr uint32_t MaxConsumers = kMaxConsumers; /**< 最大消费者数量。 */

  /**
   * @brief 订阅者句柄。
   *
   * @details
   * 每个消费者维护独立的读取序号，用于按序读取或直接追最新值。
   */
  class Consumer {
  public:
    Consumer() = default;

    Consumer(const Consumer &) = delete;
    Consumer &operator=(const Consumer &) = delete;

    Consumer(Consumer &&other) noexcept {
      MoveFrom(other);
    }

    Consumer &operator=(Consumer &&other) noexcept {
      if (this != &other) {
        Release();
        MoveFrom(other);
      }
      return *this;
    }

    ~Consumer() {
      Release();
    }

    /**
     * @brief 读取下一条按序消息。
     *
     * @param out 输出消息对象。
     * @param sequence 输出读取到的消息序号，可为 `nullptr`。
     * @return 读取结果状态。
     */
    ConsumeStatus TryRead(T &out, uint32_t *sequence = nullptr) {
      if (channel_ == nullptr) {
        return ConsumeStatus::kNoData;
      }

      return channel_->TryReadImpl(nextSequence_, false, out, sequence);
    }

    /**
     * @brief 读取当前最新一条消息。
     *
     * @param out 输出消息对象。
     * @param sequence 输出读取到的消息序号，可为 `nullptr`。
     * @return 读取结果状态。
     */
    ConsumeStatus TryReadLatest(T &out, uint32_t *sequence = nullptr) {
      if (channel_ == nullptr) {
        return ConsumeStatus::kNoData;
      }

      return channel_->TryReadImpl(nextSequence_, true, out, sequence);
    }

    /**
     * @brief 把读取位置重置到当前最新消息。
     */
    void ResetToLatest() {
      if (channel_ == nullptr) {
        return;
      }

      const uint32_t published =
          channel_->publishedSequence_.load(std::memory_order_acquire);
      nextSequence_ = (published == 0U) ? 1U : published;
    }

    /**
     * @brief 把读取位置重置到下一次发布。
     */
    void ResetToNextPublication() {
      if (channel_ == nullptr) {
        return;
      }

      const uint32_t published =
          channel_->publishedSequence_.load(std::memory_order_acquire);
      nextSequence_ = published + 1U;
    }

    /**
     * @brief 判断当前消费者是否仍附着在通道上。
     *
     * @return 已附着返回 `true`。
     */
    bool IsAttached() const {
      return channel_ != nullptr;
    }

  private:
    friend class ObserverChannel;

    Consumer(ObserverChannel *channel, uint32_t registrationIndex,
             uint32_t nextSequence)
        : channel_(channel), registrationIndex_(registrationIndex),
          nextSequence_(nextSequence) {}

    void Release() {
      if ((channel_ != nullptr) && (registrationIndex_ < kMaxConsumers)) {
        channel_->registrations_[registrationIndex_].active.store(
            false, std::memory_order_release);
      }

      channel_ = nullptr;
      registrationIndex_ = kInvalidIndex;
      nextSequence_ = 1U;
    }

    void MoveFrom(Consumer &other) {
      channel_ = other.channel_;
      registrationIndex_ = other.registrationIndex_;
      nextSequence_ = other.nextSequence_;

      other.channel_ = nullptr;
      other.registrationIndex_ = kInvalidIndex;
      other.nextSequence_ = 1U;
    }

  private:
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFUL;

    ObserverChannel *channel_ = nullptr; /**< 当前关联的通道对象。 */
    uint32_t registrationIndex_ = kInvalidIndex; /**< 当前消费者注册槽位索引。 */
    uint32_t nextSequence_ = 1U; /**< 下一次尝试读取的消息序号。 */
  };

  ObserverChannel() = default;

  ObserverChannel(const ObserverChannel &) = delete;
  ObserverChannel &operator=(const ObserverChannel &) = delete;

  /**
   * @brief 发布一条消息快照。
   *
   * @tparam Message 实际传入的消息类型。
   * @param message 待发布消息。
   * @return 发布成功返回 `true`。
   */
  template <typename Message>
  bool Publish(Message &&message) {
    static_assert(std::is_same_v<std::decay_t<Message>, T>,
                  "Publish message type must match ObserverChannel::value_type.");

    const uint32_t nextSequence =
        publishedSequence_.load(std::memory_order_relaxed) + 1U;
    Slot &slot = slots_[SlotIndex(nextSequence)];
    const uint32_t stableStamp = StableStamp(nextSequence);

    slot.stamp.store(stableStamp | 0x01U, std::memory_order_release);
    slot.value = std::forward<Message>(message);
    slot.stamp.store(stableStamp, std::memory_order_release);
    publishedSequence_.store(nextSequence, std::memory_order_release);
    return true;
  }

  /**
   * @brief 原地构造并发布一条消息。
   *
   * @tparam Args 构造参数类型列表。
   * @param args 构造消息对象的参数。
   * @return 发布成功返回 `true`。
   */
  template <typename... Args>
  bool Emplace(Args &&...args) {
    T value(std::forward<Args>(args)...);
    return Publish(std::move(value));
  }

  /**
   * @brief 从当前最新消息开始订阅。
   *
   * @return 新建的消费者句柄。
   */
  Consumer SubscribeLatest() {
    return SubscribeImpl(false);
  }

  /**
   * @brief 从下一次新发布开始订阅。
   *
   * @return 新建的消费者句柄。
   */
  Consumer SubscribeFromNext() {
    return SubscribeImpl(true);
  }

  /**
   * @brief 获取当前已发布的最新序号。
   *
   * @return 当前最新发布序号。
   */
  uint32_t PublishedSequence() const {
    return publishedSequence_.load(std::memory_order_acquire);
  }

private:
  struct Registration {
    std::atomic<bool> active {false}; /**< 当前消费者槽位是否被占用。 */
  };

  struct Slot {
    std::atomic<uint32_t> stamp {0U}; /**< 当前槽位稳定标记。 */
    T value {}; /**< 当前槽位保存的消息对象。 */
  };

  static constexpr uint32_t SlotIndex(uint32_t sequence) {
    return (sequence - 1U) % kHistoryDepth;
  }

  static constexpr uint32_t StableStamp(uint32_t sequence) {
    return sequence << 1U;
  }

  static constexpr uint32_t OldestRetainedSequence(uint32_t published) {
    if (published >= kHistoryDepth) {
      return published - kHistoryDepth + 1U;
    }

    return 1U;
  }

  Consumer SubscribeImpl(bool fromNextPublication) {
    for (uint32_t i = 0U; i < kMaxConsumers; ++i) {
      bool expected = false;
      if (registrations_[i].active.compare_exchange_strong(
              expected, true, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        const uint32_t published =
            publishedSequence_.load(std::memory_order_acquire);
        const uint32_t nextSequence =
            fromNextPublication ? (published + 1U)
                                : ((published == 0U) ? 1U : published);
        return Consumer(this, i, nextSequence);
      }
    }

    return Consumer();
  }

  bool TryCopySequence(uint32_t sequence, T &out) {
    const Slot &slot = slots_[SlotIndex(sequence)];
    const uint32_t expectedStamp = StableStamp(sequence);

    for (uint32_t retry = 0U; retry < 3U; ++retry) {
      const uint32_t before = slot.stamp.load(std::memory_order_acquire);
      if (before != expectedStamp) {
        return false;
      }

      out = slot.value;

      const uint32_t after = slot.stamp.load(std::memory_order_acquire);
      if (after == expectedStamp) {
        return true;
      }
    }

    return false;
  }

  ConsumeStatus TryReadImpl(uint32_t &nextSequence, bool latestOnly, T &out,
                            uint32_t *sequence) {
    while (true) {
      const uint32_t published =
          publishedSequence_.load(std::memory_order_acquire);
      if ((published == 0U) || (nextSequence > published)) {
        return ConsumeStatus::kNoData;
      }

      const uint32_t oldest = OldestRetainedSequence(published);
      uint32_t targetSequence = latestOnly ? published : nextSequence;
      ConsumeStatus status = ConsumeStatus::kOk;

      if (targetSequence < oldest) {
        targetSequence = latestOnly ? published : oldest;
        status = ConsumeStatus::kOverflowed;
      } else if (latestOnly && (targetSequence != published)) {
        targetSequence = published;
        status = ConsumeStatus::kOverflowed;
      }

      if (!TryCopySequence(targetSequence, out)) {
        continue;
      }

      nextSequence = targetSequence + 1U;
      if (sequence != nullptr) {
        *sequence = targetSequence;
      }
      return status;
    }
  }

private:
  static_assert(kHistoryDepth > 0U,
                "ObserverChannel history depth must be greater than zero.");
  static_assert(kMaxConsumers > 0U,
                "ObserverChannel max consumer count must be greater than zero.");
  static_assert(std::is_default_constructible_v<T>,
                "ObserverChannel value type must be default constructible.");
  static_assert(std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "ObserverChannel value type must be assignable.");

  std::array<Slot, kHistoryDepth> slots_ {}; /**< 历史消息槽位数组。 */
  std::array<Registration, kMaxConsumers> registrations_ {}; /**< 消费者注册表。 */
  std::atomic<uint32_t> publishedSequence_ {0U}; /**< 最近一次成功发布的序号。 */
};

/**
 * @brief 提取回调参数类型的函数特征模板。
 *
 * @tparam Signature 函数签名类型。
 */
template <typename T>
struct FunctionTraits;

/**
 * @brief 针对函数指针 `R (*)(Arg)` 的特征提取。
 */
template <typename R, typename Arg>
struct FunctionTraits<R (*)(Arg)> {
  using argument_type = Arg; /**< 回调参数类型。 */
};

/**
 * @brief 针对函数类型 `R(Arg)` 的特征提取。
 */
template <typename R, typename Arg>
struct FunctionTraits<R(Arg)> {
  using argument_type = Arg; /**< 回调参数类型。 */
};

/**
 * @brief 针对成员函数指针 `R (C::*)(Arg)` 的特征提取。
 */
template <typename C, typename R, typename Arg>
struct FunctionTraits<R (C::*)(Arg)> {
  using argument_type = Arg; /**< 回调参数类型。 */
};

/**
 * @brief 针对常量成员函数指针 `R (C::*)(Arg) const` 的特征提取。
 */
template <typename C, typename R, typename Arg>
struct FunctionTraits<R (C::*)(Arg) const> {
  using argument_type = Arg; /**< 回调参数类型。 */
};

/**
 * @brief 推导任意可调用对象的首个参数类型。
 *
 * @tparam Callback 回调类型。
 * @tparam 未使用的 SFINAE 占位参数。
 */
template <typename Callback, typename = void>
struct CallbackArgument {
  using type =
      typename FunctionTraits<std::remove_pointer_t<std::remove_reference_t<
          Callback>>>::argument_type; /**< 推导得到的回调参数类型。 */
};

/**
 * @brief 针对仿函数和 Lambda 的参数类型推导。
 *
 * @tparam Callback 回调类型。
 */
template <typename Callback>
struct CallbackArgument<
    Callback,
    std::void_t<decltype(&std::remove_reference_t<Callback>::operator())>> {
  using type = typename FunctionTraits<
      decltype(&std::remove_reference_t<Callback>::operator())>::argument_type; /**< 推导得到的回调参数类型。 */
};

template <typename Callback>
using callback_argument_t =
    typename CallbackArgument<Callback>::type; /**< 回调参数类型别名。 */

template <typename T>
using callback_message_t =
    std::remove_cv_t<std::remove_reference_t<T>>; /**< 标准化后的消息类型别名。 */

/**
 * @brief 把消费者句柄和消息回调组合成轮询对象。
 *
 * @tparam Channel 通道类型。
 * @tparam Callback 回调类型。
 */
template <typename Channel, typename Callback>
class CallbackConsumer {
public:
  using value_type = typename Channel::value_type; /**< 当前消费者处理的消息类型。 */

  /**
   * @brief 组合一个消费者句柄和一个消息处理回调。
   *
   * @param consumer 底层消费者句柄。
   * @param callback 消息处理回调。
   */
  CallbackConsumer(typename Channel::Consumer consumer, Callback callback)
      : consumer_(std::move(consumer)), callback_(std::move(callback)) {}

  /**
   * @brief 拉取一条按序消息并立即交给回调。
   *
   * @param sequence 输出读取到的消息序号，可为 `nullptr`。
   * @return 读取结果状态。
   */
  ConsumeStatus PollOnce(uint32_t *sequence = nullptr) {
    value_type value {};
    const ConsumeStatus status = consumer_.TryRead(value, sequence);
    if (status == ConsumeStatus::kNoData) {
      return status;
    }

    callback_(value);
    return status;
  }

  /**
   * @brief 拉取当前最新消息并立即交给回调。
   *
   * @param sequence 输出读取到的消息序号，可为 `nullptr`。
   * @return 读取结果状态。
   */
  ConsumeStatus PollLatest(uint32_t *sequence = nullptr) {
    value_type value {};
    const ConsumeStatus status = consumer_.TryReadLatest(value, sequence);
    if (status == ConsumeStatus::kNoData) {
      return status;
    }

    callback_(value);
    return status;
  }

  /**
   * @brief 持续消费直到没有新数据。
   *
   * @return 本次实际处理的消息条数。
   */
  uint32_t Drain() {
    uint32_t count = 0U;
    while (PollOnce() != ConsumeStatus::kNoData) {
      ++count;
    }
    return count;
  }

  /**
   * @brief 暴露底层消费者句柄。
   *
   * @return 底层消费者句柄引用。
   */
  typename Channel::Consumer &Handle() {
    return consumer_;
  }

private:
  typename Channel::Consumer consumer_; /**< 底层消费者句柄。 */
  Callback callback_; /**< 消息处理回调。 */
};

/**
 * @brief 创建一个回调消费者对象。
 *
 * @tparam Channel 通道类型。
 * @tparam Callback 回调类型。
 * @param consumer 底层消费者句柄。
 * @param callback 消息处理回调。
 * @return 组合后的回调消费者对象。
 */
template <typename Channel, typename Callback>
CallbackConsumer<Channel, std::decay_t<Callback>> MakeCallbackConsumer(
    typename Channel::Consumer consumer, Callback &&callback) {
  return CallbackConsumer<Channel, std::decay_t<Callback>>(
      std::move(consumer), std::forward<Callback>(callback));
}

/**
 * @brief 根据消息类型在通道列表中查找索引。
 *
 * @tparam Message 目标消息类型。
 * @tparam Channels 通道类型列表。
 */
template <typename Message, typename... Channels>
struct ChannelIndex;

/**
 * @brief 递归匹配消息类型对应的通道索引。
 */
template <typename Message, typename First, typename... Rest>
struct ChannelIndex<Message, First, Rest...> {
  static constexpr size_t value =
      std::is_same_v<Message, typename First::value_type>
          ? 0U
          : 1U + ChannelIndex<Message, Rest...>::value;
};

/**
 * @brief 单通道终止条件下的消息索引匹配。
 */
template <typename Message, typename Last>
struct ChannelIndex<Message, Last> {
  static constexpr size_t value = 0U;
};

/**
 * @brief 多消息通道的统一发布与订阅中心。
 *
 * @tparam Channels 通道类型列表。
 */
template <typename... Channels>
class ObserverHub {
private:
  template <typename Message>
  static constexpr size_t MatchCount() {
    return (0U + ... +
            (std::is_same_v<Message, typename Channels::value_type> ? 1U : 0U));
  }

  template <typename Message>
  static constexpr size_t ChannelIndexValue() {
    static_assert(MatchCount<Message>() == 1U,
                  "ObserverHub requires exactly one channel for each message "
                  "type.");
    return ChannelIndex<Message, Channels...>::value;
  }

  template <typename Message>
  using channel_type_t =
      std::tuple_element_t<ChannelIndexValue<Message>(), std::tuple<Channels...>>;

  template <typename Callback>
  using callback_message_type_t =
      callback_message_t<callback_argument_t<std::decay_t<Callback>>>;

  template <typename Callback>
  using callback_channel_type_t = channel_type_t<callback_message_type_t<Callback>>;

  template <typename Callback>
  using callback_consumer_type_t =
      CallbackConsumer<callback_channel_type_t<Callback>, std::decay_t<Callback>>;

public:
  ObserverHub() = default;

  ObserverHub(const ObserverHub &) = delete;
  ObserverHub &operator=(const ObserverHub &) = delete;

  /**
   * @brief 按消息类型把数据发布到唯一匹配的通道。
   *
   * @tparam Message 消息类型。
   * @param message 待发布消息。
   * @return 发布成功返回 `true`。
   */
  template <typename Message>
  bool Publish(Message &&message) {
    using ValueType = std::decay_t<Message>;
    return Channel<ValueType>().Publish(std::forward<Message>(message));
  }

  /**
   * @brief 在目标通道内原地构造并发布消息。
   *
   * @tparam Message 目标消息类型。
   * @tparam Args 构造参数类型列表。
   * @param args 构造消息对象的参数。
   * @return 发布成功返回 `true`。
   */
  template <typename Message, typename... Args>
  bool Emplace(Args &&...args) {
    return Channel<Message>().Emplace(std::forward<Args>(args)...);
  }

  /**
   * @brief 订阅某种消息类型的最新快照。
   *
   * @tparam Message 目标消息类型。
   * @return 对应通道的消费者句柄。
   */
  template <typename Message>
  typename channel_type_t<Message>::Consumer SubscribeLatest() {
    return Channel<Message>().SubscribeLatest();
  }

  /**
   * @brief 订阅某种消息类型的下一次发布。
   *
   * @tparam Message 目标消息类型。
   * @return 对应通道的消费者句柄。
   */
  template <typename Message>
  typename channel_type_t<Message>::Consumer SubscribeFromNext() {
    return Channel<Message>().SubscribeFromNext();
  }

  /**
   * @brief 直接访问某种消息类型对应的通道对象。
   *
   * @tparam Message 目标消息类型。
   * @return 通道对象引用。
   */
  template <typename Message>
  channel_type_t<Message> &Channel() {
    constexpr size_t index = ChannelIndexValue<Message>();
    return std::get<index>(channels_);
  }

  /**
   * @brief 只读访问某种消息类型对应的通道对象。
   *
   * @tparam Message 目标消息类型。
   * @return 只读通道对象引用。
   */
  template <typename Message>
  const channel_type_t<Message> &Channel() const {
    constexpr size_t index = ChannelIndexValue<Message>();
    return std::get<index>(channels_);
  }

  /**
   * @brief 把回调绑定到对应消息类型的“最新值”订阅器。
   *
   * @tparam Callback 回调类型。
   * @param callback 消息处理回调。
   * @return 组合后的回调消费者对象。
   */
  template <typename Callback>
  callback_consumer_type_t<Callback> BindLatest(Callback &&callback) {
    using Message = callback_message_type_t<Callback>;
    using ChannelType = callback_channel_type_t<Callback>;

    return MakeCallbackConsumer<ChannelType>(SubscribeLatest<Message>(),
                                             std::forward<Callback>(callback));
  }

  /**
   * @brief 把回调绑定到对应消息类型的“下一次发布”订阅器。
   *
   * @tparam Callback 回调类型。
   * @param callback 消息处理回调。
   * @return 组合后的回调消费者对象。
   */
  template <typename Callback>
  callback_consumer_type_t<Callback> BindFromNext(Callback &&callback) {
    using Message = callback_message_type_t<Callback>;
    using ChannelType = callback_channel_type_t<Callback>;

    return MakeCallbackConsumer<ChannelType>(SubscribeFromNext<Message>(),
                                             std::forward<Callback>(callback));
  }

private:
  std::tuple<Channels...> channels_ {}; /**< 统一持有的消息通道集合。 */
};

} // namespace iFly

#endif /* IFLY_APP_OBSERVER_OBSERVER_CHANNEL_HPP */

