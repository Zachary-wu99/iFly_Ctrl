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

constexpr uint32_t OffsetOfControlPid(uint32_t pid_offset, uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(ProjectParameters, control) +
                               pid_offset +
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
    {"control.speed_pid", "speed PID configuration", OffsetOfControl(offsetof(ControlParameters, speed_pid)),
     sizeof(Pid::Config), false},
    {"control.speed_pid.kp", "speed PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false},
    {"control.speed_pid.ki", "speed PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false},
    {"control.speed_pid.kd", "speed PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false},
    {"control.speed_pid.kff", "speed PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false},
    {"control.speed_pid.integral_min", "speed PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false},
    {"control.speed_pid.integral_max", "speed PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false},
    {"control.speed_pid.output_min", "speed PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false},
    {"control.speed_pid.output_max", "speed PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false},
    {"control.speed_pid.derivative_cutoff_hz", "speed PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false},
    {"control.speed_pid.dt_min_s", "speed PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false},
    {"control.speed_pid.dt_max_s", "speed PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false},
    {"control.speed_pid.derivative_mode", "speed PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false},

    {"control.angle_pid", "angle PID configuration", OffsetOfControl(offsetof(ControlParameters, angle_pid)),
     sizeof(Pid::Config), false},
    {"control.angle_pid.kp", "angle PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false},
    {"control.angle_pid.ki", "angle PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false},
    {"control.angle_pid.kd", "angle PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false},
    {"control.angle_pid.kff", "angle PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false},
    {"control.angle_pid.integral_min", "angle PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false},
    {"control.angle_pid.integral_max", "angle PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false},
    {"control.angle_pid.output_min", "angle PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false},
    {"control.angle_pid.output_max", "angle PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false},
    {"control.angle_pid.derivative_cutoff_hz", "angle PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false},
    {"control.angle_pid.dt_min_s", "angle PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false},
    {"control.angle_pid.dt_max_s", "angle PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false},
    {"control.angle_pid.derivative_mode", "angle PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false},

    {"control.position_pid", "position PID configuration", OffsetOfControl(offsetof(ControlParameters, position_pid)),
     sizeof(Pid::Config), false},
    {"control.position_pid.kp", "position PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false},
    {"control.position_pid.ki", "position PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false},
    {"control.position_pid.kd", "position PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false},
    {"control.position_pid.kff", "position PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false},
    {"control.position_pid.integral_min", "position PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false},
    {"control.position_pid.integral_max", "position PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false},
    {"control.position_pid.output_min", "position PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false},
    {"control.position_pid.output_max", "position PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false},
    {"control.position_pid.derivative_cutoff_hz", "position PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false},
    {"control.position_pid.dt_min_s", "position PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false},
    {"control.position_pid.dt_max_s", "position PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false},
    {"control.position_pid.derivative_mode", "position PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_mode)),
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
