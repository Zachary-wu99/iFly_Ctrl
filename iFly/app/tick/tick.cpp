#include "tick.hpp"

#include <stdint.h>

#include "systick_time.hpp"

namespace iFly::tick {

uint32_t NowMs()
{
  return systick_time::NowMs();
}

uint64_t NowUs()
{
  return systick_time::NowUs();
}

uint64_t NowNs()
{
  return systick_time::NowNs();
}

uint32_t ElapsedMs(uint32_t start_ms)
{
  return systick_time::ElapsedMs(start_ms);
}

uint64_t ElapsedUs(uint64_t start_us)
{
  return systick_time::ElapsedUs(start_us);
}

uint64_t ElapsedNs(uint64_t start_ns)
{
  return systick_time::ElapsedNs(start_ns);
}

void DelayMs(uint32_t delay_ms)
{
  systick_time::DelayMs(delay_ms);
}

void DelayUs(uint32_t delay_us)
{
  systick_time::DelayUs(delay_us);
}

bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms)
{
  return static_cast<int32_t>(now_ms - (start_ms + delay_ms)) >= 0;
}

bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us)
{
  return (now_us - start_us) >= delay_us;
}

bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns)
{
  return (now_ns - start_ns) >= delay_ns;
}

void NonBlockingDelayMs::Reset()
{
  deadline_ms_ = 0U;
  active_ = false;
}

void NonBlockingDelayMs::Start(uint32_t delay_ms)
{
  StartFrom(NowMs(), delay_ms);
}

void NonBlockingDelayMs::StartFrom(uint32_t now_ms, uint32_t delay_ms)
{
  deadline_ms_ = now_ms + delay_ms;
  active_ = true;
}

bool NonBlockingDelayMs::HasExpired() const
{
  return HasExpiredAt(NowMs());
}

bool NonBlockingDelayMs::HasExpiredAt(uint32_t now_ms) const
{
  return active_ && (static_cast<int32_t>(now_ms - deadline_ms_) >= 0);
}

bool NonBlockingDelayMs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowMs());
}

bool NonBlockingDelayMs::ConsumeIfExpiredAt(uint32_t now_ms)
{
  if (!HasExpiredAt(now_ms)) {
    return false;
  }

  Reset();
  return true;
}

void NonBlockingDelayUs::Reset()
{
  deadline_us_ = 0ULL;
  active_ = false;
}

void NonBlockingDelayUs::Start(uint64_t delay_us)
{
  StartFrom(NowUs(), delay_us);
}

void NonBlockingDelayUs::StartFrom(uint64_t now_us, uint64_t delay_us)
{
  deadline_us_ = now_us + delay_us;
  active_ = true;
}

bool NonBlockingDelayUs::HasExpired() const
{
  return HasExpiredAt(NowUs());
}

bool NonBlockingDelayUs::HasExpiredAt(uint64_t now_us) const
{
  return active_ && (now_us >= deadline_us_);
}

bool NonBlockingDelayUs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowUs());
}

bool NonBlockingDelayUs::ConsumeIfExpiredAt(uint64_t now_us)
{
  if (!HasExpiredAt(now_us)) {
    return false;
  }

  Reset();
  return true;
}

void NonBlockingDelayNs::Reset()
{
  deadline_ns_ = 0ULL;
  active_ = false;
}

void NonBlockingDelayNs::Start(uint64_t delay_ns)
{
  StartFrom(NowNs(), delay_ns);
}

void NonBlockingDelayNs::StartFrom(uint64_t now_ns, uint64_t delay_ns)
{
  deadline_ns_ = now_ns + delay_ns;
  active_ = true;
}

bool NonBlockingDelayNs::HasExpired() const
{
  return HasExpiredAt(NowNs());
}

bool NonBlockingDelayNs::HasExpiredAt(uint64_t now_ns) const
{
  return active_ && (now_ns >= deadline_ns_);
}

bool NonBlockingDelayNs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowNs());
}

bool NonBlockingDelayNs::ConsumeIfExpiredAt(uint64_t now_ns)
{
  if (!HasExpiredAt(now_ns)) {
    return false;
  }

  Reset();
  return true;
}

} // namespace iFly::tick

extern "C" uint32_t ifly_tick_now_ms(void)
{
  return iFly::tick::NowMs();
}

extern "C" uint64_t ifly_tick_now_us(void)
{
  return iFly::tick::NowUs();
}

extern "C" uint64_t ifly_tick_now_ns(void)
{
  return iFly::tick::NowNs();
}

extern "C" uint32_t ifly_tick_elapsed_ms(uint32_t start_ms)
{
  return iFly::tick::ElapsedMs(start_ms);
}

extern "C" uint64_t ifly_tick_elapsed_us(uint64_t start_us)
{
  return iFly::tick::ElapsedUs(start_us);
}

extern "C" uint64_t ifly_tick_elapsed_ns(uint64_t start_ns)
{
  return iFly::tick::ElapsedNs(start_ns);
}

extern "C" void ifly_tick_delay_ms(uint32_t delay_ms)
{
  iFly::tick::DelayMs(delay_ms);
}

extern "C" void ifly_tick_delay_us(uint32_t delay_us)
{
  iFly::tick::DelayUs(delay_us);
}
