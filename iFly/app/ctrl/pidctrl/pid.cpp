#include "pid.hpp"

#include <math.h>

namespace iFly::pidctrl {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

Pid::Pid(const Config &config) noexcept
{
  Configure(config);
}

void Pid::Configure(const Config &config) noexcept
{
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

void Pid::Reset() noexcept
{
  state_ = State {};
}

void Pid::ResetIntegrator(float integral) noexcept
{
  state_.integral = ClampIntegral(integral);
}

void Pid::SetIntegrator(float integral) noexcept
{
  state_.integral = ClampIntegral(integral);
}

Pid::UpdateResult Pid::Update(const UpdateInput &input) noexcept
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

bool Pid::IsFinite(float value) noexcept
{
  return isfinite(value) != 0;
}

float Pid::Clamp(float value, float lower, float upper) noexcept
{
  if (value < lower) {
    return lower;
  }

  if (value > upper) {
    return upper;
  }

  return value;
}

float Pid::SanitizeValue(float value, float fallback) noexcept
{
  return IsFinite(value) ? value : fallback;
}

void Pid::NormalizeRange(float *lower, float *upper) noexcept
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

bool Pid::HasValidRange(float lower, float upper) noexcept
{
  return IsFinite(lower) && IsFinite(upper) && (upper >= lower);
}

float Pid::SanitizeDt(float dt_s) const noexcept
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

float Pid::ApplyDerivativeFilter(float derivative_raw, float dt_s, bool reset) noexcept
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

float Pid::ClampIntegral(float integral) const noexcept
{
  if (!IsFinite(integral)) {
    return 0.0f;
  }

  if (HasValidRange(config_.integral_min, config_.integral_max)) {
    return Clamp(integral, config_.integral_min, config_.integral_max);
  }

  return integral;
}

float Pid::ClampOutput(float output, bool *clamped_low, bool *clamped_high) const noexcept
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

} // namespace iFly::pidctrl
