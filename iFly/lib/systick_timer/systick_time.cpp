// 底层时间源实现。
// 负责 HAL Tick、DWT 计数器和忙等延时的统一封装。
#include "systick_time.hpp"

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

namespace {

constexpr uint64_t kMicrosecondsPerSecond = 1000000ULL;
constexpr uint64_t kNanosecondsPerSecond = 1000000000ULL;

} // namespace

namespace iFly {

namespace systick_time {

// 返回当前毫秒时间。
uint32_t NowMs()
{
  return HAL_GetTick();
}

// 返回当前微秒时间。
uint64_t NowUs()
{
  return SysTickNsTimer::Instance().NowUs();
}

// 返回当前纳秒时间。
uint64_t NowNs()
{
  return SysTickNsTimer::Instance().NowNs();
}

// 计算经过的毫秒时间。
uint32_t ElapsedMs(uint32_t start_ms)
{
  return NowMs() - start_ms;
}

// 计算经过的微秒时间。
uint64_t ElapsedUs(uint64_t start_us)
{
  return SysTickNsTimer::Instance().ElapsedUs(start_us);
}

// 计算经过的纳秒时间。
uint64_t ElapsedNs(uint64_t start_ns)
{
  return SysTickNsTimer::Instance().ElapsedNs(start_ns);
}

// 执行毫秒级阻塞延时。
void DelayMs(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

// 执行微秒级阻塞延时。
void DelayUs(uint32_t delay_us)
{
  if (delay_us == 0U) {
    return;
  }

  SysTickNsTimer &timer = SysTickNsTimer::Instance();
  const uint64_t ticks_per_second = static_cast<uint64_t>(timer.TickClockHz());
  const uint64_t start_ticks = timer.NowTicks();
  const uint64_t delay_ticks =
      ((static_cast<uint64_t>(delay_us) * ticks_per_second) +
       (kMicrosecondsPerSecond - 1ULL)) /
      kMicrosecondsPerSecond;
  const uint64_t target_ticks =
      start_ticks + ((delay_ticks > 0ULL) ? delay_ticks : 1ULL);

  while (timer.NowTicks() < target_ticks) {
  }
}

} // namespace systick_time

// 返回单例实例。
SysTickNsTimer &SysTickNsTimer::Instance()
{
  static SysTickNsTimer instance;
  return instance;
}

// 确保高精度计数器已经开启。
void SysTickNsTimer::EnsureEnabled() const
{
  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0U) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  }

#if defined(DWT_LAR)
  DWT->LAR = 0xC5ACCE55U;
#endif

  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }

  if (!initialized_) {
    wrap_high_ = 0U;
    last_cycle_sample_ = DWT->CYCCNT;
    initialized_ = true;
  }
}

// 更新硬件计数器回绕状态。
void SysTickNsTimer::UpdateWrapState() const
{
  EnsureEnabled();

  const uint32_t current_cycles = DWT->CYCCNT;
  const uint32_t last_cycles = last_cycle_sample_;

  // DWT->CYCCNT 是 32 位递增计数器，当前值小于上次采样值时说明发生了回绕。
  if (current_cycles < last_cycles) {
    ++wrap_high_;
  }

  last_cycle_sample_ = current_cycles;
}

// 处理 SysTick 节拍更新。
void SysTickNsTimer::OnSysTick()
{
  // 借助 SysTick 每 1ms 调一次，保证 DWT 的 32 位回绕不会漏检。
  UpdateWrapState();
}

// 返回扩展后的时钟 tick 计数值。
uint64_t SysTickNsTimer::NowTicks() const
{
  EnsureEnabled();

  const uint32_t high_before = wrap_high_;
  uint32_t current_cycles = DWT->CYCCNT;
  const uint32_t high_after = wrap_high_;

  // 如果读取期间 SysTick 正好推进了回绕高位，就重新取一次低 32 位，
  // 确保高低位来自同一轮状态。
  if (high_before != high_after) {
    current_cycles = DWT->CYCCNT;
    return (static_cast<uint64_t>(high_after) << 32U) | current_cycles;
  }

  // 如果 DWT 刚刚回绕，但 SysTick 还没来得及更新高位，
  // 则当前值会小于最近一次的低位采样值，这里提前补 1 个高位。
  if (current_cycles < last_cycle_sample_) {
    return (static_cast<uint64_t>(high_before + 1U) << 32U) | current_cycles;
  }

  return (static_cast<uint64_t>(high_before) << 32U) | current_cycles;
}

// 返回当前纳秒时间。
uint64_t SysTickNsTimer::NowNs() const
{
  const uint64_t ticks = NowTicks();
  return (ticks * kNanosecondsPerSecond) / static_cast<uint64_t>(TickClockHz());
}

// 返回当前微秒时间。
uint64_t SysTickNsTimer::NowUs() const
{
  const uint64_t ticks = NowTicks();
  return (ticks * kMicrosecondsPerSecond) /
         static_cast<uint64_t>(TickClockHz());
}

// 计算经过的纳秒时间。
uint64_t SysTickNsTimer::ElapsedNs(uint64_t start_ns) const
{
  return NowNs() - start_ns;
}

// 计算经过的微秒时间。
uint64_t SysTickNsTimer::ElapsedUs(uint64_t start_us) const
{
  return NowUs() - start_us;
}

// 返回底层时钟频率。
uint32_t SysTickNsTimer::TickClockHz() const
{
  return SystemCoreClock;
}

// 返回单个 tick 对应的纳秒下界。
uint32_t SysTickNsTimer::TickPeriodNsFloor() const
{
  return static_cast<uint32_t>(kNanosecondsPerSecond / static_cast<uint64_t>(TickClockHz()));
}

} // namespace iFly

// 导出高精度计时器的 SysTick 钩子。
extern "C" void ifly_systick_ns_timer_tick(void)
{
  iFly::SysTickNsTimer::Instance().OnSysTick();
}

// 导出 C 接口毫秒取时函数。
extern "C" uint32_t ifly_systick_time_now_ms(void)
{
  return iFly::systick_time::NowMs();
}

// 导出 C 接口微秒取时函数。
extern "C" uint64_t ifly_systick_time_now_us(void)
{
  return iFly::systick_time::NowUs();
}

// 导出 C 接口纳秒取时函数。
extern "C" uint64_t ifly_systick_time_now_ns(void)
{
  return iFly::systick_time::NowNs();
}

// 导出 C 接口毫秒耗时函数。
extern "C" uint32_t ifly_systick_time_elapsed_ms(uint32_t start_ms)
{
  return iFly::systick_time::ElapsedMs(start_ms);
}

// 导出 C 接口微秒耗时函数。
extern "C" uint64_t ifly_systick_time_elapsed_us(uint64_t start_us)
{
  return iFly::systick_time::ElapsedUs(start_us);
}

// 导出 C 接口纳秒耗时函数。
extern "C" uint64_t ifly_systick_time_elapsed_ns(uint64_t start_ns)
{
  return iFly::systick_time::ElapsedNs(start_ns);
}

// 导出 C 接口毫秒延时函数。
extern "C" void ifly_systick_time_delay_ms(uint32_t delay_ms)
{
  iFly::systick_time::DelayMs(delay_ms);
}

// 导出 C 接口微秒延时函数。
extern "C" void ifly_systick_time_delay_us(uint32_t delay_us)
{
  iFly::systick_time::DelayUs(delay_us);
}
