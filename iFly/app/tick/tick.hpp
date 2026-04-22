/**
 * @file tick.hpp
 * @brief 时间工具模块接口。
 */
#ifndef IFLY_TICK_HPP
#define IFLY_TICK_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前毫秒时间戳。
 *
 * @return 当前毫秒时间戳。
 */
uint32_t ifly_tick_now_ms(void);

/**
 * @brief 获取当前微秒时间戳。
 *
 * @return 当前微秒时间戳。
 */
uint64_t ifly_tick_now_us(void);

/**
 * @brief 获取当前纳秒时间戳。
 *
 * @return 当前纳秒时间戳。
 */
uint64_t ifly_tick_now_ns(void);

/**
 * @brief 计算从指定毫秒时间戳开始经过的时间。
 *
 * @param start_ms 起始毫秒时间戳。
 * @return 已经过的毫秒数。
 */
uint32_t ifly_tick_elapsed_ms(uint32_t start_ms);

/**
 * @brief 计算从指定微秒时间戳开始经过的时间。
 *
 * @param start_us 起始微秒时间戳。
 * @return 已经过的微秒数。
 */
uint64_t ifly_tick_elapsed_us(uint64_t start_us);

/**
 * @brief 计算从指定纳秒时间戳开始经过的时间。
 *
 * @param start_ns 起始纳秒时间戳。
 * @return 已经过的纳秒数。
 */
uint64_t ifly_tick_elapsed_ns(uint64_t start_ns);

/**
 * @brief 执行毫秒级阻塞延时。
 *
 * @param delay_ms 延时时长，单位为毫秒。
 */
void ifly_tick_delay_ms(uint32_t delay_ms);

/**
 * @brief 执行微秒级阻塞延时。
 *
 * @param delay_us 延时时长，单位为微秒。
 */
void ifly_tick_delay_us(uint32_t delay_us);

#ifdef __cplusplus
}

namespace iFly::tick {

/**
 * @brief 获取当前毫秒时间戳。
 *
 * @return 当前毫秒时间戳。
 */
uint32_t NowMs();

/**
 * @brief 获取当前微秒时间戳。
 *
 * @return 当前微秒时间戳。
 */
uint64_t NowUs();

/**
 * @brief 获取当前纳秒时间戳。
 *
 * @return 当前纳秒时间戳。
 */
uint64_t NowNs();

/**
 * @brief 计算从指定毫秒时间戳开始经过的时间。
 *
 * @param start_ms 起始毫秒时间戳。
 * @return 已经过的毫秒数。
 */
uint32_t ElapsedMs(uint32_t start_ms);

/**
 * @brief 计算从指定微秒时间戳开始经过的时间。
 *
 * @param start_us 起始微秒时间戳。
 * @return 已经过的微秒数。
 */
uint64_t ElapsedUs(uint64_t start_us);

/**
 * @brief 计算从指定纳秒时间戳开始经过的时间。
 *
 * @param start_ns 起始纳秒时间戳。
 * @return 已经过的纳秒数。
 */
uint64_t ElapsedNs(uint64_t start_ns);

/**
 * @brief 执行毫秒级阻塞延时。
 *
 * @param delay_ms 延时时长，单位为毫秒。
 */
void DelayMs(uint32_t delay_ms);

/**
 * @brief 执行微秒级阻塞延时。
 *
 * @param delay_us 延时时长，单位为微秒。
 */
void DelayUs(uint32_t delay_us);

/**
 * @brief 判断毫秒级时间点是否已经超时。
 *
 * @param start_ms 起始毫秒时间戳。
 * @param delay_ms 超时时长，单位为毫秒。
 * @param now_ms 当前毫秒时间戳。
 * @return 已超时返回 `true`。
 */
bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms);

/**
 * @brief 判断微秒级时间点是否已经超时。
 *
 * @param start_us 起始微秒时间戳。
 * @param delay_us 超时时长，单位为微秒。
 * @param now_us 当前微秒时间戳。
 * @return 已超时返回 `true`。
 */
bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us);

/**
 * @brief 判断纳秒级时间点是否已经超时。
 *
 * @param start_ns 起始纳秒时间戳。
 * @param delay_ns 超时时长，单位为纳秒。
 * @param now_ns 当前纳秒时间戳。
 * @return 已超时返回 `true`。
 */
bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns);

/**
 * @brief 非阻塞毫秒级延时器。
 */
class NonBlockingDelayMs final {
public:
  /**
   * @brief 清除延时状态。
   */
  void Reset();

  /**
   * @brief 以当前时间为基准启动延时。
   *
   * @param delay_ms 延时时长，单位为毫秒。
   */
  void Start(uint32_t delay_ms);

  /**
   * @brief 以指定时间为基准启动延时。
   *
   * @param now_ms 基准毫秒时间戳。
   * @param delay_ms 延时时长，单位为毫秒。
   */
  void StartFrom(uint32_t now_ms, uint32_t delay_ms);

  /**
   * @brief 判断延时器是否处于激活状态。
   *
   * @return 激活返回 `true`。
   */
  bool IsActive() const {
    return active_;
  }

  /**
   * @brief 判断延时是否已经到期。
   *
   * @return 已到期返回 `true`。
   */
  bool HasExpired() const;

  /**
   * @brief 使用指定时间判断延时是否已经到期。
   *
   * @param now_ms 当前毫秒时间戳。
   * @return 已到期返回 `true`。
   */
  bool HasExpiredAt(uint32_t now_ms) const;

  /**
   * @brief 若已到期则消费本次到期状态。
   *
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpired();

  /**
   * @brief 使用指定时间判断并消费到期状态。
   *
   * @param now_ms 当前毫秒时间戳。
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpiredAt(uint32_t now_ms);

private:
  uint32_t deadline_ms_ = 0U; /**< 到期时间点，单位为毫秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

/**
 * @brief 非阻塞微秒级延时器。
 */
class NonBlockingDelayUs final {
public:
  /**
   * @brief 清除延时状态。
   */
  void Reset();

  /**
   * @brief 以当前时间为基准启动延时。
   *
   * @param delay_us 延时时长，单位为微秒。
   */
  void Start(uint64_t delay_us);

  /**
   * @brief 以指定时间为基准启动延时。
   *
   * @param now_us 基准微秒时间戳。
   * @param delay_us 延时时长，单位为微秒。
   */
  void StartFrom(uint64_t now_us, uint64_t delay_us);

  /**
   * @brief 判断延时器是否处于激活状态。
   *
   * @return 激活返回 `true`。
   */
  bool IsActive() const {
    return active_;
  }

  /**
   * @brief 判断延时是否已经到期。
   *
   * @return 已到期返回 `true`。
   */
  bool HasExpired() const;

  /**
   * @brief 使用指定时间判断延时是否已经到期。
   *
   * @param now_us 当前微秒时间戳。
   * @return 已到期返回 `true`。
   */
  bool HasExpiredAt(uint64_t now_us) const;

  /**
   * @brief 若已到期则消费本次到期状态。
   *
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpired();

  /**
   * @brief 使用指定时间判断并消费到期状态。
   *
   * @param now_us 当前微秒时间戳。
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpiredAt(uint64_t now_us);

private:
  uint64_t deadline_us_ = 0ULL; /**< 到期时间点，单位为微秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

/**
 * @brief 非阻塞纳秒级延时器。
 */
class NonBlockingDelayNs final {
public:
  /**
   * @brief 清除延时状态。
   */
  void Reset();

  /**
   * @brief 以当前时间为基准启动延时。
   *
   * @param delay_ns 延时时长，单位为纳秒。
   */
  void Start(uint64_t delay_ns);

  /**
   * @brief 以指定时间为基准启动延时。
   *
   * @param now_ns 基准纳秒时间戳。
   * @param delay_ns 延时时长，单位为纳秒。
   */
  void StartFrom(uint64_t now_ns, uint64_t delay_ns);

  /**
   * @brief 判断延时器是否处于激活状态。
   *
   * @return 激活返回 `true`。
   */
  bool IsActive() const {
    return active_;
  }

  /**
   * @brief 判断延时是否已经到期。
   *
   * @return 已到期返回 `true`。
   */
  bool HasExpired() const;

  /**
   * @brief 使用指定时间判断延时是否已经到期。
   *
   * @param now_ns 当前纳秒时间戳。
   * @return 已到期返回 `true`。
   */
  bool HasExpiredAt(uint64_t now_ns) const;

  /**
   * @brief 若已到期则消费本次到期状态。
   *
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpired();

  /**
   * @brief 使用指定时间判断并消费到期状态。
   *
   * @param now_ns 当前纳秒时间戳。
   * @return 成功消费到期状态返回 `true`。
   */
  bool ConsumeIfExpiredAt(uint64_t now_ns);

private:
  uint64_t deadline_ns_ = 0ULL; /**< 到期时间点，单位为纳秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

} // namespace iFly::tick

#endif

#endif /* IFLY_TICK_HPP */
