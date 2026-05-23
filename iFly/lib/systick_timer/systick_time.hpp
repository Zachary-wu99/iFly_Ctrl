/**
 * @file systick_time.hpp
 * @brief 基于 HAL Tick 与 DWT 的底层时间接口。
 */
#ifndef IFLY_SYSTICK_TIME_HPP
#define IFLY_SYSTICK_TIME_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在 SysTick 中断中推进纳秒级时间基。
 */
void ifly_systick_ns_timer_tick(void);

/**
 * @brief 获取当前毫秒时间戳。
 *
 * @return 当前毫秒计数值。
 */
uint32_t ifly_systick_time_now_ms(void);

/**
 * @brief 获取当前微秒时间戳。
 *
 * @return 当前微秒计数值。
 */
uint64_t ifly_systick_time_now_us(void);

/**
 * @brief 获取当前纳秒时间戳。
 *
 * @return 当前纳秒计数值。
 */
uint64_t ifly_systick_time_now_ns(void);

/**
 * @brief 计算经过的毫秒数。
 *
 * @param start_ms 起始毫秒时间戳。
 * @return 已经过的毫秒数。
 */
uint32_t ifly_systick_time_elapsed_ms(uint32_t start_ms);

/**
 * @brief 计算经过的微秒数。
 *
 * @param start_us 起始微秒时间戳。
 * @return 已经过的微秒数。
 */
uint64_t ifly_systick_time_elapsed_us(uint64_t start_us);

/**
 * @brief 计算经过的纳秒数。
 *
 * @param start_ns 起始纳秒时间戳。
 * @return 已经过的纳秒数。
 */
uint64_t ifly_systick_time_elapsed_ns(uint64_t start_ns);

/**
 * @brief 执行毫秒级阻塞延时。
 *
 * @param delay_ms 延时时长，单位为毫秒。
 */
void ifly_systick_time_delay_ms(uint32_t delay_ms);

/**
 * @brief 执行微秒级阻塞延时。
 *
 * @param delay_us 延时时长，单位为微秒。
 */
void ifly_systick_time_delay_us(uint32_t delay_us);

#ifdef __cplusplus
}

namespace iFly {

namespace systick_time {

/**
 * @brief 获取当前毫秒时间戳。
 *
 * @return 当前毫秒计数值。
 */
uint32_t NowMs();

/**
 * @brief 获取当前微秒时间戳。
 *
 * @return 当前微秒计数值。
 */
uint64_t NowUs();

/**
 * @brief 获取当前纳秒时间戳。
 *
 * @return 当前纳秒计数值。
 */
uint64_t NowNs();

/**
 * @brief 计算经过的毫秒数。
 *
 * @param start_ms 起始毫秒时间戳。
 * @return 已经过的毫秒数。
 */
uint32_t ElapsedMs(uint32_t start_ms);

/**
 * @brief 计算经过的微秒数。
 *
 * @param start_us 起始微秒时间戳。
 * @return 已经过的微秒数。
 */
uint64_t ElapsedUs(uint64_t start_us);

/**
 * @brief 计算经过的纳秒数。
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

} // namespace systick_time

/**
 * @brief 基于 HAL Tick 和 DWT 周期计数器的时间源。
 */
class SysTickNsTimer final {
public:
  /**
   * @brief 获取全局唯一实例。
   *
   * @return 单例引用。
   */
  static SysTickNsTimer &Instance();

  /**
   * @brief 在 SysTick 中断中更新回绕状态。
   */
  void OnSysTick();

  /**
   * @brief 获取累计 CPU 周期计数。
   *
   * @return 扩展后的 64 位周期计数值。
   */
  uint64_t NowTicks() const;

  /**
   * @brief 获取当前纳秒时间戳。
   *
   * @return 当前纳秒值。
   */
  uint64_t NowNs() const;

  /**
   * @brief 获取当前微秒时间戳。
   *
   * @return 当前微秒值。
   */
  uint64_t NowUs() const;

  /**
   * @brief 计算经过的纳秒数。
   *
   * @param start_ns 起始纳秒时间戳。
   * @return 已经过的纳秒数。
   */
  uint64_t ElapsedNs(uint64_t start_ns) const;

  /**
   * @brief 计算经过的微秒数。
   *
   * @param start_us 起始微秒时间戳。
   * @return 已经过的微秒数。
   */
  uint64_t ElapsedUs(uint64_t start_us) const;

  /**
   * @brief 获取 DWT 计数时钟频率。
   *
   * @return 时钟频率，单位为 Hz。
   */
  uint32_t TickClockHz() const;

  /**
   * @brief 获取单个 DWT Tick 对应的纳秒下界。
   *
   * @return 每 Tick 对应的纳秒数下界。
   */
  uint32_t TickPeriodNsFloor() const;

private:
  SysTickNsTimer() = default;

  /**
   * @brief 确保 DWT 已启用。
   */
  void EnsureEnabled() const;

  /**
   * @brief 更新 DWT 回绕扩展状态。
   */
  void UpdateWrapState() const;

  mutable volatile uint32_t wrap_high_ = 0U; /**< 软件维护的高 32 位回绕计数。 */
  mutable volatile uint32_t last_cycle_sample_ = 0U; /**< 最近一次低 32 位周期采样值。 */
  mutable volatile bool initialized_ = false; /**< DWT 是否已完成初始化。 */
};

} // namespace iFly

#endif

#endif /* IFLY_SYSTICK_TIME_HPP */

