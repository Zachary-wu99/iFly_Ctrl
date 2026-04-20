// 底层时间源接口。
// 封装 HAL Tick 和 DWT 周期计数器的 ms/us/ns 读时能力。
#ifndef IFLY_SYSTICK_TIME_HPP
#define IFLY_SYSTICK_TIME_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 在 SysTick 中断里调用，用于维护 DWT 周期计数器的回绕扩展。
void ifly_systick_ns_timer_tick(void);
uint32_t ifly_systick_time_now_ms(void);
uint64_t ifly_systick_time_now_us(void);
uint64_t ifly_systick_time_now_ns(void);
uint32_t ifly_systick_time_elapsed_ms(uint32_t start_ms);
uint64_t ifly_systick_time_elapsed_us(uint64_t start_us);
uint64_t ifly_systick_time_elapsed_ns(uint64_t start_ns);
void ifly_systick_time_delay_ms(uint32_t delay_ms);
void ifly_systick_time_delay_us(uint32_t delay_us);

#ifdef __cplusplus
}

namespace iFly {

namespace systick_time {

// HAL/DWT 底层时间接口。
uint32_t NowMs();
uint64_t NowUs();
uint64_t NowNs();
uint32_t ElapsedMs(uint32_t start_ms);
uint64_t ElapsedUs(uint64_t start_us);
uint64_t ElapsedNs(uint64_t start_ns);
void DelayMs(uint32_t delay_ms);
void DelayUs(uint32_t delay_us);

} // namespace systick_time

// 基于 HAL Tick 和 DWT 周期计数器的底层时间源。
class SysTickNsTimer final {
public:
  // 获取全局唯一实例。
  static SysTickNsTimer &Instance();

  // 在 SysTick 中断中调用，维护 CYCCNT 回绕扩展。
  void OnSysTick();

  // 获取累计 CPU 周期数以及 ns/us 时间戳。
  uint64_t NowTicks() const;
  uint64_t NowNs() const;
  uint64_t NowUs() const;
  uint64_t ElapsedNs(uint64_t start_ns) const;
  uint64_t ElapsedUs(uint64_t start_us) const;

  // 获取 DWT 时钟信息。
  uint32_t TickClockHz() const;
  uint32_t TickPeriodNsFloor() const;

private:
  SysTickNsTimer() = default;

  // 确保 DWT 已启用，并在需要时推进软件高位。
  void EnsureEnabled() const;
  void UpdateWrapState() const;

private:
  // DWT 低 32 位回绕后，软件维护的高 32 位计数。
  mutable volatile uint32_t wrap_high_ = 0U;
  // 最近一次用于判断回绕的低 32 位采样值。
  mutable volatile uint32_t last_cycle_sample_ = 0U;
  // DWT 是否已经完成初始化。
  mutable volatile bool initialized_ = false;
};

} // namespace iFly

#endif

#endif /* IFLY_SYSTICK_TIME_HPP */
