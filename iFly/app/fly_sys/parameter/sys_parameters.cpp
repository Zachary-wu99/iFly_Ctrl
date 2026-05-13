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

constexpr uint32_t OffsetOfMavlink(uint32_t member_offset) {
  return static_cast<uint32_t>(offsetof(SysParameters, mavlink) + member_offset);
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
    {"MAV_SYS_ID", "mavlink system id", OffsetOfMavlink(offsetof(MavlinkParameters, system_id)),
     sizeof(MavlinkParameters {}.system_id), true, kUint8},
    {"MAV_COMP_ID", "mavlink component id", OffsetOfMavlink(offsetof(MavlinkParameters, component_id)),
     sizeof(MavlinkParameters {}.component_id), true, kUint8},
    {"MAV_TYPE", "mavlink vehicle type", OffsetOfMavlink(offsetof(MavlinkParameters, vehicle_type)),
     sizeof(MavlinkParameters {}.vehicle_type), true, kUint8},
    {"MAV_AUTOPILOT", "mavlink autopilot type", OffsetOfMavlink(offsetof(MavlinkParameters, autopilot_type)),
     sizeof(MavlinkParameters {}.autopilot_type), true, kUint8},
    {"BAT1_N_CELLS", "battery cell count", OffsetOfBattery(offsetof(BatteryParameters, cell_count)),
     sizeof(BatteryParameters {}.cell_count), false, kInt32},
    {"BAT1_V_EMPTY", "battery empty voltage", OffsetOfBattery(offsetof(BatteryParameters, empty_voltage)),
     sizeof(BatteryParameters {}.empty_voltage), false, kFloat},
    {"BAT1_V_CHARGED", "battery charged voltage", OffsetOfBattery(offsetof(BatteryParameters, charged_voltage)),
     sizeof(BatteryParameters {}.charged_voltage), false, kFloat},
    {"BAT1_CAPACITY", "battery capacity in mAh", OffsetOfBattery(offsetof(BatteryParameters, capacity_mah)),
     sizeof(BatteryParameters {}.capacity_mah), false, kFloat},

    {"RC_MAP_ROLL", "rc roll channel", OffsetOfRcMap(offsetof(RcMapParameters, roll)),
     sizeof(RcMapParameters {}.roll), false, kInt32},
    {"RC_MAP_PITCH", "rc pitch channel", OffsetOfRcMap(offsetof(RcMapParameters, pitch)),
     sizeof(RcMapParameters {}.pitch), false, kInt32},
    {"RC_MAP_THROTTLE", "rc throttle channel", OffsetOfRcMap(offsetof(RcMapParameters, throttle)),
     sizeof(RcMapParameters {}.throttle), false, kInt32},
    {"RC_MAP_YAW", "rc yaw channel", OffsetOfRcMap(offsetof(RcMapParameters, yaw)),
     sizeof(RcMapParameters {}.yaw), false, kInt32},

    {"MOT_PWM_MIN", "minimum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, min_pwm)),
     sizeof(MotorParameters {}.min_pwm), false, kUint16},
    {"MOT_PWM_IDLE", "idle motor PWM output", OffsetOfMotor(offsetof(MotorParameters, idle_pwm)),
     sizeof(MotorParameters {}.idle_pwm), false, kUint16},
    {"MOT_PWM_MAX", "maximum motor PWM output", OffsetOfMotor(offsetof(MotorParameters, max_pwm)),
     sizeof(MotorParameters {}.max_pwm), false, kUint16},

    {"SPD_PID_P", "speed PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {"SPD_PID_I", "speed PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {"SPD_PID_D", "speed PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {"SPD_PID_FF", "speed PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {"SPD_PID_IMIN", "speed PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {"SPD_PID_IMAX", "speed PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {"SPD_PID_OMIN", "speed PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {"SPD_PID_OMAX", "speed PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {"SPD_PID_FLTD", "speed PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {"SPD_PID_DTMIN", "speed PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {"SPD_PID_DTMAX", "speed PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {"SPD_PID_DMODE", "speed PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, speed_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false, kUint8},

    {"ANG_PID_P", "angle PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {"ANG_PID_I", "angle PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {"ANG_PID_D", "angle PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {"ANG_PID_FF", "angle PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {"ANG_PID_IMIN", "angle PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {"ANG_PID_IMAX", "angle PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {"ANG_PID_OMIN", "angle PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {"ANG_PID_OMAX", "angle PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {"ANG_PID_FLTD", "angle PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {"ANG_PID_DTMIN", "angle PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {"ANG_PID_DTMAX", "angle PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {"ANG_PID_DMODE", "angle PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, angle_pid), offsetof(Pid::Config, derivative_mode)),
     sizeof(Pid::Config {}.derivative_mode), false, kUint8},

    {"POS_PID_P", "position PID proportional gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kp)),
     sizeof(Pid::Config {}.kp), false, kFloat},
    {"POS_PID_I", "position PID integral gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, ki)),
     sizeof(Pid::Config {}.ki), false, kFloat},
    {"POS_PID_D", "position PID derivative gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kd)),
     sizeof(Pid::Config {}.kd), false, kFloat},
    {"POS_PID_FF", "position PID feedforward gain", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, kff)),
     sizeof(Pid::Config {}.kff), false, kFloat},
    {"POS_PID_IMIN", "position PID integral lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_min)),
     sizeof(Pid::Config {}.integral_min), false, kFloat},
    {"POS_PID_IMAX", "position PID integral upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, integral_max)),
     sizeof(Pid::Config {}.integral_max), false, kFloat},
    {"POS_PID_OMIN", "position PID output lower limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_min)),
     sizeof(Pid::Config {}.output_min), false, kFloat},
    {"POS_PID_OMAX", "position PID output upper limit", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, output_max)),
     sizeof(Pid::Config {}.output_max), false, kFloat},
    {"POS_PID_FLTD", "position PID derivative filter cutoff", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_cutoff_hz)),
     sizeof(Pid::Config {}.derivative_cutoff_hz), false, kFloat},
    {"POS_PID_DTMIN", "position PID minimum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_min_s)),
     sizeof(Pid::Config {}.dt_min_s), false, kFloat},
    {"POS_PID_DTMAX", "position PID maximum accepted control period", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, dt_max_s)),
     sizeof(Pid::Config {}.dt_max_s), false, kFloat},
    {"POS_PID_DMODE", "position PID derivative mode enum value", OffsetOfControlPid(offsetof(ControlParameters, position_pid), offsetof(Pid::Config, derivative_mode)),
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


