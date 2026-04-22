/**
 * @file pid.hpp
 * @brief PID 控制器接口。
 */
#ifndef IFLY_APP_CTRL_PIDCTRL_PID_HPP
#define IFLY_APP_CTRL_PIDCTRL_PID_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 通用 PID 控制器。
 */
class Pid final {
public:
  /**
   * @brief 微分项计算模式。
   */
  enum class DerivativeMode : uint8_t {
    kDisabled = 0U, /**< 禁用微分项。 */
    kOnError, /**< 基于误差计算微分。 */
    kOnMeasurement, /**< 基于测量值计算微分，内部自动取反。 */
    kOnExternalMeasurementRate /**< 直接使用外部测量变化率。 */
  };

  /**
   * @brief PID 配置参数。
   */
  struct Config final {
    float kp = 0.0f; /**< 比例增益。 */
    float ki = 0.0f; /**< 积分增益。 */
    float kd = 0.0f; /**< 微分增益。 */
    float kff = 0.0f; /**< 前馈增益。 */

    float integral_min = -1.0e30f; /**< 积分项下限。 */
    float integral_max = 1.0e30f; /**< 积分项上限。 */
    float output_min = -1.0e30f; /**< 输出下限。 */
    float output_max = 1.0e30f; /**< 输出上限。 */

    float derivative_cutoff_hz = 0.0f; /**< 微分低通滤波截止频率。 */
    float dt_min_s = 1.0e-4f; /**< 允许的最小采样周期。 */
    float dt_max_s = 1.0f; /**< 允许的最大采样周期。 */

    DerivativeMode derivative_mode =
        DerivativeMode::kOnMeasurement; /**< 微分项工作模式。 */
  };

  /**
   * @brief 单次控制更新输入。
   */
  struct UpdateInput final {
    float setpoint = 0.0f; /**< 目标值。 */
    float measurement = 0.0f; /**< 当前测量值。 */
    float dt_s = 0.0f; /**< 本次控制周期，单位为秒。 */

    float feedforward = 0.0f; /**< 外部前馈项。 */
    float measurement_rate = 0.0f; /**< 外部测量变化率。 */
    bool measurement_rate_valid = false; /**< 外部测量变化率是否有效。 */

    bool update_integral = true; /**< 是否更新积分项。 */
    bool reset_integral = false; /**< 是否在更新前清零积分项。 */
    bool reset_derivative = false; /**< 是否在更新前重置微分滤波状态。 */
  };

  /**
   * @brief 单次控制更新结果。
   */
  struct UpdateResult final {
    float output = 0.0f; /**< 限幅后的最终输出。 */
    float unsaturated_output = 0.0f; /**< 未限幅的原始输出。 */
    float error = 0.0f; /**< 当前控制误差。 */

    float proportional = 0.0f; /**< 比例项输出。 */
    float integral = 0.0f; /**< 积分项输出。 */
    float derivative = 0.0f; /**< 微分项输出。 */
    float feedforward = 0.0f; /**< 前馈项输出。 */
    float derivative_raw = 0.0f; /**< 微分项滤波前的原始值。 */

    bool output_clamped_low = false; /**< 输出是否命中下限。 */
    bool output_clamped_high = false; /**< 输出是否命中上限。 */
  };

  /**
   * @brief 控制器内部运行状态。
   */
  struct State final {
    float integral = 0.0f; /**< 当前积分状态。 */
    float previous_error = 0.0f; /**< 上一次误差值。 */
    float previous_measurement = 0.0f; /**< 上一次测量值。 */
    float derivative_state = 0.0f; /**< 微分滤波器内部状态。 */
    float last_output = 0.0f; /**< 上一次输出结果。 */
    bool initialized = false; /**< 控制器是否已经完成首次更新。 */
  };

  Pid() = default;

  /**
   * @brief 使用给定配置直接构造控制器。
   *
   * @param config PID 配置参数。
   */
  explicit Pid(const Config &config);

  /**
   * @brief 应用新的 PID 配置。
   *
   * @param config PID 配置参数。
   */
  void Configure(const Config &config);

  /**
   * @brief 获取当前生效配置。
   *
   * @return 配置只读引用。
   */
  const Config &GetConfig() const {
    return config_;
  }

  /**
   * @brief 获取当前内部状态。
   *
   * @return 状态只读引用。
   */
  const State &GetState() const {
    return state_;
  }

  /**
   * @brief 完全重置控制器状态。
   */
  void Reset();

  /**
   * @brief 将积分项重置为指定值。
   *
   * @param integral 重置后的积分值。
   */
  void ResetIntegrator(float integral = 0.0f);

  /**
   * @brief 直接设置积分项。
   *
   * @param integral 新的积分值。
   */
  void SetIntegrator(float integral);

  /**
   * @brief 获取当前积分项值。
   *
   * @return 当前积分值。
   */
  float Integrator() const {
    return state_.integral;
  }

  /**
   * @brief 执行一次 PID 更新。
   *
   * @param input 本次更新输入数据。
   * @return 本次更新结果。
   */
  UpdateResult Update(const UpdateInput &input);

private:
  /**
   * @brief 判断浮点值是否为有限数。
   *
   * @param value 待判断的浮点值。
   * @return 有限数返回 `true`。
   */
  static bool IsFinite(float value);

  /**
   * @brief 将浮点值限制在指定区间内。
   *
   * @param value 待限制的值。
   * @param lower 区间下界。
   * @param upper 区间上界。
   * @return 限制后的值。
   */
  static float Clamp(float value, float lower, float upper);

  /**
   * @brief 对非法浮点值进行回退处理。
   *
   * @param value 待检查的浮点值。
   * @param fallback 回退值。
   * @return 有效时返回原值，否则返回回退值。
   */
  static float SanitizeValue(float value, float fallback);

  /**
   * @brief 规范化上下界顺序。
   *
   * @param lower 区间下界指针。
   * @param upper 区间上界指针。
   */
  static void NormalizeRange(float *lower, float *upper);

  /**
   * @brief 判断上下界是否构成有效区间。
   *
   * @param lower 区间下界。
   * @param upper 区间上界。
   * @return 有效区间返回 `true`。
   */
  static bool HasValidRange(float lower, float upper);

  /**
   * @brief 清洗输入采样周期。
   *
   * @param dt_s 输入采样周期，单位为秒。
   * @return 清洗后的采样周期。
   */
  float SanitizeDt(float dt_s) const;

  /**
   * @brief 应用微分滤波器。
   *
   * @param derivative_raw 原始微分值。
   * @param dt_s 当前采样周期。
   * @param reset 是否重置滤波状态。
   * @return 滤波后的微分值。
   */
  float ApplyDerivativeFilter(float derivative_raw, float dt_s, bool reset);

  /**
   * @brief 对积分项执行限幅。
   *
   * @param integral 待限制的积分值。
   * @return 限制后的积分值。
   */
  float ClampIntegral(float integral) const;

  /**
   * @brief 对控制输出执行限幅。
   *
   * @param output 待限制的输出值。
   * @param clamped_low 输出是否命中下限。
   * @param clamped_high 输出是否命中上限。
   * @return 限制后的输出值。
   */
  float ClampOutput(float output, bool *clamped_low, bool *clamped_high) const;

  Config config_ {}; /**< 当前生效配置。 */
  State state_ {}; /**< 当前运行状态。 */
};

} // namespace iFly

#endif /* IFLY_APP_CTRL_PIDCTRL_PID_HPP */
