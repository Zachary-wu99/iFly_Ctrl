#ifndef IFLY_FLIGHT_CTRL_CLI_HPP
#define IFLY_FLIGHT_CTRL_CLI_HPP

#include <stdint.h>

#include "pid.hpp"
#include "project_parameter_manager.hpp"
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
  static constexpr uint8_t kManagedParameterCount = 11U;

  struct TransportBinding final {
    const char *name = nullptr;
    SerialIoBase *io = nullptr;
  };

  enum class ManagedParameterType : uint8_t {
    kFloat = 0U,
    kUint32,
    kBool,
  };

  struct ManagedParameterContext final {
    FlightCtrlCli *owner = nullptr;
    const char *project_name = nullptr;
    ManagedParameterType type = ManagedParameterType::kFloat;
    float min_float = 0.0f;
    float max_float = 0.0f;
    uint32_t min_u32 = 0U;
    uint32_t max_u32 = 0U;
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
  static bool GetManagedParameter(void *context, char *buffer,
                                  uint32_t bufferSize);
  static bool SetManagedParameter(void *context, const char *value);

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
  static void OnProjectParameterUpdated(const char *name, void *context);

private:
  ProjectParameterManager &parameter_manager_;
  Pid rate_pid_;
  Shell shell_ {};
  ManagedParameterContext managed_parameter_contexts_[kManagedParameterCount] {};

  TransportBinding transports_[kMaxTransportCount] {};
  uint8_t transport_count_ = 0U;
  const char *active_transport_name_ = "unbound";

  char banner_subtitle_[64] {};
  IntroAnimationState intro_animation_ {};
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
