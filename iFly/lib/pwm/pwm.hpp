#ifndef IFLY_PWM_HPP
#define IFLY_PWM_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 统一的 PWM 通道枚举。
 * @details
 * 这里做的是软件层抽象，便于上层不直接依赖 `TIM_CHANNEL_x` 宏。
 */
enum class PwmChannelId : uint8_t {
  kChannel1 = 1U,
  kChannel2 = 2U,
  kChannel3 = 3U,
  kChannel4 = 4U
};

const char *ToString(PwmChannelId channel);

/**
 * @brief 单路 PWM 输出控制对象。
 * @details
 * 这个类只负责“基于已经由 CubeMX 初始化好的 TIM PWM 通道做运行时控制”，
 * 不负责定时器时钟、GPIO 复用、PWM 模式等底层初始化。
 *
 * 设计目标：
 * - 通过 `void* + channel` 绑定任意 PWM 通道；
 * - 提供原始 compare 值控制，也提供归一化占空比控制；
 * - 允许通过 `min_compare/max_compare` 给业务层限定安全输出范围；
 * - 保持类本身足够薄，方便后续移植到其他使用 HAL TIM 的工程。
 */
class PwmChannel final {
public:
  /**
   * @brief PWM 通道初始化参数。
   * @param htim            已完成 CubeMX/HAL 初始化的定时器句柄。
   * @param channel         逻辑 PWM 通道号。
   * @param min_compare     允许输出的最小比较值。
   * @param max_compare     允许输出的最大比较值。
   * @param initial_compare 初始比较值，初始化时会自动夹紧到合法范围。
   * @param auto_start      初始化完成后是否立即启动该 PWM 通道。
   */
  struct Config final {
    void *htim = nullptr;
    PwmChannelId channel = PwmChannelId::kChannel1;
    uint32_t min_compare = 0U;
    uint32_t max_compare = 0U;
    uint32_t initial_compare = 0U;
    bool auto_start = false;
  };

  PwmChannel() = default;
  explicit PwmChannel(const Config &config);

  /** @brief 根据配置绑定硬件并设置初始输出。*/
  bool Init(const Config &config);
  /** @brief 停止当前输出并清空对象内部绑定关系。*/
  void Deinit();

  /** @brief 绑定一个 HAL 定时器句柄和逻辑通道号。*/
  void AttachHardware(void *htim, PwmChannelId channel);
  /** @brief 绑定一个 HAL 定时器句柄和原生 `TIM_CHANNEL_x` 宏值。*/
  void AttachHardware(void *htim, uint32_t hal_channel);

  /** @brief 启动当前 PWM 通道输出。*/
  bool Start();
  /** @brief 停止当前 PWM 通道输出。*/
  void Stop();

  /** @brief 按原始 compare 值设置输出，占空比由 ARR 和 compare 共同决定。*/
  bool SetCompare(uint32_t compare);
  /** @brief 按 0.0f ~ 1.0f 的归一化占空比设置输出。*/
  bool SetDutyCycle(float duty_cycle);

  /** @brief 当前对象是否已经绑定到有效的 PWM 硬件通道。*/
  bool IsReady() const;
  /** @brief 当前 PWM 通道是否已经处于启动状态。*/
  bool IsStarted() const;

  /** @brief 返回当前绑定的 HAL 定时器句柄。*/
  void *Handle() const;
  /** @brief 返回当前绑定的 HAL 原生通道值，例如 `TIM_CHANNEL_1`。*/
  uint32_t HalChannel() const;
  /** @brief 返回当前实际 compare 值。*/
  uint32_t Compare() const;
  /** @brief 返回当前定时器 ARR，也就是 PWM 周期上限。*/
  uint32_t Period() const;
  /** @brief 返回当前生效的最小 compare 限幅。*/
  uint32_t MinCompare() const;
  /** @brief 返回当前生效的最大 compare 限幅。*/
  uint32_t MaxCompare() const;
  /** @brief 返回当前通道输出对应的归一化占空比。*/
  float DutyCycle() const;

  /** @brief 将逻辑通道号转换为 HAL 的 `TIM_CHANNEL_x` 宏值。*/
  static uint32_t ToHalChannel(PwmChannelId channel);
  /** @brief 判断是否为当前实现支持的 HAL PWM 通道。*/
  static bool IsSupportedChannel(uint32_t hal_channel);

private:
  /** @brief 结合用户配置和 ARR 后得到的实际最小 compare。*/
  uint32_t EffectiveMinCompare() const;
  /** @brief 结合用户配置和 ARR 后得到的实际最大 compare。*/
  uint32_t EffectiveMaxCompare() const;
  /** @brief 把 compare 值夹紧到当前合法范围内。*/
  uint32_t ClampCompare(uint32_t compare) const;

private:
  void *htim_ = nullptr;
  uint32_t channel_ = 0U;
  uint32_t min_compare_ = 0U;
  uint32_t max_compare_ = 0U;
  uint32_t compare_ = 0U;
};

using pwm_channel = PwmChannel;

} // namespace iFly

#endif /* IFLY_PWM_HPP */
