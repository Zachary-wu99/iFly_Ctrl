#include "systick_ns_timer.hpp"

#include "stm32f4xx.h"

namespace {

constexpr uint64_t kNanosecondsPerSecond = 1000000000ULL;

} // namespace

namespace iFly {

SysTickNsTimer &SysTickNsTimer::Instance()
{
  static SysTickNsTimer instance;
  return instance;
}

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

void SysTickNsTimer::OnSysTick()
{
  // 借助 SysTick 每 1ms 调一次，保证 DWT 的 32 位回绕不会漏检。
  UpdateWrapState();
}

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

uint64_t SysTickNsTimer::NowNs() const
{
  const uint64_t ticks = NowTicks();
  return (ticks * kNanosecondsPerSecond) / static_cast<uint64_t>(TickClockHz());
}

uint64_t SysTickNsTimer::ElapsedNs(uint64_t start_ns) const
{
  return NowNs() - start_ns;
}

uint32_t SysTickNsTimer::TickClockHz() const
{
  return SystemCoreClock;
}

uint32_t SysTickNsTimer::TickPeriodNsFloor() const
{
  return static_cast<uint32_t>(kNanosecondsPerSecond / static_cast<uint64_t>(TickClockHz()));
}

} // namespace iFly

extern "C" void ifly_systick_ns_timer_tick(void)
{
  iFly::SysTickNsTimer::Instance().OnSysTick();
}
