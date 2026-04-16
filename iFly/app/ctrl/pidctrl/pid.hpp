#ifndef IFLY_APP_CTRL_PIDCTRL_PID_HPP
#define IFLY_APP_CTRL_PIDCTRL_PID_HPP

#include <stdint.h>

namespace iFly::pidctrl {

// Reusable PID controller inspired by PX4's generic controller structure:
// derivative-on-measurement by default, filtered D-term, integral clamping and saturation-aware anti-windup.
class Pid final {
public:
  enum class DerivativeMode : uint8_t {
    kDisabled = 0U,
    kOnError,
    kOnMeasurement,
    kOnExternalMeasurementRate,
  };

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

  struct UpdateInput final {
    float setpoint = 0.0f;
    float measurement = 0.0f;
    float dt_s = 0.0f;

    float feedforward = 0.0f;
    // Positive measurement_rate means the measured value is increasing.
    // In derivative-on-measurement mode the controller will apply the negative sign internally.
    float measurement_rate = 0.0f;
    bool measurement_rate_valid = false;

    bool update_integral = true;
    bool reset_integral = false;
    bool reset_derivative = false;
  };

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

  struct State final {
    float integral = 0.0f;
    float previous_error = 0.0f;
    float previous_measurement = 0.0f;
    float derivative_state = 0.0f;
    float last_output = 0.0f;
    bool initialized = false;
  };

  Pid() noexcept = default;
  explicit Pid(const Config &config) noexcept;

  void Configure(const Config &config) noexcept;
  const Config &GetConfig() const noexcept {
    return config_;
  }

  const State &GetState() const noexcept {
    return state_;
  }

  void Reset() noexcept;
  void ResetIntegrator(float integral = 0.0f) noexcept;
  void SetIntegrator(float integral) noexcept;
  float Integrator() const noexcept {
    return state_.integral;
  }

  UpdateResult Update(const UpdateInput &input) noexcept;

private:
  static bool IsFinite(float value) noexcept;
  static float Clamp(float value, float lower, float upper) noexcept;
  static float SanitizeValue(float value, float fallback) noexcept;
  static void NormalizeRange(float *lower, float *upper) noexcept;
  static bool HasValidRange(float lower, float upper) noexcept;

  float SanitizeDt(float dt_s) const noexcept;
  float ApplyDerivativeFilter(float derivative_raw, float dt_s, bool reset) noexcept;
  float ClampIntegral(float integral) const noexcept;
  float ClampOutput(float output, bool *clamped_low, bool *clamped_high) const noexcept;

private:
  Config config_ {};
  State state_ {};
};

} // namespace iFly::pidctrl

#endif /* IFLY_APP_CTRL_PIDCTRL_PID_HPP */
