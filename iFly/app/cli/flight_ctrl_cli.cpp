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

const char *ConfiguredCliPassword()
{
  return kDefaultCliPassword;
}

} // namespace

FlightCtrlCli::FlightCtrlCli()
    : parameter_manager_(ParameterManager::Instance())
{
  ResetIntroAnimation();
}

void FlightCtrlCli::Init()
{
  parameter_manager_.ResetToDefaults();
  ResetIntroAnimation();
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.SetPassword(ConfiguredCliPassword());
  shell_.SetActivationKey(' ', kActivationPrompt);
  shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this);
  active_transport_name_ = "mavlink";
  UpdateShellBanner();
  RegisterParameters();
  RegisterFunctions();
}

void FlightCtrlCli::SetOutput(Shell::OutputHandler output, void *context)
{
  shell_.SetOutput(output, context);
}

void FlightCtrlCli::SetConnected(bool connected)
{
  shell_.SetConnected(connected);
}

void FlightCtrlCli::ProcessInput(const uint8_t *data, uint32_t length)
{
  shell_.ProcessInput(data, length);
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

  register_managed_parameter("SPD_PID_P", "speed PID proportional gain",
                             "SPD_PID_P", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_I", "speed PID integral gain",
                             "SPD_PID_I", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_D", "speed PID derivative gain",
                             "SPD_PID_D", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_FF", "speed PID feedforward gain",
                             "SPD_PID_FF", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_IMIN", "speed PID integral lower limit",
                             "SPD_PID_IMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_IMAX", "speed PID integral upper limit",
                             "SPD_PID_IMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_OMIN", "speed PID output lower limit",
                             "SPD_PID_OMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_OMAX", "speed PID output upper limit",
                             "SPD_PID_OMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("SPD_PID_FLTD", "speed PID derivative LPF cutoff",
                             "SPD_PID_FLTD",
                             ManagedParameterType::kFloat, 0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_P", "angle PID proportional gain",
                             "ANG_PID_P", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_I", "angle PID integral gain",
                             "ANG_PID_I", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_D", "angle PID derivative gain",
                             "ANG_PID_D", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_FF", "angle PID feedforward gain",
                             "ANG_PID_FF", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_IMIN", "angle PID integral lower limit",
                             "ANG_PID_IMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_IMAX", "angle PID integral upper limit",
                             "ANG_PID_IMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_OMIN", "angle PID output lower limit",
                             "ANG_PID_OMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_OMAX", "angle PID output upper limit",
                             "ANG_PID_OMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("ANG_PID_FLTD", "angle PID derivative LPF cutoff",
                             "ANG_PID_FLTD",
                             ManagedParameterType::kFloat, 0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_P", "position PID proportional gain",
                             "POS_PID_P", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_I", "position PID integral gain",
                             "POS_PID_I", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_D", "position PID derivative gain",
                             "POS_PID_D", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_FF", "position PID feedforward gain",
                             "POS_PID_FF", ManagedParameterType::kFloat,
                             0.0f, 1000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_IMIN", "position PID integral lower limit",
                             "POS_PID_IMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_IMAX", "position PID integral upper limit",
                             "POS_PID_IMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_OMIN", "position PID output lower limit",
                             "POS_PID_OMIN",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_OMAX", "position PID output upper limit",
                             "POS_PID_OMAX",
                             ManagedParameterType::kFloat, -1000000.0f,
                             1000000.0f, 0U, 0U);
  register_managed_parameter("POS_PID_FLTD", "position PID derivative LPF cutoff",
                             "POS_PID_FLTD",
                             ManagedParameterType::kFloat, 0.0f, 1000.0f, 0U, 0U);
  (void)shell_.RegisterParameter(
      {"sys.transport", "active CLI transport",
       &FlightCtrlCli::GetTransportParameter, nullptr, this});
  (void)shell_.RegisterParameter(
      {"sys.uptime_ms", "system uptime in milliseconds",
       &FlightCtrlCli::GetUptimeParameter, nullptr, this});

}

void FlightCtrlCli::RegisterFunctions()
{
  (void)shell_.RegisterFunction(
      {"status", "print flight-controller status", &FlightCtrlCli::StatusFunction,
       this});
  (void)shell_.RegisterFunction(
      {"sys.reboot", "trigger MCU software reset",
       &FlightCtrlCli::RebootFunction, this});
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

  shell->WriteLine("Flight Controller Status");
  shell->Printf("  transport     : %s\r\n",
                (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                         : "unbound");
  shell->Printf("  shell_link    : %s\r\n",
                cli->shell_.IsConnected() ? "connected" : "disconnected");
  shell->Printf("  uptime_ms     : %lu\r\n",
                static_cast<unsigned long>(tick::NowMs()));
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

bool FlightCtrlCli::IntroAnimation(Shell *shell, void *context, bool start)
{
  FlightCtrlCli *owner = reinterpret_cast<FlightCtrlCli *>(context);
  if (owner == nullptr) {
    return true;
  }

  return owner->UpdateIntroAnimation(shell, start);
}

} // namespace iFly

