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
void DelayUs(uint32_t delay_us);

bool IsExpiredMs(uint32_t start_ms, uint32_t delay_ms, uint32_t now_ms);
bool IsExpiredUs(uint64_t start_us, uint64_t delay_us, uint64_t now_us);
bool IsExpiredNs(uint64_t start_ns, uint64_t delay_ns, uint64_t now_ns);

class NonBlockingDelayMs final {
public:
  void Reset();
  void Start(uint32_t delay_ms);
  void StartFrom(uint32_t now_ms, uint32_t delay_ms);

  bool IsActive() const {
    return active_;
  }

  bool HasExpired() const;
  bool HasExpiredAt(uint32_t now_ms) const;
  bool ConsumeIfExpired();
  bool ConsumeIfExpiredAt(uint32_t now_ms);

private:
  uint32_t deadline_ms_ = 0U;
  bool active_ = false;
};

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
  uint64_t deadline_us_ = 0ULL;
  bool active_ = false;
};

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
  uint64_t deadline_ns_ = 0ULL;
  bool active_ = false;
};

} // namespace iFly::tick

#endif

#endif /* IFLY_TICK_HPP */
