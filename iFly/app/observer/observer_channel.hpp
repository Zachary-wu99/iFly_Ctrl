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

enum class ConsumeStatus : uint8_t {
  kNoData = 0U,
  kOk,
  kOverflowed,
};

template <typename T, uint32_t kHistoryDepth = 4U, uint32_t kMaxConsumers = 4U>
class ObserverChannel {
public:
  using value_type = T;
  static constexpr uint32_t HistoryDepth = kHistoryDepth;
  static constexpr uint32_t MaxConsumers = kMaxConsumers;

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

    ConsumeStatus TryRead(T &out, uint32_t *sequence = nullptr) {
      if (channel_ == nullptr) {
        return ConsumeStatus::kNoData;
      }

      return channel_->TryReadImpl(nextSequence_, false, out, sequence);
    }

    ConsumeStatus TryReadLatest(T &out, uint32_t *sequence = nullptr) {
      if (channel_ == nullptr) {
        return ConsumeStatus::kNoData;
      }

      return channel_->TryReadImpl(nextSequence_, true, out, sequence);
    }

    void ResetToLatest() {
      if (channel_ == nullptr) {
        return;
      }

      const uint32_t published =
          channel_->publishedSequence_.load(std::memory_order_acquire);
      nextSequence_ = (published == 0U) ? 1U : published;
    }

    void ResetToNextPublication() {
      if (channel_ == nullptr) {
        return;
      }

      const uint32_t published =
          channel_->publishedSequence_.load(std::memory_order_acquire);
      nextSequence_ = published + 1U;
    }

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

    ObserverChannel *channel_ = nullptr;
    uint32_t registrationIndex_ = kInvalidIndex;
    uint32_t nextSequence_ = 1U;
  };

  ObserverChannel() = default;

  ObserverChannel(const ObserverChannel &) = delete;
  ObserverChannel &operator=(const ObserverChannel &) = delete;

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

  template <typename... Args>
  bool Emplace(Args &&...args) {
    T value(std::forward<Args>(args)...);
    return Publish(std::move(value));
  }

  Consumer SubscribeLatest() {
    return SubscribeImpl(false);
  }

  Consumer SubscribeFromNext() {
    return SubscribeImpl(true);
  }

  uint32_t PublishedSequence() const {
    return publishedSequence_.load(std::memory_order_acquire);
  }

private:
  struct Registration {
    std::atomic<bool> active {false};
  };

  struct Slot {
    std::atomic<uint32_t> stamp {0U};
    T value {};
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

  std::array<Slot, kHistoryDepth> slots_ {};
  std::array<Registration, kMaxConsumers> registrations_ {};
  std::atomic<uint32_t> publishedSequence_ {0U};
};

template <typename T>
struct FunctionTraits;

template <typename R, typename Arg>
struct FunctionTraits<R (*)(Arg)> {
  using argument_type = Arg;
};

template <typename R, typename Arg>
struct FunctionTraits<R(Arg)> {
  using argument_type = Arg;
};

template <typename C, typename R, typename Arg>
struct FunctionTraits<R (C::*)(Arg)> {
  using argument_type = Arg;
};

template <typename C, typename R, typename Arg>
struct FunctionTraits<R (C::*)(Arg) const> {
  using argument_type = Arg;
};

template <typename Callback, typename = void>
struct CallbackArgument {
  using type =
      typename FunctionTraits<std::remove_pointer_t<std::remove_reference_t<
          Callback>>>::argument_type;
};

template <typename Callback>
struct CallbackArgument<
    Callback,
    std::void_t<decltype(&std::remove_reference_t<Callback>::operator())>> {
  using type = typename FunctionTraits<
      decltype(&std::remove_reference_t<Callback>::operator())>::argument_type;
};

template <typename Callback>
using callback_argument_t = typename CallbackArgument<Callback>::type;

template <typename T>
using callback_message_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Channel, typename Callback>
class CallbackConsumer {
public:
  using value_type = typename Channel::value_type;

  CallbackConsumer(typename Channel::Consumer consumer, Callback callback)
      : consumer_(std::move(consumer)), callback_(std::move(callback)) {}

  ConsumeStatus PollOnce(uint32_t *sequence = nullptr) {
    value_type value {};
    const ConsumeStatus status = consumer_.TryRead(value, sequence);
    if (status == ConsumeStatus::kNoData) {
      return status;
    }

    callback_(value);
    return status;
  }

  ConsumeStatus PollLatest(uint32_t *sequence = nullptr) {
    value_type value {};
    const ConsumeStatus status = consumer_.TryReadLatest(value, sequence);
    if (status == ConsumeStatus::kNoData) {
      return status;
    }

    callback_(value);
    return status;
  }

  uint32_t Drain() {
    uint32_t count = 0U;
    while (PollOnce() != ConsumeStatus::kNoData) {
      ++count;
    }
    return count;
  }

  typename Channel::Consumer &Handle() {
    return consumer_;
  }

private:
  typename Channel::Consumer consumer_;
  Callback callback_;
};

template <typename Channel, typename Callback>
CallbackConsumer<Channel, std::decay_t<Callback>> MakeCallbackConsumer(
    typename Channel::Consumer consumer, Callback &&callback) {
  return CallbackConsumer<Channel, std::decay_t<Callback>>(
      std::move(consumer), std::forward<Callback>(callback));
}

template <typename Message, typename... Channels>
struct ChannelIndex;

template <typename Message, typename First, typename... Rest>
struct ChannelIndex<Message, First, Rest...> {
  static constexpr size_t value =
      std::is_same_v<Message, typename First::value_type>
          ? 0U
          : 1U + ChannelIndex<Message, Rest...>::value;
};

template <typename Message, typename Last>
struct ChannelIndex<Message, Last> {
  static constexpr size_t value = 0U;
};

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

  template <typename Message>
  bool Publish(Message &&message) {
    using ValueType = std::decay_t<Message>;
    return Channel<ValueType>().Publish(std::forward<Message>(message));
  }

  template <typename Message, typename... Args>
  bool Emplace(Args &&...args) {
    return Channel<Message>().Emplace(std::forward<Args>(args)...);
  }

  template <typename Message>
  typename channel_type_t<Message>::Consumer SubscribeLatest() {
    return Channel<Message>().SubscribeLatest();
  }

  template <typename Message>
  typename channel_type_t<Message>::Consumer SubscribeFromNext() {
    return Channel<Message>().SubscribeFromNext();
  }

  template <typename Message>
  channel_type_t<Message> &Channel() {
    constexpr size_t index = ChannelIndexValue<Message>();
    return std::get<index>(channels_);
  }

  template <typename Message>
  const channel_type_t<Message> &Channel() const {
    constexpr size_t index = ChannelIndexValue<Message>();
    return std::get<index>(channels_);
  }

  template <typename Callback>
  callback_consumer_type_t<Callback> BindLatest(Callback &&callback) {
    using Message = callback_message_type_t<Callback>;
    using ChannelType = callback_channel_type_t<Callback>;

    return MakeCallbackConsumer<ChannelType>(SubscribeLatest<Message>(),
                                             std::forward<Callback>(callback));
  }

  template <typename Callback>
  callback_consumer_type_t<Callback> BindFromNext(Callback &&callback) {
    using Message = callback_message_type_t<Callback>;
    using ChannelType = callback_channel_type_t<Callback>;

    return MakeCallbackConsumer<ChannelType>(SubscribeFromNext<Message>(),
                                             std::forward<Callback>(callback));
  }

private:
  std::tuple<Channels...> channels_ {};
};

} // namespace iFly::observer

#endif /* IFLY_APP_OBSERVER_OBSERVER_CHANNEL_HPP */
