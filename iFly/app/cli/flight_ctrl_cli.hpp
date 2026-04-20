#ifndef IFLY_FLIGHT_CTRL_CLI_HPP
#define IFLY_FLIGHT_CTRL_CLI_HPP

#include <stdint.h>

#include "parameter_manager.hpp"
#include "pid.hpp"
#include "shell.hpp"
#include "tick.hpp"

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

  enum class IntroAnimationPhase : uint8_t {
    kIdle = 0U,
    kBootMessage,
    kTransportSpinner,
    kRegistrySpinner,
    kProgressBar,
    kWelcomeMessage,
    kOnlineMessage,
    kCompleted,
  };

  struct IntroAnimationState final {
    IntroAnimationPhase phase = IntroAnimationPhase::kIdle;
    tick::NonBlockingDelayNs delay {};
    uint32_t element_index = 0U;
    bool phase_started = false;
  };

  void RegisterParameters();
  void RegisterFunctions();
  void UpdateShellBanner();
  void ApplyPidConfiguration();
  void ResetIntroAnimation();
  void AdvanceIntroAnimation(IntroAnimationPhase next_phase);
  bool UpdateIntroAnimation(Shell *shell, bool start);
  bool StepTypewriterLine(Shell *shell, uint64_t now_ns, const char *text,
                          uint32_t delay_ms);
  bool StepSpinnerLine(Shell *shell, uint64_t now_ns, const char *label,
                       uint8_t rounds, uint32_t frame_delay_ms);
  bool StepProgressLine(Shell *shell, uint64_t now_ns, const char *label,
                        uint8_t steps, uint32_t step_delay_ms);

  const TransportBinding *FindTransport(const char *name) const;

  static bool GetTransportParameter(void *context, char *buffer,
                                    uint32_t bufferSize);
  static bool GetUptimeParameter(void *context, char *buffer,
                                 uint32_t bufferSize);

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
  static bool IntroAnimation(Shell *shell, void *context, bool start);

  static void OnPidParameterUpdated(void *context);

private:
  RuntimeConfig runtime_ {};
  Pid rate_pid_;
  ParameterManager parameter_manager_ {};
  Shell shell_ {};

  TransportBinding transports_[kMaxTransportCount] {};
  uint8_t transport_count_ = 0U;
  const char *active_transport_name_ = "unbound";

  char banner_subtitle_[64] {};
  IntroAnimationState intro_animation_ {};
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
