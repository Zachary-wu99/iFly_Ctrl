// PID 控制器实现。
// 包括输入清洗、积分限幅、微分滤波和输出钳位逻辑。
#include "pid.hpp"

#include <math.h>

namespace iFly {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

// 构造PID 控制器并初始化默认成员状态。
Pid::Pid(const Config &config)
{
  Configure(config);
}

// 应用新的配置参数。
void Pid::Configure(const Config &config)
{
  // 对外部配置做一次统一清洗，避免 NaN、范围反转或超界参数进入控制环。
  config_ = config;

  config_.kp = SanitizeValue(config_.kp, 0.0f);
  config_.ki = SanitizeValue(config_.ki, 0.0f);
  config_.kd = SanitizeValue(config_.kd, 0.0f);
  config_.kff = SanitizeValue(config_.kff, 0.0f);

  config_.derivative_cutoff_hz = SanitizeValue(config_.derivative_cutoff_hz, 0.0f);
  if (config_.derivative_cutoff_hz < 0.0f) {
    config_.derivative_cutoff_hz = 0.0f;
  }

  config_.dt_min_s = SanitizeValue(config_.dt_min_s, 1.0e-4f);
  config_.dt_max_s = SanitizeValue(config_.dt_max_s, 1.0f);
  if (config_.dt_min_s < 0.0f) {
    config_.dt_min_s = 0.0f;
  }
  if ((config_.dt_max_s > 0.0f) && (config_.dt_max_s < config_.dt_min_s)) {
    config_.dt_max_s = config_.dt_min_s;
  }

  config_.integral_min = SanitizeValue(config_.integral_min, -1.0e30f);
  config_.integral_max = SanitizeValue(config_.integral_max, 1.0e30f);
  NormalizeRange(&config_.integral_min, &config_.integral_max);

  config_.output_min = SanitizeValue(config_.output_min, -1.0e30f);
  config_.output_max = SanitizeValue(config_.output_max, 1.0e30f);
  NormalizeRange(&config_.output_min, &config_.output_max);

  state_.integral = ClampIntegral(state_.integral);
}

// 重置内部运行状态。
void Pid::Reset()
{
  state_ = State {};
}

// 重置积分项为指定值。
void Pid::ResetIntegrator(float integral)
{
  state_.integral = ClampIntegral(integral);
}

// 设置积分项当前值。
void Pid::SetIntegrator(float integral)
{
  state_.integral = ClampIntegral(integral);
}

// 执行一次更新计算。
Pid::UpdateResult Pid::Update(const UpdateInput &input)
{
  UpdateResult result {};

  if (!IsFinite(input.setpoint) || !IsFinite(input.measurement)) {
    result.output = state_.last_output;
    result.unsaturated_output = state_.last_output;
    return result;
  }

  if (input.reset_integral) {
    state_.integral = 0.0f;
  }

  const float dt_s = SanitizeDt(input.dt_s);
  const float error = input.setpoint - input.measurement;
  const bool reset_derivative = input.reset_derivative || !state_.initialized;

  // 先根据配置选择微分信号来源，再统一进入微分滤波与增益计算。
  float derivative_raw = 0.0f;
  if (reset_derivative) {
    state_.previous_error = error;
    state_.previous_measurement = input.measurement;
  } else {
    switch (config_.derivative_mode) {
      case DerivativeMode::kDisabled:
        derivative_raw = 0.0f;
        break;

      case DerivativeMode::kOnError:
        derivative_raw = (dt_s > 0.0f) ? ((error - state_.previous_error) / dt_s) : 0.0f;
        break;

      case DerivativeMode::kOnMeasurement:
        derivative_raw = (dt_s > 0.0f)
                             ? (-(input.measurement - state_.previous_measurement) / dt_s)
                             : 0.0f;
        break;

      case DerivativeMode::kOnExternalMeasurementRate:
        derivative_raw = (input.measurement_rate_valid && IsFinite(input.measurement_rate))
                             ? (-input.measurement_rate)
                             : ((dt_s > 0.0f) ? (-(input.measurement - state_.previous_measurement) / dt_s) : 0.0f);
        break;

      default:
        derivative_raw = 0.0f;
        break;
    }
  }

  if (!IsFinite(derivative_raw)) {
    derivative_raw = 0.0f;
  }

  const float derivative = config_.kd * ApplyDerivativeFilter(derivative_raw, dt_s, reset_derivative);
  const float proportional = config_.kp * error;
  const float feedforward = config_.kff * (IsFinite(input.feedforward) ? input.feedforward : 0.0f);

  if (reset_derivative) {
    state_.initialized = true;
  }

  if (input.update_integral && IsFinite(error) && IsFinite(dt_s) && (dt_s > 0.0f) &&
      (config_.ki != 0.0f)) {
    // 在输出已经朝同方向饱和时暂停继续积分，减少积分饱和带来的恢复迟滞。
    const float candidate_integral = ClampIntegral(state_.integral + (config_.ki * error * dt_s));
    const float candidate_output = proportional + candidate_integral + derivative + feedforward;

    const bool prevent_high_windup = HasValidRange(config_.output_min, config_.output_max) &&
                                     (candidate_output > config_.output_max) && (error > 0.0f);
    const bool prevent_low_windup = HasValidRange(config_.output_min, config_.output_max) &&
                                    (candidate_output < config_.output_min) && (error < 0.0f);

    if (!prevent_high_windup && !prevent_low_windup) {
      state_.integral = candidate_integral;
    }
  }

  result.error = error;
  result.proportional = proportional;
  result.integral = state_.integral;
  result.derivative = derivative;
  result.feedforward = feedforward;
  result.derivative_raw = derivative_raw;
  result.unsaturated_output = proportional + state_.integral + derivative + feedforward;
  result.output = ClampOutput(result.unsaturated_output, &result.output_clamped_low, &result.output_clamped_high);

  state_.previous_error = error;
  state_.previous_measurement = input.measurement;
  state_.last_output = result.output;
  state_.initialized = true;
  return result;
}

// 判断输入值是否为有限数。
bool Pid::IsFinite(float value)
{
  return isfinite(value) != 0;
}

// 把数值限制到给定范围。
float Pid::Clamp(float value, float lower, float upper)
{
  if (value < lower) {
    return lower;
  }

  if (value > upper) {
    return upper;
  }

  return value;
}

// 清洗非法输入值并提供回退值。
float Pid::SanitizeValue(float value, float fallback)
{
  return IsFinite(value) ? value : fallback;
}

// 确保上下界顺序合法。
void Pid::NormalizeRange(float *lower, float *upper)
{
  if ((lower == nullptr) || (upper == nullptr)) {
    return;
  }

  if (*lower > *upper) {
    const float temp = *lower;
    *lower = *upper;
    *upper = temp;
  }
}

// 检查上下界是否构成有效范围。
bool Pid::HasValidRange(float lower, float upper)
{
  return IsFinite(lower) && IsFinite(upper) && (upper >= lower);
}

// 清洗并限制控制周期。
float Pid::SanitizeDt(float dt_s) const
{
  if (!IsFinite(dt_s) || (dt_s <= 0.0f)) {
    dt_s = config_.dt_min_s;
  }

  if ((config_.dt_min_s > 0.0f) && (dt_s < config_.dt_min_s)) {
    dt_s = config_.dt_min_s;
  }

  if ((config_.dt_max_s > 0.0f) && (dt_s > config_.dt_max_s)) {
    dt_s = config_.dt_max_s;
  }

  return dt_s;
}

// 对微分项应用低通滤波。
float Pid::ApplyDerivativeFilter(float derivative_raw, float dt_s, bool reset)
{
  if (reset || !IsFinite(state_.derivative_state)) {
    state_.derivative_state = derivative_raw;
  }

  if ((config_.derivative_cutoff_hz <= 0.0f) || (dt_s <= 0.0f)) {
    state_.derivative_state = derivative_raw;
    return state_.derivative_state;
  }

  const float time_constant = 1.0f / (2.0f * kPi * config_.derivative_cutoff_hz);
  const float alpha = dt_s / (dt_s + time_constant);
  state_.derivative_state += alpha * (derivative_raw - state_.derivative_state);
  return state_.derivative_state;
}

// 对积分项执行限幅。
float Pid::ClampIntegral(float integral) const
{
  if (!IsFinite(integral)) {
    return 0.0f;
  }

  if (HasValidRange(config_.integral_min, config_.integral_max)) {
    return Clamp(integral, config_.integral_min, config_.integral_max);
  }

  return integral;
}

// 对最终输出执行限幅。
float Pid::ClampOutput(float output, bool *clamped_low, bool *clamped_high) const
{
  if (clamped_low != nullptr) {
    *clamped_low = false;
  }
  if (clamped_high != nullptr) {
    *clamped_high = false;
  }

  if (!IsFinite(output)) {
    return state_.last_output;
  }

  if (!HasValidRange(config_.output_min, config_.output_max)) {
    return output;
  }

  if (output < config_.output_min) {
    if (clamped_low != nullptr) {
      *clamped_low = true;
    }
    return config_.output_min;
  }

  if (output > config_.output_max) {
    if (clamped_high != nullptr) {
      *clamped_high = true;
    }
    return config_.output_max;
  }

  return output;
}

} // namespace iFly::PIDCtrl
