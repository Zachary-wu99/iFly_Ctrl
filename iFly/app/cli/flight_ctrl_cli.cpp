// 飞控 CLI 模块实现。
// 包含控制台初始化、参数绑定、命令处理和开场动画状态机。
#include "flight_ctrl_cli.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

namespace iFly {

namespace {

constexpr char kCliPrompt[] = "iFly> ";
constexpr char kCliPassword[] = "ifly";
constexpr char kActivationPrompt[] =
    "Press SPACE to enter the iFly secure terminal.";
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ULL;
constexpr char kSpinnerFrames[4] = {'|', '/', '-', '\\'};

// 解析 CLI 输入中的浮点数字符串。
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

// 把毫秒数转换为纳秒数。
uint64_t MsToNs(uint32_t value_ms)
{
  return static_cast<uint64_t>(value_ms) * kNanosecondsPerMillisecond;
}

} // namespace

// 构造飞控 CLI 并准备默认运行时状态。
FlightCtrlCli::FlightCtrlCli()
    : rate_pid_(runtime_.rate_pid)
{
  ResetIntroAnimation();
}

// 初始化 Shell、参数表、命令表和 PID 运行时。
void FlightCtrlCli::Init()
{
  ResetIntroAnimation();
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.SetPassword(kCliPassword);
  shell_.SetActivationKey(' ', kActivationPrompt);
  shell_.SetSessionAnimation(&FlightCtrlCli::IntroAnimation, this);
  UpdateShellBanner();
  RegisterParameters();
  RegisterFunctions();
  ApplyPidConfiguration();
}

// 注册一个可切换的 CLI 传输通道。
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

// 切换当前正在使用的 CLI 传输通道。
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

// 轮询 Shell，推动输入输出和状态机运行。
void FlightCtrlCli::Poll()
{
  shell_.Poll();
}

// 向 Shell 注册可读写参数和只读状态项。
void FlightCtrlCli::RegisterParameters()
{
  parameter_manager_.Clear();

  // 允许通过 CLI 直接查看和调整速率环 PID 参数，便于在线调参。
  const ParameterManager::FloatSpec pid_parameters[] = {
      {"pid.kp", "rate PID proportional gain", &runtime_.rate_pid.kp, 0.0f,
       1000.0f, true, &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.ki", "rate PID integral gain", &runtime_.rate_pid.ki, 0.0f,
       1000.0f, true, &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.kd", "rate PID derivative gain", &runtime_.rate_pid.kd, 0.0f,
       1000.0f, true, &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.kff", "rate PID feedforward gain", &runtime_.rate_pid.kff, 0.0f,
       1000.0f, true, &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.i_min", "rate PID integral lower limit",
       &runtime_.rate_pid.integral_min, -1000000.0f, 1000000.0f, true,
       &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.i_max", "rate PID integral upper limit",
       &runtime_.rate_pid.integral_max, -1000000.0f, 1000000.0f, true,
       &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.out_min", "rate PID output lower limit",
       &runtime_.rate_pid.output_min, -1000000.0f, 1000000.0f, true,
       &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.out_max", "rate PID output upper limit",
       &runtime_.rate_pid.output_max, -1000000.0f, 1000000.0f, true,
       &FlightCtrlCli::OnPidParameterUpdated, this},
      {"pid.d_cutoff_hz", "rate PID derivative LPF cutoff",
       &runtime_.rate_pid.derivative_cutoff_hz, 0.0f, 1000.0f, true,
       &FlightCtrlCli::OnPidParameterUpdated, this},
  };

  uint32_t index = 0U;
  for (index = 0U; index < (sizeof(pid_parameters) / sizeof(pid_parameters[0]));
       ++index) {
    (void)parameter_manager_.AddFloat(pid_parameters[index]);
  }

  const ParameterManager::U32Spec system_u32_parameters[] = {
      {"sys.loop_hz", "control loop frequency", &runtime_.control_loop_hz, 50U,
       4000U, true, nullptr, nullptr},
  };
  for (index = 0U;
       index <
       (sizeof(system_u32_parameters) / sizeof(system_u32_parameters[0]));
       ++index) {
    (void)parameter_manager_.AddU32(system_u32_parameters[index]);
  }

  const ParameterManager::BoolSpec system_bool_parameters[] = {
      {"sys.arm_locked", "arming lock switch", &runtime_.arm_locked, nullptr,
       nullptr},
  };
  for (index = 0U;
       index <
       (sizeof(system_bool_parameters) / sizeof(system_bool_parameters[0]));
       ++index) {
    (void)parameter_manager_.AddBool(system_bool_parameters[index]);
  }

  // 只读项用于暴露运行时状态，不允许通过参数写接口修改。
  const ParameterManager::CallbackSpec readonly_parameters[] = {
      {"sys.transport", "active CLI transport",
       &FlightCtrlCli::GetTransportParameter, nullptr, this, nullptr, nullptr},
      {"sys.uptime_ms", "system uptime in milliseconds",
       &FlightCtrlCli::GetUptimeParameter, nullptr, nullptr, nullptr, nullptr},
  };
  for (index = 0U;
       index < (sizeof(readonly_parameters) / sizeof(readonly_parameters[0]));
       ++index) {
    (void)parameter_manager_.AddCallback(readonly_parameters[index]);
  }

  (void)parameter_manager_.RegisterToShell(&shell_);
}

// 向 Shell 注册飞控相关命令。
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

// 刷新 Shell 标题，反映当前传输通道。
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

// 把运行时 PID 参数同步到控制器实例。
void FlightCtrlCli::ApplyPidConfiguration()
{
  rate_pid_.Configure(runtime_.rate_pid);
  runtime_.rate_pid = rate_pid_.GetConfig();
}

// 重置开场动画状态机。
void FlightCtrlCli::ResetIntroAnimation()
{
  intro_animation_.phase = IntroAnimationPhase::kIdle;
  intro_animation_.delay.Reset();
  intro_animation_.element_index = 0U;
  intro_animation_.phase_started = false;
}

// 推进开场动画到下一阶段。
void FlightCtrlCli::AdvanceIntroAnimation(IntroAnimationPhase next_phase)
{
  intro_animation_.phase = next_phase;
  intro_animation_.delay.Reset();
  intro_animation_.element_index = 0U;
  intro_animation_.phase_started = false;
}

// 按打字机效果逐步输出一行文本。
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

// 按旋转指示器效果逐步输出一行文本。
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

// 按进度条效果逐步输出一行文本。
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

// 根据当前阶段推进开场动画并决定是否结束。
bool FlightCtrlCli::UpdateIntroAnimation(Shell *shell, bool start)
{
  if (shell == nullptr) {
    return true;
  }

  if (start) {
    ResetIntroAnimation();
    shell->Write("\x1B[2J\x1B[H");
    shell->WriteLine("");
    shell->WriteLine("        ___  ________           ");
    shell->WriteLine("       / _ \\/ __/ / /_ _____    ");
    shell->WriteLine("      / , _/ _// / / // / -_)   ");
    shell->WriteLine("     /_/|_/___/_/_/\\_, /\\__/    ");
    shell->WriteLine("                   /___/        ");
    shell->WriteLine("");
    AdvanceIntroAnimation(IntroAnimationPhase::kBootMessage);
  }

  const uint64_t now_ns = tick::NowNs();
  // 每个阶段都以“是否完成当前表现”作为推进条件，保证动画在主循环中非阻塞执行。
  switch (intro_animation_.phase) {
    case IntroAnimationPhase::kIdle:
      return false;

    case IntroAnimationPhase::kBootMessage:
      if (StepTypewriterLine(shell, now_ns, "Booting iFly secure terminal...",
                             14U)) {
        AdvanceIntroAnimation(IntroAnimationPhase::kTransportSpinner);
      }
      return false;

    case IntroAnimationPhase::kTransportSpinner:
      if (StepSpinnerLine(shell, now_ns, "Checking transport link", 10U, 45U)) {
        AdvanceIntroAnimation(IntroAnimationPhase::kRegistrySpinner);
      }
      return false;

    case IntroAnimationPhase::kRegistrySpinner:
      if (StepSpinnerLine(shell, now_ns, "Synchronizing command registry", 10U,
                          45U)) {
        AdvanceIntroAnimation(IntroAnimationPhase::kProgressBar);
      }
      return false;

    case IntroAnimationPhase::kProgressBar:
      if (StepProgressLine(shell, now_ns, "Preparing English CLI", 18U, 22U)) {
        shell->WriteLine("");
        AdvanceIntroAnimation(IntroAnimationPhase::kWelcomeMessage);
      }
      return false;

    case IntroAnimationPhase::kWelcomeMessage:
      if (StepTypewriterLine(shell, now_ns,
                             "Welcome to the iFly Flight Controller.", 12U)) {
        AdvanceIntroAnimation(IntroAnimationPhase::kOnlineMessage);
      }
      return false;

    case IntroAnimationPhase::kOnlineMessage:
      if (StepTypewriterLine(shell, now_ns,
                             "English terminal mode is now online.", 12U)) {
        shell->WriteLine("");
        AdvanceIntroAnimation(IntroAnimationPhase::kCompleted);
      }
      return false;

    case IntroAnimationPhase::kCompleted:
      ResetIntroAnimation();
      return true;

    default:
      ResetIntroAnimation();
      return true;
  }
}

// 按名称查找已注册的传输通道。
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

// 读取当前 CLI 传输通道名称。
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

// 读取系统运行时间参数。
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

// 处理 `status` 命令。
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
                static_cast<unsigned long>(tick::NowMs()));
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

// 处理 `sys.reboot` 命令。
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

// 处理 `pid.reset` 命令。
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

// 处理 `pid.sample` 命令。
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

// 处理 `transport.list` 命令。
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

// 处理 `transport.use` 命令。
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

// 作为 Shell 回调驱动开场动画。
bool FlightCtrlCli::IntroAnimation(Shell *shell, void *context, bool start)
{
  FlightCtrlCli *owner = reinterpret_cast<FlightCtrlCli *>(context);
  if (owner == nullptr) {
    return true;
  }

  return owner->UpdateIntroAnimation(shell, start);
}

// 在 PID 参数变化后重新应用配置。
void FlightCtrlCli::OnPidParameterUpdated(void *context)
{
  FlightCtrlCli *owner = reinterpret_cast<FlightCtrlCli *>(context);
  if (owner == nullptr) {
    return;
  }

  owner->ApplyPidConfiguration();
}

} // namespace iFly
