#include "sys_parameters.hpp"

#include <stddef.h>

namespace iFly {

namespace {

constexpr ParameterType kBool = ParameterType::kBool;
constexpr ParameterType kUint8 = ParameterType::kUint8;
constexpr ParameterType kUint16 = ParameterType::kUint16;
constexpr ParameterType kUint32 = ParameterType::kUint32;
constexpr ParameterType kInt32 = ParameterType::kInt32;
constexpr ParameterType kFloat = ParameterType::kFloat;

constexpr uint32_t OffsetOfIdentity(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, identity) + member_offset);
}

constexpr uint32_t OffsetOfControl(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, control) + member_offset);
}

constexpr uint32_t OffsetOfMotor(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, motor) + member_offset);
}

constexpr uint32_t OffsetOfBattery(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, battery) + member_offset);
}

constexpr uint32_t OffsetOfRcMap(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, rc_map) + member_offset);
}

constexpr uint32_t OffsetOfControlPid(uint32_t pid_offset, uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, control) +
                               pid_offset +
                               member_offset);
}

const ParameterBinding kBindings[] = {
    {SysParameterNames::kIdentitySystemId, "system id",
     OffsetOfIdentity(offsetof(SystemIdentityParameters, system_id)),
     sizeof(SystemIdentityParameters {}.system_id), true, kUint8},
    {SysParameterNames::kIdentityComponentId, "component id",
     OffsetOfIdentity(offsetof(SystemIdentityParameters, component_id)),
     sizeof(SystemIdentityParameters {}.component_id), true, kUint8},
    {SysParameterNames::kIdentityVehicleType, "vehicle type",
     OffsetOfIdentity(offsetof(SystemIdentityParameters, vehicle_type)),
     sizeof(SystemIdentityParameters {}.vehicle_type), true, kUint8},
    {SysParameterNames::kIdentityAutopilotType, "autopilot type",
     OffsetOfIdentity(offsetof(SystemIdentityParameters, autopilot_type)),
     sizeof(SystemIdentityParameters {}.autopilot_type), true, kUint8},
    {SysParameterNames::kBatteryCellCount, "battery cell count", OffsetOfBattery(offsetof(BatteryParameters, cell_count)),
     sizeof(BatteryParameters {}.cell_count), false, kInt32},
    {SysParameterNames::kBatteryEmptyVoltage, "battery empty voltage", OffsetOfBattery(offsetof(BatteryParameters, empty_voltage)),
     sizeof(BatteryParameters {}.empty_voltage), false, kFloat},
    {SysParameterNames::kBatteryChargedVoltage, "battery charged voltage", OffsetOfBattery(offsetof(BatteryParameters, charged_voltage)),
     sizeof(BatteryParameters {}.charged_voltage), false, kFloat},
    {SysParameterNames::kBatteryCapacityMah, "battery capacity in mAh", OffsetOfBattery(offsetof(BatteryParameters, capacity_mah)),
     sizeof(BatteryParameters {}.capacity_mah), false, kFloat},

    {SysParameterNames::kRcRollChannel, "rc roll channel", OffsetOfRcMap(offsetof(RcMapParameters, roll)),
     sizeof(RcMapParameters {}.roll), false, kInt32},
    {SysParameterNames::kRcPitchChannel, "rc pitch channel", OffsetOfRcMap(offsetof(RcMapParameters, pitch)),
     sizeof(RcMapParameters {}.pitch), false, kInt32},
    {SysParameterNames::kRcThrottleChannel, "rc throttle channel", OffsetOfRcMap(offsetof(RcMapParameters, throttle)),
     sizeof(RcMapParameters {}.throttle), false, kInt32},
    {SysParameterNames::kRcYawChannel, "rc yaw channel", OffsetOfRcMap(offsetof(RcMapParameters, yaw)),
     sizeof(RcMapParameters {}.yaw), false, kInt32},

    {SysParameterNames::kMotorPwmMin, "minimum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, min_pwm)),
     sizeof(MotorParameters {}.min_pwm), false, kUint16},
    {SysParameterNames::kMotorPwmIdle, "idle motor PWM output", OffsetOfMotor(offsetof(MotorParameters, idle_pwm)),
     sizeof(MotorParameters {}.idle_pwm), false, kUint16},
    {SysParameterNames::kMotorPwmMax, "maximum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, max_pwm)),
     sizeof(MotorParameters {}.max_pwm), false, kUint16},

    {SysParameterNames::kControlSpeedKp, "speed PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {SysParameterNames::kControlSpeedKi, "speed PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {SysParameterNames::kControlSpeedKd, "speed PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {SysParameterNames::kControlSpeedKff, "speed PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {SysParameterNames::kControlSpeedIntegralMin, "speed PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {SysParameterNames::kControlSpeedIntegralMax, "speed PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {SysParameterNames::kControlSpeedOutputMin, "speed PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {SysParameterNames::kControlSpeedOutputMax, "speed PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {SysParameterNames::kControlSpeedDerivativeCutoffHz, "speed PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {SysParameterNames::kControlSpeedDtMinS, "speed PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {SysParameterNames::kControlSpeedDtMaxS, "speed PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {SysParameterNames::kControlSpeedDerivativeMode, "speed PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false, kUint8},

    {SysParameterNames::kControlAngleKp, "angle PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {SysParameterNames::kControlAngleKi, "angle PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {SysParameterNames::kControlAngleKd, "angle PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {SysParameterNames::kControlAngleKff, "angle PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {SysParameterNames::kControlAngleIntegralMin, "angle PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {SysParameterNames::kControlAngleIntegralMax, "angle PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {SysParameterNames::kControlAngleOutputMin, "angle PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {SysParameterNames::kControlAngleOutputMax, "angle PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {SysParameterNames::kControlAngleDerivativeCutoffHz, "angle PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {SysParameterNames::kControlAngleDtMinS, "angle PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {SysParameterNames::kControlAngleDtMaxS, "angle PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {SysParameterNames::kControlAngleDerivativeMode, "angle PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false, kUint8},

    {SysParameterNames::kControlPositionKp, "position PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {SysParameterNames::kControlPositionKi, "position PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {SysParameterNames::kControlPositionKd, "position PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {SysParameterNames::kControlPositionKff, "position PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {SysParameterNames::kControlPositionIntegralMin, "position PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {SysParameterNames::kControlPositionIntegralMax, "position PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {SysParameterNames::kControlPositionOutputMin, "position PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {SysParameterNames::kControlPositionOutputMax, "position PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {SysParameterNames::kControlPositionDerivativeCutoffHz, "position PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {SysParameterNames::kControlPositionDtMinS, "position PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {SysParameterNames::kControlPositionDtMaxS, "position PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {SysParameterNames::kControlPositionDerivativeMode, "position PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false, kUint8},
};

} // namespace

SysParameters MakeDefaultSysParameters() {
  return SysParameters {};
}

const ParameterBinding *GetSysParameterBindings(uint16_t *count) {
  if (count != nullptr) {
    *count = static_cast<uint16_t>(sizeof(kBindings) / sizeof(kBindings[0]));
  }

  return kBindings;
}

} // namespace iFly


