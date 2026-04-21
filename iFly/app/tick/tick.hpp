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

uint32_t ifly_tick_now_ms(void);
uint64_t ifly_tick_now_us(void);
uint64_t ifly_tick_now_ns(void);
uint32_t ifly_tick_elapsed_ms(uint32_t start_ms);
uint64_t ifly_tick_elapsed_us(uint64_t start_us);
uint64_t ifly_tick_elapsed_ns(uint64_t start_ns);
void ifly_tick_delay_ms(uint32_t delay_ms);
void ifly_tick_delay_us(uint32_t delay_us);

#ifdef __cplusplus
}

namespace iFly::tick {

uint32_t NowMs();
uint64_t NowUs();
uint64_t NowNs();
uint32_t ElapsedMs(uint32_t start_ms);
uint64_t ElapsedUs(uint64_t start_us);
uint64_t ElapsedNs(uint64_t start_ns);
void DelayMs(uint32_t delay_ms);
void DelayUs(uint64_t delay_us);
bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms);
bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us);
bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns);

/**
 * @brief 非阻塞毫秒延时器。
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
   * @param now_ms 基准时间戳，单位为毫秒。
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

  bool HasExpired() const;
  bool HasExpiredAt(uint32_t now_ms) const;
  bool ConsumeIfExpired();
  bool ConsumeIfExpiredAt(uint32_t now_ms);

private:
  uint32_t deadline_ms_ = 0U; /**< 到期时间点，单位为毫秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

/**
 * @brief 非阻塞微秒延时器。
 */
class NonBlockingDelayUs final {
public:
  void Reset();
  void Start(uint64_t delay_us);
  void StartFrom(uint64_t now_us, uint64_t delay_us);

  bool IsActive() const {
    return active_;
  }

  bool HasExpired() const;
  bool HasExpiredAt(uint64_t now_us) const;
  bool ConsumeIfExpired();
  bool ConsumeIfExpiredAt(uint64_t now_us);

private:
  uint64_t deadline_us_ = 0ULL; /**< 到期时间点，单位为微秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

/**
 * @brief 非阻塞纳秒延时器。
 */
class NonBlockingDelayNs final {
public:
  void Reset();
  void Start(uint64_t delay_ns);
  void StartFrom(uint64_t now_ns, uint64_t delay_ns);

  bool IsActive() const {
    return active_;
  }

  bool HasExpired() const;
  bool HasExpiredAt(uint64_t now_ns) const;
  bool ConsumeIfExpired();
  bool ConsumeIfExpiredAt(uint64_t now_ns);

private:
  uint64_t deadline_ns_ = 0ULL; /**< 到期时间点，单位为纳秒。 */
  bool active_ = false; /**< 延时器是否处于激活状态。 */
};

} // namespace iFly::tick

#endif

#endif /* IFLY_TICK_HPP */
