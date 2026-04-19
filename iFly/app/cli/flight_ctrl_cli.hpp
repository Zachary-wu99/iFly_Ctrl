#ifndef IFLY_FLIGHT_CTRL_CLI_HPP
#define IFLY_FLIGHT_CTRL_CLI_HPP

#include <stdint.h>

#include "pid.hpp"
#include "shell.hpp"

namespace iFly {

class FlightCtrlCli final {
public:
  static constexpr uint8_t kMaxTransportCount = 4U;

  FlightCtrlCli();

  void Init();

  bool RegisterTransport(const char *name, SerialIoBase *io);
  bool UseTransport(const char *name);

  void Poll();

  Shell &Console() {
    return shell_;
  }

  const Shell &Console() const {
    return shell_;
  }

private:
  struct RuntimeConfig final {
    uint32_t control_loop_hz = 1000U;
    bool arm_locked = true;
    Pid::Config rate_pid {
        0.8f,
        0.1f,
        0.02f,
        0.0f,
        -100.0f,
        100.0f,
        -500.0f,
        500.0f,
        30.0f,
        5.0e-4f,
        2.0e-2f,
        Pid::DerivativeMode::kOnMeasurement};
  };

  struct TransportBinding final {
    const char *name = nullptr;
    SerialIoBase *io = nullptr;
  };

  struct FloatParameterBinding final {
    FlightCtrlCli *owner = nullptr;
    float *value = nullptr;
    float min_value = 0.0f;
    float max_value = 0.0f;
    bool has_range = false;
    void (*on_updated)(FlightCtrlCli *owner) = nullptr;
  };

  struct U32ParameterBinding final {
    FlightCtrlCli *owner = nullptr;
    uint32_t *value = nullptr;
    uint32_t min_value = 0U;
    uint32_t max_value = 0U;
    bool has_range = false;
    void (*on_updated)(FlightCtrlCli *owner) = nullptr;
  };

  struct BoolParameterBinding final {
    FlightCtrlCli *owner = nullptr;
    bool *value = nullptr;
    void (*on_updated)(FlightCtrlCli *owner) = nullptr;
  };

  void RegisterParameters();
  void RegisterFunctions();
  void RegisterReadonlyParameters();
  void UpdateShellBanner();
  void ApplyPidConfiguration();

  const TransportBinding *FindTransport(const char *name) const;

  static bool GetFloatParameter(void *context, char *buffer, uint32_t bufferSize);
  static bool SetFloatParameter(void *context, const char *value);
  static bool GetU32Parameter(void *context, char *buffer, uint32_t bufferSize);
  static bool SetU32Parameter(void *context, const char *value);
  static bool GetBoolParameter(void *context, char *buffer, uint32_t bufferSize);
  static bool SetBoolParameter(void *context, const char *value);
  static bool GetTransportParameter(void *context, char *buffer, uint32_t bufferSize);
  static bool GetUptimeParameter(void *context, char *buffer, uint32_t bufferSize);

  static bool StatusFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  static bool RebootFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  static bool PidResetFunction(Shell *shell, void *context, uint8_t argc,
                               const char *const *argv);
  static bool PidSampleFunction(Shell *shell, void *context, uint8_t argc,
                                const char *const *argv);
  static bool TransportListFunction(Shell *shell, void *context, uint8_t argc,
                                    const char *const *argv);
  static bool TransportUseFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv);
  static void IntroAnimation(Shell *shell, void *context);

  static void OnPidParameterUpdated(FlightCtrlCli *owner);

private:
  RuntimeConfig runtime_ {};
  Pid rate_pid_;
  Shell shell_ {};

  TransportBinding transports_[kMaxTransportCount] {};
  uint8_t transport_count_ = 0U;
  const char *active_transport_name_ = "unbound";

  char banner_subtitle_[64] {};

  FloatParameterBinding pid_kp_ {};
  FloatParameterBinding pid_ki_ {};
  FloatParameterBinding pid_kd_ {};
  FloatParameterBinding pid_kff_ {};
  FloatParameterBinding pid_integral_min_ {};
  FloatParameterBinding pid_integral_max_ {};
  FloatParameterBinding pid_output_min_ {};
  FloatParameterBinding pid_output_max_ {};
  FloatParameterBinding pid_cutoff_hz_ {};
  U32ParameterBinding control_loop_hz_ {};
  BoolParameterBinding arm_locked_ {};
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
