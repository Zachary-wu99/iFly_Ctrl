#include "flight_ctrl_cli.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

namespace iFly {

namespace {

constexpr char kCliPrompt[] = "fc> ";
constexpr char kCliPassword[] = "ifly";
constexpr char kActivationPrompt[] =
    "Press SPACE to enter the iFly secure terminal.";

void WriteDelay(Shell *shell, const char *text, uint32_t charDelayMs)
{
  if ((shell == nullptr) || (text == nullptr)) {
    return;
  }

  uint32_t index = 0U;
  while (text[index] != '\0') {
    char chunk[2] = {text[index], '\0'};
    shell->Write(chunk);
    ++index;
    if (charDelayMs > 0U) {
      HAL_Delay(charDelayMs);
    }
  }
}

void WriteSpinner(Shell *shell, const char *label, uint8_t rounds,
                  uint32_t frameDelayMs)
{
  static const char frames[4] = {'|', '/', '-', '\\'};

  if ((shell == nullptr) || (label == nullptr) || (rounds == 0U)) {
    return;
  }

  shell->Printf("%s ", label);

  uint8_t round = 0U;
  for (round = 0U; round < rounds; ++round) {
    const char frame[2] = {frames[round & 0x03U], '\0'};
    shell->Write(frame);
    HAL_Delay(frameDelayMs);
    shell->Write("\b");
  }

  shell->WriteLine("OK");
}

void WriteProgressBar(Shell *shell, const char *label, uint8_t steps,
                      uint32_t stepDelayMs)
{
  if ((shell == nullptr) || (label == nullptr) || (steps == 0U)) {
    return;
  }

  shell->Printf("%s [", label);
  uint8_t step = 0U;
  for (step = 0U; step < steps; ++step) {
    shell->Write("#");
    HAL_Delay(stepDelayMs);
  }
  shell->WriteLine("]");
}

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

bool ParseU32(const char *text, uint32_t *value)
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

  if ((*end != '\0') || (parsed > 0xFFFFFFFFUL)) {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool ParseBool(const char *text, bool *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  if ((strcmp(text, "1") == 0) || (strcmp(text, "true") == 0) ||
      (strcmp(text, "on") == 0) || (strcmp(text, "yes") == 0) ||
      (strcmp(text, "lock") == 0)) {
    *value = true;
    return true;
  }

  if ((strcmp(text, "0") == 0) || (strcmp(text, "false") == 0) ||
      (strcmp(text, "off") == 0) || (strcmp(text, "no") == 0) ||
      (strcmp(text, "unlock") == 0)) {
    *value = false;
    return true;
  }

  return false;
}

bool FormatFloat(char *buffer, uint32_t bufferSize, float value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%.6g",
                               static_cast<double>(value));
  return (written > 0) &&
         (static_cast<uint32_t>(written) < bufferSize);
}

bool FormatU32(char *buffer, uint32_t bufferSize, uint32_t value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%lu",
                               static_cast<unsigned long>(value));
  return (written > 0) &&
         (static_cast<uint32_t>(written) < bufferSize);
}

bool FormatBool(char *buffer, uint32_t bufferSize, bool value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%s", value ? "true" : "false");
  return (written > 0) &&
         (static_cast<uint32_t>(written) < bufferSize);
}

} // namespace

FlightCtrlCli::FlightCtrlCli()
    : rate_pid_(runtime_.rate_pid)
{
  pid_kp_.owner = this;
  pid_kp_.value = &runtime_.rate_pid.kp;
  pid_kp_.min_value = 0.0f;
  pid_kp_.max_value = 1000.0f;
  pid_kp_.has_range = true;
  pid_kp_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_ki_.owner = this;
  pid_ki_.value = &runtime_.rate_pid.ki;
  pid_ki_.min_value = 0.0f;
  pid_ki_.max_value = 1000.0f;
  pid_ki_.has_range = true;
  pid_ki_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_kd_.owner = this;
  pid_kd_.value = &runtime_.rate_pid.kd;
  pid_kd_.min_value = 0.0f;
  pid_kd_.max_value = 1000.0f;
  pid_kd_.has_range = true;
  pid_kd_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_kff_.owner = this;
  pid_kff_.value = &runtime_.rate_pid.kff;
  pid_kff_.min_value = 0.0f;
  pid_kff_.max_value = 1000.0f;
  pid_kff_.has_range = true;
  pid_kff_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_integral_min_.owner = this;
  pid_integral_min_.value = &runtime_.rate_pid.integral_min;
  pid_integral_min_.min_value = -1000000.0f;
  pid_integral_min_.max_value = 1000000.0f;
  pid_integral_min_.has_range = true;
  pid_integral_min_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_integral_max_.owner = this;
  pid_integral_max_.value = &runtime_.rate_pid.integral_max;
  pid_integral_max_.min_value = -1000000.0f;
  pid_integral_max_.max_value = 1000000.0f;
  pid_integral_max_.has_range = true;
  pid_integral_max_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_output_min_.owner = this;
  pid_output_min_.value = &runtime_.rate_pid.output_min;
  pid_output_min_.min_value = -1000000.0f;
  pid_output_min_.max_value = 1000000.0f;
  pid_output_min_.has_range = true;
  pid_output_min_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_output_max_.owner = this;
  pid_output_max_.value = &runtime_.rate_pid.output_max;
  pid_output_max_.min_value = -1000000.0f;
  pid_output_max_.max_value = 1000000.0f;
  pid_output_max_.has_range = true;
  pid_output_max_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  pid_cutoff_hz_.owner = this;
  pid_cutoff_hz_.value = &runtime_.rate_pid.derivative_cutoff_hz;
  pid_cutoff_hz_.min_value = 0.0f;
  pid_cutoff_hz_.max_value = 1000.0f;
  pid_cutoff_hz_.has_range = true;
  pid_cutoff_hz_.on_updated = &FlightCtrlCli::OnPidParameterUpdated;

  control_loop_hz_.owner = this;
  control_loop_hz_.value = &runtime_.control_loop_hz;
  control_loop_hz_.min_value = 50U;
  control_loop_hz_.max_value = 4000U;
  control_loop_hz_.has_range = true;
  control_loop_hz_.on_updated = nullptr;

  arm_locked_.owner = this;
  arm_locked_.value = &runtime_.arm_locked;
  arm_locked_.on_updated = nullptr;
}

void FlightCtrlCli::Init()
{
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.SetPassword(kCliPassword);
  shell_.SetActivationKey(' ', kActivationPrompt);
  shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this);
  UpdateShellBanner();
  RegisterParameters();
  RegisterReadonlyParameters();
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
  (void)shell_.RegisterParameter(
      {"pid.kp", "rate PID proportional gain", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_kp_});
  (void)shell_.RegisterParameter(
      {"pid.ki", "rate PID integral gain", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_ki_});
  (void)shell_.RegisterParameter(
      {"pid.kd", "rate PID derivative gain", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_kd_});
  (void)shell_.RegisterParameter(
      {"pid.kff", "rate PID feedforward gain", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_kff_});
  (void)shell_.RegisterParameter(
      {"pid.i_min", "rate PID integral lower limit", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_integral_min_});
  (void)shell_.RegisterParameter(
      {"pid.i_max", "rate PID integral upper limit", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_integral_max_});
  (void)shell_.RegisterParameter(
      {"pid.out_min", "rate PID output lower limit", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_output_min_});
  (void)shell_.RegisterParameter(
      {"pid.out_max", "rate PID output upper limit", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_output_max_});
  (void)shell_.RegisterParameter(
      {"pid.d_cutoff_hz", "rate PID derivative LPF cutoff", &FlightCtrlCli::GetFloatParameter,
       &FlightCtrlCli::SetFloatParameter, &pid_cutoff_hz_});
  (void)shell_.RegisterParameter(
      {"sys.loop_hz", "control loop frequency", &FlightCtrlCli::GetU32Parameter,
       &FlightCtrlCli::SetU32Parameter, &control_loop_hz_});
  (void)shell_.RegisterParameter(
      {"sys.arm_locked", "arming lock switch", &FlightCtrlCli::GetBoolParameter,
       &FlightCtrlCli::SetBoolParameter, &arm_locked_});
}

void FlightCtrlCli::RegisterReadonlyParameters()
{
  (void)shell_.RegisterParameter(
      {"sys.transport", "active CLI transport", &FlightCtrlCli::GetTransportParameter,
       nullptr, this});
  (void)shell_.RegisterParameter(
      {"sys.uptime_ms", "system uptime in milliseconds", &FlightCtrlCli::GetUptimeParameter,
       nullptr, this});
}

void FlightCtrlCli::RegisterFunctions()
{
  (void)shell_.RegisterFunction(
      {"status", "print flight-controller status", &FlightCtrlCli::StatusFunction, this});
  (void)shell_.RegisterFunction(
      {"sys.reboot", "trigger MCU software reset", &FlightCtrlCli::RebootFunction, this});
  (void)shell_.RegisterFunction(
      {"pid.reset", "reset PID runtime state", &FlightCtrlCli::PidResetFunction, this});
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
  rate_pid_.Configure(runtime_.rate_pid);
  runtime_.rate_pid = rate_pid_.GetConfig();
}

const FlightCtrlCli::TransportBinding *FlightCtrlCli::FindTransport(
    const char *name) const
{
  if (name == nullptr) {
    return nullptr;
  }

  uint8_t index = 0U;
  for (index = 0U; index < transport_count_; ++index) {
    if ((transports_[index].name != nullptr) &&
        (strcmp(transports_[index].name, name) == 0)) {
      return &transports_[index];
    }
  }

  return nullptr;
}

bool FlightCtrlCli::GetFloatParameter(void *context, char *buffer,
                                      uint32_t bufferSize)
{
  FloatParameterBinding *binding =
      reinterpret_cast<FloatParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  return FormatFloat(buffer, bufferSize, *binding->value);
}

bool FlightCtrlCli::SetFloatParameter(void *context, const char *value)
{
  FloatParameterBinding *binding =
      reinterpret_cast<FloatParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  float parsed = 0.0f;
  if (!ParseFloat(value, &parsed)) {
    return false;
  }

  if (binding->has_range &&
      ((parsed < binding->min_value) || (parsed > binding->max_value))) {
    return false;
  }

  *binding->value = parsed;
  if ((binding->on_updated != nullptr) && (binding->owner != nullptr)) {
    binding->on_updated(binding->owner);
  }
  return true;
}

bool FlightCtrlCli::GetU32Parameter(void *context, char *buffer,
                                    uint32_t bufferSize)
{
  U32ParameterBinding *binding = reinterpret_cast<U32ParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  return FormatU32(buffer, bufferSize, *binding->value);
}

bool FlightCtrlCli::SetU32Parameter(void *context, const char *value)
{
  U32ParameterBinding *binding = reinterpret_cast<U32ParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  uint32_t parsed = 0U;
  if (!ParseU32(value, &parsed)) {
    return false;
  }

  if (binding->has_range &&
      ((parsed < binding->min_value) || (parsed > binding->max_value))) {
    return false;
  }

  *binding->value = parsed;
  if ((binding->on_updated != nullptr) && (binding->owner != nullptr)) {
    binding->on_updated(binding->owner);
  }
  return true;
}

bool FlightCtrlCli::GetBoolParameter(void *context, char *buffer,
                                     uint32_t bufferSize)
{
  BoolParameterBinding *binding =
      reinterpret_cast<BoolParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  return FormatBool(buffer, bufferSize, *binding->value);
}

bool FlightCtrlCli::SetBoolParameter(void *context, const char *value)
{
  BoolParameterBinding *binding =
      reinterpret_cast<BoolParameterBinding *>(context);
  if ((binding == nullptr) || (binding->value == nullptr)) {
    return false;
  }

  bool parsed = false;
  if (!ParseBool(value, &parsed)) {
    return false;
  }

  *binding->value = parsed;
  if ((binding->on_updated != nullptr) && (binding->owner != nullptr)) {
    binding->on_updated(binding->owner);
  }
  return true;
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
  return FormatU32(buffer, bufferSize, HAL_GetTick());
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

  const Pid::State &pid_state = cli->rate_pid_.GetState();
  shell->WriteLine("Flight Controller Status");
  shell->Printf("  transport     : %s\r\n",
                (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                         : "unbound");
  shell->Printf("  shell_link    : %s\r\n",
                cli->shell_.IsConnected() ? "connected" : "disconnected");
  shell->Printf("  uptime_ms     : %lu\r\n",
                static_cast<unsigned long>(HAL_GetTick()));
  shell->Printf("  loop_hz       : %lu\r\n",
                static_cast<unsigned long>(cli->runtime_.control_loop_hz));
  shell->Printf("  arm_locked    : %s\r\n",
                cli->runtime_.arm_locked ? "true" : "false");
  shell->Printf("  pid_kp        : %.6g\r\n",
                static_cast<double>(cli->runtime_.rate_pid.kp));
  shell->Printf("  pid_ki        : %.6g\r\n",
                static_cast<double>(cli->runtime_.rate_pid.ki));
  shell->Printf("  pid_kd        : %.6g\r\n",
                static_cast<double>(cli->runtime_.rate_pid.kd));
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

bool FlightCtrlCli::TransportListFunction(Shell *shell, void *context, uint8_t argc,
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

  uint8_t index = 0U;
  for (index = 0U; index < cli->transport_count_; ++index) {
    const bool is_active =
        (cli->active_transport_name_ != nullptr) &&
        (strcmp(cli->active_transport_name_, cli->transports_[index].name) == 0);
    shell->Printf("  %s%s\r\n", cli->transports_[index].name,
                  is_active ? " [active]" : "");
  }
  return true;
}

bool FlightCtrlCli::TransportUseFunction(Shell *shell, void *context, uint8_t argc,
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

void FlightCtrlCli::IntroAnimation(Shell *shell, void *context)
{
  (void)context;

  if (shell == nullptr) {
    return;
  }

  shell->Write("\x1B[2J\x1B[H");
  shell->WriteLine("");
  shell->WriteLine("        ___  ________           ");
  shell->WriteLine("       / _ \\/ __/ / /_ _____    ");
  shell->WriteLine("      / , _/ _// / / // / -_)   ");
  shell->WriteLine("     /_/|_/___/_/_/\\_, /\\__/    ");
  shell->WriteLine("                   /___/        ");
  shell->WriteLine("");

  WriteDelay(shell, "Booting iFly secure terminal...", 14U);
  shell->WriteLine("");
  WriteSpinner(shell, "Checking transport link", 10U, 45U);
  WriteSpinner(shell, "Synchronizing command registry", 10U, 45U);
  WriteProgressBar(shell, "Preparing English CLI", 18U, 22U);
  shell->WriteLine("");
  WriteDelay(shell, "Welcome to the iFly Flight Controller.", 12U);
  shell->WriteLine("");
  WriteDelay(shell, "English terminal mode is now online.", 12U);
  shell->WriteLine("");
  shell->WriteLine("");
}

void FlightCtrlCli::OnPidParameterUpdated(FlightCtrlCli *owner)
{
  if (owner == nullptr) {
    return;
  }

  owner->ApplyPidConfiguration();
}

} // namespace iFly
