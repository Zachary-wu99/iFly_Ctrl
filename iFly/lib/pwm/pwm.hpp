/**
 * @file pwm.hpp
 * @brief PWM 输出控制接口。
 */
#ifndef IFLY_PWM_HPP
#define IFLY_PWM_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 统一的 PWM 逻辑通道编号。
 */
enum class PwmChannelId : uint8_t {
  kChannel1 = 1U, /**< 通道 1。 */
  kChannel2 = 2U, /**< 通道 2。 */
  kChannel3 = 3U, /**< 通道 3。 */
  kChannel4 = 4U  /**< 通道 4。 */
};

/**
 * @brief 将 PWM 逻辑通道编号转换为可读字符串。
 *
 * @param channel PWM 逻辑通道编号。
 * @return 对应的字符串常量。
 */
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
    PwmChannelId channel = PwmChannelId::kChannel1; /**< 逻辑 PWM 通道编号。 */
    uint32_t min_compare = 0U; /**< 允许输出的最小比较值。 */
    uint32_t max_compare = 0U; /**< 允许输出的最大比较值。 */
    uint32_t initial_compare = 0U; /**< 初始比较值。 */
    bool auto_start = false; /**< 初始化后是否自动启动输出。 */
  };

  PwmChannel() = default;

  /**
   * @brief 使用给定配置直接构造 PWM 通道对象。
   *
   * @param config PWM 初始化配置。
   */
  explicit PwmChannel(const Config &config);

  /**
   * @brief 初始化 PWM 通道。
   *
   * @param config PWM 初始化配置。
   * @return 初始化成功返回 `true`。
   */
  bool Init(const Config &config);

  /**
   * @brief 解除当前 PWM 通道与硬件的绑定关系。
   */
  void Deinit();

  /**
   * @brief 按逻辑通道编号重新绑定底层定时器。
   *
   * @param htim HAL 定时器句柄。
   * @param channel 逻辑 PWM 通道编号。
   */
  void AttachHardware(void *htim, PwmChannelId channel);

  /**
   * @brief 按 HAL 原生通道值重新绑定底层定时器。
   *
   * @param htim HAL 定时器句柄。
   * @param hal_channel HAL 原生通道值。
   */
  void AttachHardware(void *htim, uint32_t hal_channel);

  /**
   * @brief 启动当前 PWM 通道输出。
   *
   * @return 启动成功返回 `true`。
   */
  bool Start();

  /**
   * @brief 停止当前 PWM 通道输出。
   */
  void Stop();

  /**
   * @brief 直接设置比较寄存器目标值。
   *
   * @param compare 新的比较值。
   * @return 设置成功返回 `true`。
   */
  bool SetCompare(uint32_t compare);

  /**
   * @brief 按归一化占空比设置输出。
   *
   * @param duty_cycle 目标占空比，范围为 `0.0f` 到 `1.0f`。
   * @return 设置成功返回 `true`。
   */
  bool SetDutyCycle(float duty_cycle);

  /**
   * @brief 判断当前对象是否已绑定有效硬件。
   *
   * @return 已绑定有效硬件返回 `true`。
   */
  bool IsReady() const;

  /**
   * @brief 判断当前 PWM 通道是否正在输出。
   *
   * @return 正在输出返回 `true`。
   */
  bool IsStarted() const;

  /**
   * @brief 获取当前绑定的 HAL 定时器句柄。
   *
   * @return HAL 定时器句柄。
   */
  void *Handle() const;

  /**
   * @brief 获取当前绑定的 HAL 原生通道值。
   *
   * @return HAL 原生通道值。
   */
  uint32_t HalChannel() const;

  /**
   * @brief 获取当前实际比较值。
   *
   * @return 当前比较值。
   */
  uint32_t Compare() const;

  /**
   * @brief 获取当前定时器周期。
   *
   * @return 当前自动重装载值。
   */
  uint32_t Period() const;

  /**
   * @brief 获取当前生效的最小比较值。
   *
   * @return 生效后的最小比较值。
   */
  uint32_t MinCompare() const;

  /**
   * @brief 获取当前生效的最大比较值。
   *
   * @return 生效后的最大比较值。
   */
  uint32_t MaxCompare() const;

  /**
   * @brief 获取当前归一化占空比。
   *
   * @return 当前占空比，范围为 `0.0f` 到 `1.0f`。
   */
  float DutyCycle() const;

  /**
   * @brief 将逻辑通道编号转换为 HAL 原生通道值。
   *
   * @param channel 逻辑 PWM 通道编号。
   * @return 对应的 HAL 原生通道值。
   */
  static uint32_t ToHalChannel(PwmChannelId channel);

  /**
   * @brief 判断 HAL 原生通道值是否受支持。
   *
   * @param hal_channel HAL 原生通道值。
   * @return 支持返回 `true`。
   */
  static bool IsSupportedChannel(uint32_t hal_channel);

private:
  /**
   * @brief 获取当前生效的最小比较值。
   *
   * @return 生效后的最小比较值。
   */
  uint32_t EffectiveMinCompare() const;

  /**
   * @brief 获取当前生效的最大比较值。
   *
   * @return 生效后的最大比较值。
   */
  uint32_t EffectiveMaxCompare() const;

  /**
   * @brief 将比较值限制在当前合法区间内。
   *
   * @param compare 待限制的比较值。
   * @return 限制后的比较值。
   */
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
