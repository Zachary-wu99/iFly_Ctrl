// 时间工具模块实现。
// 负责时间源转发、到期判断和非阻塞延时状态机逻辑。
#include "tick.hpp"

#include <stdint.h>

#include "systick_time.hpp"

namespace iFly::tick {

// 返回当前毫秒时间。
uint32_t NowMs()
{
  return systick_time::NowMs();
}

// 返回当前微秒时间。
uint64_t NowUs()
{
  return systick_time::NowUs();
}

// 返回当前纳秒时间。
uint64_t NowNs()
{
  return systick_time::NowNs();
}

// 计算经过的毫秒时间。
uint32_t ElapsedMs(uint32_t start_ms)
{
  return systick_time::ElapsedMs(start_ms);
}

// 计算经过的微秒时间。
uint64_t ElapsedUs(uint64_t start_us)
{
  return systick_time::ElapsedUs(start_us);
}

// 计算经过的纳秒时间。
uint64_t ElapsedNs(uint64_t start_ns)
{
  return systick_time::ElapsedNs(start_ns);
}

// 执行毫秒级阻塞延时。
void DelayMs(uint32_t delay_ms)
{
  systick_time::DelayMs(delay_ms);
}

// 执行微秒级阻塞延时。
void DelayUs(uint32_t delay_us)
{
  systick_time::DelayUs(delay_us);
}

// 判断毫秒级延时是否到期。
bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms)
{
  return static_cast<int32_t>(now_ms - (start_ms + delay_ms)) >= 0;
}

// 判断微秒级延时是否到期。
bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us)
{
  return (now_us - start_us) >= delay_us;
}

// 判断纳秒级延时是否到期。
bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns)
{
  return (now_ns - start_ns) >= delay_ns;
}

// 重置内部运行状态。
void NonBlockingDelayMs::Reset()
{
  deadline_ms_ = 0U;
  active_ = false;
}

// 启动一次非阻塞延时。
void NonBlockingDelayMs::Start(uint32_t delay_ms)
{
  StartFrom(NowMs(), delay_ms);
}

// 以给定时刻为起点启动非阻塞延时。
void NonBlockingDelayMs::StartFrom(uint32_t now_ms, uint32_t delay_ms)
{
  deadline_ms_ = now_ms + delay_ms;
  active_ = true;
}

// 判断当前非阻塞延时是否已经到期。
bool NonBlockingDelayMs::HasExpired() const
{
  return HasExpiredAt(NowMs());
}

// 在指定时间点判断非阻塞延时是否已经到期。
bool NonBlockingDelayMs::HasExpiredAt(uint32_t now_ms) const
{
  return active_ && (static_cast<int32_t>(now_ms - deadline_ms_) >= 0);
}

// 若已经到期则消费状态并复位。
bool NonBlockingDelayMs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowMs());
}

// 在指定时间点检查到期后消费状态并复位。
bool NonBlockingDelayMs::ConsumeIfExpiredAt(uint32_t now_ms)
{
  if (!HasExpiredAt(now_ms)) {
    return false;
  }

  Reset();
  return true;
}

// 重置内部运行状态。
void NonBlockingDelayUs::Reset()
{
  deadline_us_ = 0ULL;
  active_ = false;
}

// 启动一次非阻塞延时。
void NonBlockingDelayUs::Start(uint64_t delay_us)
{
  StartFrom(NowUs(), delay_us);
}

// 以给定时刻为起点启动非阻塞延时。
void NonBlockingDelayUs::StartFrom(uint64_t now_us, uint64_t delay_us)
{
  deadline_us_ = now_us + delay_us;
  active_ = true;
}

// 判断当前非阻塞延时是否已经到期。
bool NonBlockingDelayUs::HasExpired() const
{
  return HasExpiredAt(NowUs());
}

// 在指定时间点判断非阻塞延时是否已经到期。
bool NonBlockingDelayUs::HasExpiredAt(uint64_t now_us) const
{
  return active_ && (now_us >= deadline_us_);
}

// 若已经到期则消费状态并复位。
bool NonBlockingDelayUs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowUs());
}

// 在指定时间点检查到期后消费状态并复位。
bool NonBlockingDelayUs::ConsumeIfExpiredAt(uint64_t now_us)
{
  if (!HasExpiredAt(now_us)) {
    return false;
  }

  Reset();
  return true;
}

// 重置内部运行状态。
void NonBlockingDelayNs::Reset()
{
  deadline_ns_ = 0ULL;
  active_ = false;
}

// 启动一次非阻塞延时。
void NonBlockingDelayNs::Start(uint64_t delay_ns)
{
  StartFrom(NowNs(), delay_ns);
}

// 以给定时刻为起点启动非阻塞延时。
void NonBlockingDelayNs::StartFrom(uint64_t now_ns, uint64_t delay_ns)
{
  deadline_ns_ = now_ns + delay_ns;
  active_ = true;
}

// 判断当前非阻塞延时是否已经到期。
bool NonBlockingDelayNs::HasExpired() const
{
  return HasExpiredAt(NowNs());
}

// 在指定时间点判断非阻塞延时是否已经到期。
bool NonBlockingDelayNs::HasExpiredAt(uint64_t now_ns) const
{
  return active_ && (now_ns >= deadline_ns_);
}

// 若已经到期则消费状态并复位。
bool NonBlockingDelayNs::ConsumeIfExpired()
{
  return ConsumeIfExpiredAt(NowNs());
}

// 在指定时间点检查到期后消费状态并复位。
bool NonBlockingDelayNs::ConsumeIfExpiredAt(uint64_t now_ns)
{
  if (!HasExpiredAt(now_ns)) {
    return false;
  }

  Reset();
  return true;
}

} // namespace iFly::tick

// 导出 C 接口毫秒取时函数。
extern "C" uint32_t ifly_tick_now_ms(void)
{
  return iFly::tick::NowMs();
}

// 导出 C 接口微秒取时函数。
extern "C" uint64_t ifly_tick_now_us(void)
{
  return iFly::tick::NowUs();
}

// 导出 C 接口纳秒取时函数。
extern "C" uint64_t ifly_tick_now_ns(void)
{
  return iFly::tick::NowNs();
}

// 导出 C 接口毫秒耗时函数。
extern "C" uint32_t ifly_tick_elapsed_ms(uint32_t start_ms)
{
  return iFly::tick::ElapsedMs(start_ms);
}

// 导出 C 接口微秒耗时函数。
extern "C" uint64_t ifly_tick_elapsed_us(uint64_t start_us)
{
  return iFly::tick::ElapsedUs(start_us);
}

// 导出 C 接口纳秒耗时函数。
extern "C" uint64_t ifly_tick_elapsed_ns(uint64_t start_ns)
{
  return iFly::tick::ElapsedNs(start_ns);
}

// 导出 C 接口毫秒延时函数。
extern "C" void ifly_tick_delay_ms(uint32_t delay_ms)
{
  iFly::tick::DelayMs(delay_ms);
}

// 导出 C 接口微秒延时函数。
extern "C" void ifly_tick_delay_us(uint32_t delay_us)
{
  iFly::tick::DelayUs(delay_us);
}
