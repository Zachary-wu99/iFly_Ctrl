/**
 * @file pwm.hpp
 * @brief PWM 输出控制接口。
 */
#ifndef IFLY_PWM_HPP
#define IFLY_PWM_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 统一的 PWM 逻辑通道号。
 */
enum class PwmChannelId : uint8_t {
  kChannel1 = 1U, /**< 通道 1。 */
  kChannel2 = 2U, /**< 通道 2。 */
  kChannel3 = 3U, /**< 通道 3。 */
  kChannel4 = 4U  /**< 通道 4。 */
};

const char *ToString(PwmChannelId channel);

/**
 * @brief 单路 PWM 输出控制对象。
 */
class PwmChannel final {
public:
  /**
   * @brief PWM 通道初始化配置。
   */
  struct Config final {
    void *htim = nullptr; /**< 已完成初始化的 HAL 定时器句柄。 */
    PwmChannelId channel = PwmChannelId::kChannel1; /**< 逻辑 PWM 通道号。 */
    uint32_t min_compare = 0U; /**< 允许输出的最小比较值。 */
    uint32_t max_compare = 0U; /**< 允许输出的最大比较值。 */
    uint32_t initial_compare = 0U; /**< 初始比较值。 */
    bool auto_start = false; /**< 初始化后是否自动启动输出。 */
  };

  PwmChannel() = default;
  explicit PwmChannel(const Config &config);

  bool Init(const Config &config);
  void Deinit();
  void AttachHardware(void *htim, PwmChannelId channel);
  void AttachHardware(void *htim, uint32_t hal_channel);
  bool Start();
  void Stop();
  bool SetCompare(uint32_t compare);
  bool SetDutyCycle(float duty_cycle);
  bool IsReady() const;
  bool IsStarted() const;
  void *Handle() const;
  uint32_t HalChannel() const;
  uint32_t Compare() const;
  uint32_t Period() const;
  uint32_t MinCompare() const;
  uint32_t MaxCompare() const;
  float DutyCycle() const;
  static uint32_t ToHalChannel(PwmChannelId channel);
  static bool IsSupportedChannel(uint32_t hal_channel);

private:
  uint32_t EffectiveMinCompare() const;
  uint32_t EffectiveMaxCompare() const;
  uint32_t ClampCompare(uint32_t compare) const;

  void *htim_ = nullptr; /**< 当前绑定的 HAL 定时器句柄。 */
  uint32_t channel_ = 0U; /**< 当前绑定的 HAL 原生通道值。 */
  uint32_t min_compare_ = 0U; /**< 配置的最小比较值。 */
  uint32_t max_compare_ = 0U; /**< 配置的最大比较值。 */
  uint32_t compare_ = 0U; /**< 当前比较值缓存。 */
};

using pwm_channel = PwmChannel;

} // namespace iFly

#endif /* IFLY_PWM_HPP */
