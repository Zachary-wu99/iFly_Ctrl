// 时间工具模块接口。
// 对下衔接 systick_time，对上提供统一时间读数和非阻塞延时工具。
#ifndef IFLY_TICK_HPP
#define IFLY_TICK_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 供 C 模块读取当前毫秒时间戳。 */
uint32_t ifly_tick_now_ms(void);
/** @brief 供 C 模块读取当前微秒时间戳。 */
uint64_t ifly_tick_now_us(void);
/** @brief 供 C 模块读取当前纳秒时间戳。 */
uint64_t ifly_tick_now_ns(void);
/** @brief 供 C 模块计算毫秒级已流逝时间。 */
uint32_t ifly_tick_elapsed_ms(uint32_t start_ms);
/** @brief 供 C 模块计算微秒级已流逝时间。 */
uint64_t ifly_tick_elapsed_us(uint64_t start_us);
/** @brief 供 C 模块计算纳秒级已流逝时间。 */
uint64_t ifly_tick_elapsed_ns(uint64_t start_ns);
/** @brief 供 C 模块执行阻塞式毫秒延时。 */
void ifly_tick_delay_ms(uint32_t delay_ms);
/** @brief 供 C 模块执行阻塞式微秒延时。 */
void ifly_tick_delay_us(uint32_t delay_us);

#ifdef __cplusplus
}

namespace iFly::tick {

/** @brief 读取当前毫秒时间戳。 */
uint32_t NowMs();
/** @brief 读取当前微秒时间戳。 */
uint64_t NowUs();
/** @brief 读取当前纳秒时间戳。 */
uint64_t NowNs();
/** @brief 计算从 `start_ms` 到当前时刻的毫秒流逝量。 */
uint32_t ElapsedMs(uint32_t start_ms);
/** @brief 计算从 `start_us` 到当前时刻的微秒流逝量。 */
uint64_t ElapsedUs(uint64_t start_us);
/** @brief 计算从 `start_ns` 到当前时刻的纳秒流逝量。 */
uint64_t ElapsedNs(uint64_t start_ns);
/** @brief 阻塞式毫秒延时。 */
void DelayMs(uint32_t delay_ms);
/** @brief 阻塞式微秒延时。 */
void DelayUs(uint64_t delay_us);

/** @brief 判断毫秒级延时是否已到期，兼容 32 位回绕。 */
bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms);
/** @brief 判断微秒级延时是否已到期。 */
bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us);
/** @brief 判断纳秒级延时是否已到期。 */
bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns);

/** @brief 非阻塞式毫秒延时器。 */
class NonBlockingDelayMs final {
public:
  /** @brief 清空延时状态。 */
  void Reset();
  /** @brief 从当前时间开始启动一个毫秒延时。 */
  void Start(uint32_t delay_ms);
  /** @brief 从指定时间基准启动一个毫秒延时。 */
  void StartFrom(uint32_t now_ms, uint32_t delay_ms);

  /** @brief 判断延时器当前是否处于激活状态。 */
  bool IsActive() const {
    return active_;
  }

  /** @brief 使用当前系统时间判断是否已到期。 */
  bool HasExpired() const;
  /** @brief 使用外部传入的时间判断是否已到期。 */
  bool HasExpiredAt(uint32_t now_ms) const;
  /** @brief 若已到期则消费本次延时并返回 true。 */
  bool ConsumeIfExpired();
  /** @brief 用指定时间判断并消费到期事件。 */
  bool ConsumeIfExpiredAt(uint32_t now_ms);

private:
  uint32_t deadline_ms_ = 0U;
  bool active_ = false;
};

/** @brief 非阻塞式微秒延时器。 */
class NonBlockingDelayUs final {
public:
  /** @brief 清空延时状态。 */
  void Reset();
  /** @brief 从当前时间开始启动一个微秒延时。 */
  void Start(uint64_t delay_us);
  /** @brief 从指定时间基准启动一个微秒延时。 */
  void StartFrom(uint64_t now_us, uint64_t delay_us);

  /** @brief 判断延时器当前是否处于激活状态。 */
  bool IsActive() const {
    return active_;
  }

  /** @brief 使用当前系统时间判断是否已到期。 */
  bool HasExpired() const;
  /** @brief 使用外部传入的时间判断是否已到期。 */
  bool HasExpiredAt(uint64_t now_us) const;
  /** @brief 若已到期则消费本次延时并返回 true。 */
  bool ConsumeIfExpired();
  /** @brief 用指定时间判断并消费到期事件。 */
  bool ConsumeIfExpiredAt(uint64_t now_us);

private:
  uint64_t deadline_us_ = 0ULL;
  bool active_ = false;
};

/** @brief 非阻塞式纳秒延时器。 */
class NonBlockingDelayNs final {
public:
  /** @brief 清空延时状态。 */
  void Reset();
  /** @brief 从当前时间开始启动一个纳秒延时。 */
  void Start(uint64_t delay_ns);
  /** @brief 从指定时间基准启动一个纳秒延时。 */
  void StartFrom(uint64_t now_ns, uint64_t delay_ns);

  /** @brief 判断延时器当前是否处于激活状态。 */
  bool IsActive() const {
    return active_;
  }

  /** @brief 使用当前系统时间判断是否已到期。 */
  bool HasExpired() const;
  /** @brief 使用外部传入的时间判断是否已到期。 */
  bool HasExpiredAt(uint64_t now_ns) const;
  /** @brief 若已到期则消费本次延时并返回 true。 */
  bool ConsumeIfExpired();
  /** @brief 用指定时间判断并消费到期事件。 */
  bool ConsumeIfExpiredAt(uint64_t now_ns);

private:
  uint64_t deadline_ns_ = 0ULL;
  bool active_ = false;
};

} // namespace iFly::tick

#endif

#endif /* IFLY_TICK_HPP */
