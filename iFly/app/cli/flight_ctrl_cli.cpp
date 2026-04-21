#include "flight_ctrl_cli.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

namespace iFly {

namespace {

constexpr char kCliPrompt[] = "iFly> ";
constexpr char kDefaultCliPassword[] = "ifly";
constexpr char kActivationPrompt[] =
    "Press SPACE to enter the iFly secure terminal.";
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ULL;
constexpr char kSpinnerFrames[4] = {'|', '/', '-', '\\'};

bool ParseFloat(const char *text, float *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  char *end = nullptr;
  const float parsed = strtof(text, &end);
  if ((end == text) || (end == nullptr)) {
    return false;
  }

  while ((*end == ' ') || (*end == '\t')) {
    ++end;
  }

  if ((*end != '\0') || (isfinite(parsed) == 0)) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParseUint32(const char *text, uint32_t *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if ((end == text) || (end == nullptr)) {
    return false;
  }

  while ((*end == ' ') || (*end == '\t')) {
    ++end;
  }

  if (*end != '\0') {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool CharEqualsIgnoreCase(char left, char right)
{
  if ((left >= 'A') && (left <= 'Z')) {
    left = static_cast<char>(left - 'A' + 'a');
  }
  if ((right >= 'A') && (right <= 'Z')) {
    right = static_cast<char>(right - 'A' + 'a');
  }
  return left == right;
}

bool EqualsIgnoreCase(const char *left, const char *right)
{
  if ((left == nullptr) || (right == nullptr)) {
    return false;
  }

  while ((*left != '\0') && (*right != '\0')) {
    if (!CharEqualsIgnoreCase(*left, *right)) {
      return false;
    }
    ++left;
    ++right;
  }

  return (*left == '\0') && (*right == '\0');
}

bool ParseBool(const char *text, bool *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  if (EqualsIgnoreCase(text, "1") ||
      EqualsIgnoreCase(text, "true") ||
      EqualsIgnoreCase(text, "on") ||
      EqualsIgnoreCase(text, "yes")) {
    *value = true;
    return true;
  }

  if (EqualsIgnoreCase(text, "0") ||
      EqualsIgnoreCase(text, "false") ||
      EqualsIgnoreCase(text, "off") ||
      EqualsIgnoreCase(text, "no")) {
    *value = false;
    return true;
  }

  return false;
}

uint64_t MsToNs(uint32_t value_ms)
{
  return static_cast<uint64_t>(value_ms) * kNanosecondsPerMillisecond;
}

bool IsPidParameterName(const char *name)
{
  constexpr char kPrefix[] = "control.rate_pid";
  constexpr uint32_t kPrefixLength = sizeof(kPrefix) - 1U;

  if (name == nullptr) {
    return false;
  }

  if (strncmp(name, kPrefix, kPrefixLength) != 0) {
    return false;
  }

  return (name[kPrefixLength] == '\0') || (name[kPrefixLength] == '.');
}

const char *ConfiguredCliPassword(const ProjectParameters &parameters)
{
  return (parameters.cli.password[0] != '\0') ? parameters.cli.password
                                              : kDefaultCliPassword;
}

} // namespace

FlightCtrlCli::FlightCtrlCli()
    : parameter_manager_(ProjectParameterManager::Instance()),
      rate_pid_(parameter_manager_.Data().control.rate_pid)
{
  ResetIntroAnimation();
}

void FlightCtrlCli::Init()
{
  parameter_manager_.ResetToDefaults();
  ResetIntroAnimation();
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.SetPassword(ConfiguredCliPassword(parameter_manager_.Data()));
  shell_.SetActivationKey(' ', kActivationPrompt);
  shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this);
  active_transport_name_ = "unbound";
  UpdateShellBanner();
  RegisterParameters();
  RegisterFunctions();
  ApplyPidConfiguration();
}

bool FlightCtrlCli::RegisterTransport(const char *name, SerialIoBase *io)
{
  if ((name == nullptr) || (io == nullptr) ||
      (transport_count_ >= kMaxTransportCount)) {
    return false;
  }

  if (FindTransport(name) != nullptr) {
    return false;
  }

  transports_[transport_count_].name = name;
  transports_[transport_count_].io = io;
  ++transport_count_;
  return true;
}

bool FlightCtrlCli::UseTransport(const char *name)
{
  const TransportBinding *binding = FindTransport(name);
  if (binding == nullptr) {
    return false;
  }

  active_transport_name_ = binding->name;
  UpdateShellBanner();
  shell_.BindIo(binding->io);
  return true;
}

void FlightCtrlCli::Poll()
{
  shell_.Poll();
}

void FlightCtrlCli::RegisterParameters()
{
  uint8_t index = 0U;
  auto register_managed_parameter =
      [this, &index](const char *shell_name,
                     const char *help,
                     const char *project_name,
                     ManagedParameterType type,
                     float min_float,
                     float max_float,
                     uint32_t min_u32,
                     uint32_t max_u32) {
        if ((shell_name == nullptr) || (project_name == nullptr) ||
            (index >= kManagedParameterCount)) {
          return;
        }

        ManagedParameterContext &context = managed_parameter_contexts_[index];
        context = ManagedParameterContext {};
        context.owner = this;
        context.project_name = project_name;
        context.type = type;
        context.min_float = min_float;
        context.max_float = max_float;
        context.min_u32 = min_u32;
        context.max_u32 = max_u32;

        (void)shell_.RegisterParameter(
            {shell_name,
             help,
             &FlightCtrlCli::GetManagedParameter,
             &FlightCtrlCli::SetManagedParameter,
             &context});
        ++index;
      };

  register_managed_parameter("pid.kp", "rate PID proportional gain",
                             "control.rate_pid.kp", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("pid.ki", "rate PID integral gain",
                             "control.rate_pid.ki", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("pid.kd", "rate PID derivative gain",
                             "control.rate_pid.kd", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("pid.kff", "rate PID feedforward gain",
                             "control.rate_pid.kff", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("pid.i_min", "rate PID integral lower limit",
                             "control.rate_pid.integral_min",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("pid.i_max", "rate PID integral upper limit",
                             "control.rate_pid.integral_max",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("pid.out_min", "rate PID output lower limit",
                             "control.rate_pid.output_min",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("pid.out_max", "rate PID output upper limit",
                             "control.rate_pid.output_max",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("pid.d_cutoff_hz", "rate PID derivative LPF cutoff",
                             "control.rate_pid.derivative_cutoff_hz",
                             ManagedParameterType::kFloat, 0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("sys.loop_hz", "control loop frequency",
                             "system.control_loop_hz",
                             ManagedParameterType::kUint32, 0.0f, 0.0f,
                             50U, 4000U);
  register_managed_parameter("sys.arm_locked", "arming lock switch",
                             "system.arm_locked",
                             ManagedParameterType::kBool, 0.0f, 0.0f, 0U, 0U);

  (void)shell_.RegisterParameter(
      {"sys.transport", "active CLI transport",
       &FlightCtrlCli::GetTransportParameter, nullptr, this});
  (void)shell_.RegisterParameter(
      {"sys.uptime_ms", "system uptime in milliseconds",
       &FlightCtrlCli::GetUptimeParameter, nullptr, this});

  (void)parameter_manager_.SetChangeHandler("control.rate_pid.kp",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.ki",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.kd",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.kff",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.integral_min",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.integral_max",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.output_min",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.output_max",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("control.rate_pid.derivative_cutoff_hz",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
  (void)parameter_manager_.SetChangeHandler("cli.password",
                                            &FlightCtrlCli::OnProjectParameterUpdated,
                                            this);
}

void FlightCtrlCli::RegisterFunctions()
{
  (void)shell_.RegisterFunction(
      {"status", "print flight-controller status", &FlightCtrlCli::StatusFunction,
       this});
  (void)shell_.RegisterFunction(
      {"sys.reboot", "trigger MCU software reset",
       &FlightCtrlCli::RebootFunction, this});
  (void)shell_.RegisterFunction(
      {"pid.reset", "reset PID runtime state", &FlightCtrlCli::PidResetFunction,
       this});
  (void)shell_.RegisterFunction(
      {"pid.sample", "run one PID update: <sp> <meas> <dt_ms>",
       &FlightCtrlCli::PidSampleFunction, this});
  (void)shell_.RegisterFunction(
      {"transport.list", "list registered CLI transports",
       &FlightCtrlCli::TransportListFunction, this});
  (void)shell_.RegisterFunction(
      {"transport.use", "switch CLI transport: <name>",
       &FlightCtrlCli::TransportUseFunction, this});
}

void FlightCtrlCli::UpdateShellBanner()
{
  const int written = snprintf(
      banner_subtitle_, sizeof(banner_subtitle_),
      "iFly flight controller CLI | transport=%s",
      (active_transport_name_ != nullptr) ? active_transport_name_ : "unbound");
  if ((written <= 0) ||
      (static_cast<uint32_t>(written) >= sizeof(banner_subtitle_))) {
    banner_subtitle_[0] = '\0';
  }

  shell_.SetBanner("iFly Flight Controller", banner_subtitle_);
}

void FlightCtrlCli::ApplyPidConfiguration()
{
  rate_pid_.Configure(parameter_manager_.Data().control.rate_pid);

  const Pid::Config &sanitized = rate_pid_.GetConfig();
  if (memcmp(&parameter_manager_.Data().control.rate_pid,
             &sanitized,
             sizeof(Pid::Config)) != 0) {
    (void)parameter_manager_.Write("control.rate_pid", sanitized);
  }
}

void FlightCtrlCli::ResetIntroAnimation()
{
  intro_animation_.phase = IntroAnimationPhase::kIdle;
  intro_animation_.delay.Reset();
  intro_animation_.element_index = 0U;
  intro_animation_.phase_started = false;
}

void FlightCtrlCli::AdvanceIntroAnimation(IntroAnimationPhase next_phase)
{
  intro_animation_.phase = next_phase;
  intro_animation_.delay.Reset();
  intro_animation_.element_index = 0U;
  intro_animation_.phase_started = false;
}

bool FlightCtrlCli::StepTypewriterLine(Shell *shell, uint64_t now_ns,
                                       const char *text, uint32_t delay_ms)
{
  if ((shell == nullptr) || (text == nullptr)) {
    return true;
  }

  const uint64_t delay_ns = MsToNs(delay_ms);
  const uint32_t text_length = static_cast<uint32_t>(strlen(text));

  if (!intro_animation_.phase_started) {
    intro_animation_.phase_started = true;
    intro_animation_.element_index = 0U;
    intro_animation_.delay.Reset();
  }

  if (intro_animation_.element_index >= text_length) {
    if (!intro_animation_.delay.IsActive()) {
      intro_animation_.delay.StartFrom(now_ns, delay_ns);
      return false;
    }

    if (!intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
      return false;
    }

    shell->WriteLine("");
    return true;
  }

  if (intro_animation_.delay.IsActive() &&
      !intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
    return false;
  }

  char chunk[2] = {text[intro_animation_.element_index], '\0'};
  shell->Write(chunk);
  ++intro_animation_.element_index;
  intro_animation_.delay.StartFrom(now_ns, delay_ns);
  return false;
}

bool FlightCtrlCli::StepSpinnerLine(Shell *shell, uint64_t now_ns,
                                    const char *label, uint8_t rounds,
                                    uint32_t frame_delay_ms)
{
  if ((shell == nullptr) || (label == nullptr) || (rounds == 0U)) {
    return true;
  }

  const uint64_t frame_delay_ns = MsToNs(frame_delay_ms);
  if (!intro_animation_.phase_started) {
    intro_animation_.phase_started = true;
    intro_animation_.element_index = 0U;
    shell->Printf("%s ", label);
  }

  if (intro_animation_.element_index >= rounds) {
    if (!intro_animation_.delay.IsActive()) {
      intro_animation_.delay.StartFrom(now_ns, frame_delay_ns);
      return false;
    }

    if (!intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
      return false;
    }

    shell->Write("\b");
    shell->WriteLine("OK");
    return true;
  }

  if (intro_animation_.delay.IsActive() &&
      !intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
    return false;
  }

  if (intro_animation_.element_index > 0U) {
    shell->Write("\b");
  }

  const char frame[2] = {kSpinnerFrames[intro_animation_.element_index & 0x03U],
                         '\0'};
  shell->Write(frame);
  ++intro_animation_.element_index;
  intro_animation_.delay.StartFrom(now_ns, frame_delay_ns);
  return false;
}

bool FlightCtrlCli::StepProgressLine(Shell *shell, uint64_t now_ns,
                                     const char *label, uint8_t steps,
                                     uint32_t step_delay_ms)
{
  if ((shell == nullptr) || (label == nullptr) || (steps == 0U)) {
    return true;
  }

  const uint64_t step_delay_ns = MsToNs(step_delay_ms);
  if (!intro_animation_.phase_started) {
    intro_animation_.phase_started = true;
    intro_animation_.element_index = 0U;
    shell->Printf("%s [", label);
  }

  if (intro_animation_.element_index >= steps) {
    if (!intro_animation_.delay.IsActive()) {
      intro_animation_.delay.StartFrom(now_ns, step_delay_ns);
      return false;
    }

    if (!intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
      return false;
    }

    shell->WriteLine("]");
    return true;
  }

  if (intro_animation_.delay.IsActive() &&
      !intro_animation_.delay.ConsumeIfExpiredAt(now_ns)) {
    return false;
  }

  shell->Write("#");
  ++intro_animation_.element_index;
  intro_animation_.delay.StartFrom(now_ns, step_delay_ns);
  return false;
}

bool FlightCtrlCli::UpdateIntroAnimation(Shell *shell, bool start)
{
  if (shell == nullptr) {
    return true;
  }

  if (!start) {
    return true;
  }

  ResetIntroAnimation();
  shell->Write("\x1B[2J\x1B[H");
  shell->WriteLine("");
  shell->WriteLine("        ___  ________           ");
  shell->WriteLine("       / _ \\/ __/ / /_ _____    ");
  shell->WriteLine("      / , _/ _// / / // / -_)   ");
  shell->WriteLine("     /_/|_/___/_/_/\\_, /\\__/    ");
  shell->WriteLine("                   /___/        ");
  shell->WriteLine("");
  shell->WriteLine("Booting iFly secure terminal...");
  shell->WriteLine("Welcome to the iFly Flight Controller.");
  shell->WriteLine("English terminal mode is now online.");
  shell->WriteLine("");
  return true;
}

const FlightCtrlCli::TransportBinding *FlightCtrlCli::FindTransport(
    const char *name) const
{
  if (name == nullptr) {
    return nullptr;
  }

  for (uint8_t index = 0U; index < transport_count_; ++index) {
    if ((transports_[index].name != nullptr) &&
        (strcmp(transports_[index].name, name) == 0)) {
      return &transports_[index];
    }
  }

  return nullptr;
}

bool FlightCtrlCli::GetTransportParameter(void *context, char *buffer,
                                          uint32_t bufferSize)
{
  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((cli == nullptr) || (buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written =
      snprintf(buffer, bufferSize, "%s",
               (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                        : "unbound");
  return (written > 0) &&
         (static_cast<uint32_t>(written) < bufferSize);
}

bool FlightCtrlCli::GetUptimeParameter(void *context, char *buffer,
                                       uint32_t bufferSize)
{
  (void)context;

  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%lu",
                               static_cast<unsigned long>(tick::NowMs()));
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool FlightCtrlCli::GetManagedParameter(void *context, char *buffer,
                                        uint32_t bufferSize)
{
  ManagedParameterContext *parameter =
      reinterpret_cast<ManagedParameterContext *>(context);
  if ((parameter == nullptr) || (parameter->owner == nullptr) ||
      (parameter->project_name == nullptr) || (buffer == nullptr) ||
      (bufferSize == 0U)) {
    return false;
  }

  int written = 0;
  switch (parameter->type) {
    case ManagedParameterType::kFloat: {
      float value = 0.0f;
      if (!parameter->owner->parameter_manager_.Read(parameter->project_name, &value)) {
        return false;
      }
      written = snprintf(buffer, bufferSize, "%.6g", static_cast<double>(value));
      break;
    }

    case ManagedParameterType::kUint32: {
      uint32_t value = 0U;
      if (!parameter->owner->parameter_manager_.Read(parameter->project_name, &value)) {
        return false;
      }
      written = snprintf(buffer, bufferSize, "%lu",
                         static_cast<unsigned long>(value));
      break;
    }

    case ManagedParameterType::kBool: {
      bool value = false;
      if (!parameter->owner->parameter_manager_.Read(parameter->project_name, &value)) {
        return false;
      }
      written = snprintf(buffer, bufferSize, "%s", value ? "true" : "false");
      break;
    }

    default:
      return false;
  }

  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool FlightCtrlCli::SetManagedParameter(void *context, const char *value)
{
  ManagedParameterContext *parameter =
      reinterpret_cast<ManagedParameterContext *>(context);
  if ((parameter == nullptr) || (parameter->owner == nullptr) ||
      (parameter->project_name == nullptr) || (value == nullptr)) {
    return false;
  }

  switch (parameter->type) {
    case ManagedParameterType::kFloat: {
      float parsed = 0.0f;
      if (!ParseFloat(value, &parsed) ||
          (parsed < parameter->min_float) ||
          (parsed > parameter->max_float)) {
        return false;
      }
      return parameter->owner->parameter_manager_.Write(parameter->project_name, parsed);
    }

    case ManagedParameterType::kUint32: {
      uint32_t parsed = 0U;
      if (!ParseUint32(value, &parsed) ||
          (parsed < parameter->min_u32) ||
          (parsed > parameter->max_u32)) {
        return false;
      }
      return parameter->owner->parameter_manager_.Write(parameter->project_name, parsed);
    }

    case ManagedParameterType::kBool: {
      bool parsed = false;
      if (!ParseBool(value, &parsed)) {
        return false;
      }
      return parameter->owner->parameter_manager_.Write(parameter->project_name, parsed);
    }

    default:
      return false;
  }
}

bool FlightCtrlCli::StatusFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv)
{
  (void)argv;

  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call status");
    }
    return false;
  }

  const ProjectParameters &parameters = cli->parameter_manager_.Data();
  const Pid::State &pid_state = cli->rate_pid_.GetState();
  shell->WriteLine("Flight Controller Status");
  shell->Printf("  transport     : %s\r\n",
                (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                         : "unbound");
  shell->Printf("  shell_link    : %s\r\n",
                cli->shell_.IsConnected() ? "connected" : "disconnected");
  shell->Printf("  uptime_ms     : %lu\r\n",
                static_cast<unsigned long>(tick::NowMs()));
  shell->Printf("  loop_hz       : %lu\r\n",
                static_cast<unsigned long>(parameters.system.control_loop_hz));
  shell->Printf("  arm_locked    : %s\r\n",
                parameters.system.arm_locked ? "true" : "false");
  shell->Printf("  pid_kp        : %.6g\r\n",
                static_cast<double>(parameters.control.rate_pid.kp));
  shell->Printf("  pid_ki        : %.6g\r\n",
                static_cast<double>(parameters.control.rate_pid.ki));
  shell->Printf("  pid_kd        : %.6g\r\n",
                static_cast<double>(parameters.control.rate_pid.kd));
  shell->Printf("  pid_integral  : %.6g\r\n",
                static_cast<double>(pid_state.integral));
  shell->Printf("  pid_last_out  : %.6g\r\n",
                static_cast<double>(pid_state.last_output));
  return true;
}

bool FlightCtrlCli::RebootFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv)
{
  (void)context;
  (void)argv;

  if ((shell == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call sys.reboot");
    }
    return false;
  }

  shell->WriteLine("System reboot requested.");
  NVIC_SystemReset();
  return true;
}

bool FlightCtrlCli::PidResetFunction(Shell *shell, void *context, uint8_t argc,
                                     const char *const *argv)
{
  (void)argv;

  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call pid.reset");
    }
    return false;
  }

  cli->rate_pid_.Reset();
  shell->WriteLine("PID runtime state reset.");
  return true;
}

bool FlightCtrlCli::PidSampleFunction(Shell *shell, void *context, uint8_t argc,
                                      const char *const *argv)
{
  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 3U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call pid.sample <setpoint> <measurement> <dt_ms>");
    }
    return false;
  }

  float setpoint = 0.0f;
  float measurement = 0.0f;
  float dt_ms = 0.0f;
  if (!ParseFloat(argv[0], &setpoint) || !ParseFloat(argv[1], &measurement) ||
      !ParseFloat(argv[2], &dt_ms) || (dt_ms <= 0.0f)) {
    shell->WriteLine("Invalid pid.sample arguments.");
    return false;
  }

  Pid::UpdateInput input {};
  input.setpoint = setpoint;
  input.measurement = measurement;
  input.dt_s = dt_ms * 0.001f;

  const Pid::UpdateResult result = cli->rate_pid_.Update(input);
  shell->Printf(
      "output=%.6g unsat=%.6g err=%.6g p=%.6g i=%.6g d=%.6g ff=%.6g\r\n",
      static_cast<double>(result.output),
      static_cast<double>(result.unsaturated_output),
      static_cast<double>(result.error),
      static_cast<double>(result.proportional),
      static_cast<double>(result.integral),
      static_cast<double>(result.derivative),
      static_cast<double>(result.feedforward));
  return true;
}

bool FlightCtrlCli::TransportListFunction(Shell *shell, void *context,
                                          uint8_t argc,
                                          const char *const *argv)
{
  (void)argv;

  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call transport.list");
    }
    return false;
  }

  shell->WriteLine("Registered transports:");
  if (cli->transport_count_ == 0U) {
    shell->WriteLine("  (none)");
    return true;
  }

  for (uint8_t index = 0U; index < cli->transport_count_; ++index) {
    const bool is_active =
        (cli->active_transport_name_ != nullptr) &&
        (strcmp(cli->active_transport_name_, cli->transports_[index].name) == 0);
    shell->Printf("  %s%s\r\n", cli->transports_[index].name,
                  is_active ? " [active]" : "");
  }
  return true;
}

bool FlightCtrlCli::TransportUseFunction(Shell *shell, void *context,
                                         uint8_t argc,
                                         const char *const *argv)
{
  FlightCtrlCli *cli = reinterpret_cast<FlightCtrlCli *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 1U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call transport.use <name>");
    }
    return false;
  }

  if (cli->FindTransport(argv[0]) == nullptr) {
    shell->Printf("Unknown transport: %s\r\n", argv[0]);
    return false;
  }

  shell->Printf("Switching CLI transport to %s\r\n", argv[0]);
  shell->WriteLine("Reconnect on the selected port.");
  return cli->UseTransport(argv[0]);
}

bool FlightCtrlCli::IntroAnimation(Shell *shell, void *context, bool start)
{
  FlightCtrlCli *owner = reinterpret_cast<FlightCtrlCli *>(context);
  if (owner == nullptr) {
    return true;
  }

  return owner->UpdateIntroAnimation(shell, start);
}

void FlightCtrlCli::OnProjectParameterUpdated(const char *name, void *context)
{
  FlightCtrlCli *owner = reinterpret_cast<FlightCtrlCli *>(context);
  if ((owner == nullptr) || (name == nullptr)) {
    return;
  }

  if (IsPidParameterName(name)) {
    owner->ApplyPidConfiguration();
    return;
  }

  if (strcmp(name, "cli.password") == 0) {
    owner->shell_.SetPassword(ConfiguredCliPassword(owner->parameter_manager_.Data()));
  }
}

} // namespace iFly
