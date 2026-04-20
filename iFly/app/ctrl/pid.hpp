// PID 控制器接口。
// 提供参数配置、状态维护和单次控制更新所需的数据结构。
#ifndef IFLY_APP_CTRL_PIDCTRL_PID_HPP
#define IFLY_APP_CTRL_PIDCTRL_PID_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 通用 PID 控制器。
 *
 * @details
 * 该实现面向嵌入式闭环控制场景，包含参数清洗、微分滤波、
 * 积分限幅、输出限幅以及简单的抗积分饱和逻辑。
 */
class Pid final {
public:
  /** @brief 微分项的计算方式。 */
  enum class DerivativeMode : uint8_t {
    /** @brief 关闭微分项。 */
    kDisabled = 0U,
    /** @brief 对误差做微分。 */
    kOnError,
    /** @brief 对测量值做微分，内部会自动带负号。 */
    kOnMeasurement,
    /** @brief 直接使用外部提供的测量值变化率。 */
    kOnExternalMeasurementRate,
  };

  /** @brief PID 配置参数。 */
  struct Config final {
    float kp = 0.0f;
    float ki = 0.0f;
    float kd = 0.0f;
    float kff = 0.0f;

    float integral_min = -1.0e30f;
    float integral_max = 1.0e30f;
    float output_min = -1.0e30f;
    float output_max = 1.0e30f;

    float derivative_cutoff_hz = 0.0f;
    float dt_min_s = 1.0e-4f;
    float dt_max_s = 1.0f;

    DerivativeMode derivative_mode = DerivativeMode::kOnMeasurement;
  };

  /** @brief 单次 PID 更新输入。 */
  struct UpdateInput final {
    float setpoint = 0.0f;
    float measurement = 0.0f;
    float dt_s = 0.0f;

    float feedforward = 0.0f;
    // 正值表示测量值正在上升。
    // 在按测量值微分模式下，控制器会在内部自动添加负号。
    float measurement_rate = 0.0f;
    bool measurement_rate_valid = false;

    bool update_integral = true;
    bool reset_integral = false;
    bool reset_derivative = false;
  };

  /** @brief 单次 PID 更新结果。 */
  struct UpdateResult final {
    float output = 0.0f;
    float unsaturated_output = 0.0f;
    float error = 0.0f;

    float proportional = 0.0f;
    float integral = 0.0f;
    float derivative = 0.0f;
    float feedforward = 0.0f;
    float derivative_raw = 0.0f;

    bool output_clamped_low = false;
    bool output_clamped_high = false;
  };

  /** @brief 控制器内部运行状态。 */
  struct State final {
    float integral = 0.0f;
    float previous_error = 0.0f;
    float previous_measurement = 0.0f;
    float derivative_state = 0.0f;
    float last_output = 0.0f;
    bool initialized = false;
  };

  /** @brief 构造一个空配置 PID 控制器。 */
  Pid() = default;
  /** @brief 直接使用给定配置构造 PID 控制器。 */
  explicit Pid(const Config &config);

  /** @brief 应用新配置，并对范围与非法值做清洗。 */
  void Configure(const Config &config);
  /** @brief 返回当前生效的 PID 配置。 */
  const Config &GetConfig() const {
    return config_;
  }

  /** @brief 返回当前内部状态。 */
  const State &GetState() const {
    return state_;
  }

  /** @brief 完全重置 PID 运行状态。 */
  void Reset();
  /** @brief 把积分项重置到指定值。 */
  void ResetIntegrator(float integral = 0.0f);
  /** @brief 直接设置积分项，并自动做积分限幅。 */
  void SetIntegrator(float integral);
  /** @brief 读取当前积分项。 */
  float Integrator() const {
    return state_.integral;
  }

  /** @brief 执行一次 PID 更新。 */
  UpdateResult Update(const UpdateInput &input);

private:
  static bool IsFinite(float value);
  static float Clamp(float value, float lower, float upper);
  static float SanitizeValue(float value, float fallback);
  static void NormalizeRange(float *lower, float *upper);
  static bool HasValidRange(float lower, float upper);

  float SanitizeDt(float dt_s) const;
  float ApplyDerivativeFilter(float derivative_raw, float dt_s, bool reset);
  float ClampIntegral(float integral) const;
  float ClampOutput(float output, bool *clamped_low, bool *clamped_high) const;

private:
  Config config_ {};
  State state_ {};
};

} // namespace iFly

#endif /* IFLY_APP_CTRL_PIDCTRL_PID_HPP */
