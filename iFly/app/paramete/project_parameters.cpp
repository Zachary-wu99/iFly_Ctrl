#include "project_parameters.hpp"

#include <stddef.h>

namespace iFly {

namespace {

constexpr uint32_t OffsetOfSystem(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, system) + member_offset);
}

constexpr uint32_t OffsetOfTask(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, task) + member_offset);
}

constexpr uint32_t OffsetOfCli(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, cli) + member_offset);
}

constexpr uint32_t OffsetOfControl(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, control) + member_offset);
}

constexpr uint32_t OffsetOfMotor(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, motor) + member_offset);
}

constexpr uint32_t OffsetOfDebug(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, debug) + member_offset);
}

constexpr uint32_t OffsetOfRatePid(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, control) +
                               offsetof(ControlParameters, rate_pid) +
                               member_offset);
}

const ProjectParameterBinding kBindings[] = {
    {"project", "whole project parameter tree", 0U, sizeof(ProjectParameters), false},

    {"system", "system parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, system)),
     sizeof(SystemParameters), false},
    {"system.control_loop_hz", "main control loop frequency", OffsetOfSystem(offsetof(SystemParameters, control_loop_hz)),
     sizeof(SystemParameters {}.control_loop_hz), false},
    {"system.arm_locked", "arming lock state", OffsetOfSystem(offsetof(SystemParameters, arm_locked)),
     sizeof(SystemParameters {}.arm_locked), false},

    {"task", "task scheduler parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, task)),
     sizeof(TaskParameters), false},
    {"task.main_loop_delay_ms", "main loop delay in milliseconds", OffsetOfTask(offsetof(TaskParameters, main_loop_delay_ms)),
     sizeof(TaskParameters {}.main_loop_delay_ms), false},
    {"task.cli_poll_period_ms", "CLI polling period in milliseconds", OffsetOfTask(offsetof(TaskParameters, cli_poll_period_ms)),
     sizeof(TaskParameters {}.cli_poll_period_ms), false},

    {"cli", "CLI parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, cli)),
     sizeof(CliParameters), false},
    {"cli.rx_queue_size", "CLI RX queue size in bytes", OffsetOfCli(offsetof(CliParameters, rx_queue_size)),
     sizeof(CliParameters {}.rx_queue_size), false},
    {"cli.default_transport", "default CLI transport name", OffsetOfCli(offsetof(CliParameters, default_transport)),
     sizeof(CliParameters {}.default_transport), false},
    {"cli.password", "CLI login password", OffsetOfCli(offsetof(CliParameters, password)),
     sizeof(CliParameters {}.password), false},

    {"control", "controller parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, control)),
     sizeof(ControlParameters), false},
    {"control.rate_pid", "rate PID configuration", OffsetOfControl(offsetof(ControlParameters, rate_pid)),
     sizeof(Pid::Config), false},
    {"control.rate_pid.kp", "rate PID proportional gain", OffsetOfRatePid(offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false},
    {"control.rate_pid.ki", "rate PID integral gain", OffsetOfRatePid(offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false},
    {"control.rate_pid.kd", "rate PID derivative gain", OffsetOfRatePid(offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false},
    {"control.rate_pid.kff", "rate PID feedforward gain", OffsetOfRatePid(offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false},
    {"control.rate_pid.integral_min", "rate PID integral lower limit", OffsetOfRatePid(offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false},
    {"control.rate_pid.integral_max", "rate PID integral upper limit", OffsetOfRatePid(offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false},
    {"control.rate_pid.output_min", "rate PID output lower limit", OffsetOfRatePid(offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false},
    {"control.rate_pid.output_max", "rate PID output upper limit", OffsetOfRatePid(offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false},
    {"control.rate_pid.derivative_cutoff_hz", "rate PID derivative filter cutoff", OffsetOfRatePid(offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false},
    {"control.rate_pid.dt_min_s", "minimum accepted control period", OffsetOfRatePid(offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false},
    {"control.rate_pid.dt_max_s", "maximum accepted control period", OffsetOfRatePid(offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false},
    {"control.rate_pid.derivative_mode", "PID derivative mode enum value", OffsetOfRatePid(offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false},

    {"motor", "motor output parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, motor)),
     sizeof(MotorParameters), false},
    {"motor.min_pwm", "minimum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, min_pwm)),
     sizeof(MotorParameters {}.min_pwm), false},
    {"motor.idle_pwm", "idle motor PWM output", OffsetOfMotor(offsetof(MotorParameters, idle_pwm)),
     sizeof(MotorParameters {}.idle_pwm), false},
    {"motor.max_pwm", "maximum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, max_pwm)),
     sizeof(MotorParameters {}.max_pwm), false},

    {"debug", "debug parameter group", static_cast<uint32_t>(offsetof(ProjectParameters, debug)),
     sizeof(DebugParameters), false},
    {"debug.enable_cli", "whether CLI service is enabled", OffsetOfDebug(offsetof(DebugParameters, enable_cli)),
     sizeof(DebugParameters {}.enable_cli), false},
    {"debug.verbose_shell", "whether shell prints verbose logs", OffsetOfDebug(offsetof(DebugParameters, verbose_shell)),
     sizeof(DebugParameters {}.verbose_shell), false},
};

} // namespace

ProjectParameters MakeDefaultProjectParameters() {
  return ProjectParameters {};
}

const ProjectParameterBinding *GetProjectParameterBindings(uint16_t *count) {
  if (count != nullptr) {
    *count = static_cast<uint16_t>(sizeof(kBindings) / sizeof(kBindings[0]));
  }

  return kBindings;
}

} // namespace iFly
