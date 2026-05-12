/**
 * @file project_parameters.hpp
 * @brief 工程参数树定义。
 */
#ifndef IFLY_APP_FLY_SYS_PROJECT_PARAMETERS_HPP
#define IFLY_APP_FLY_SYS_PROJECT_PARAMETERS_HPP

#include <stdint.h>

#include "pid.hpp"

namespace iFly {

/**
 * @brief MAVLink 基础参数分组。
 */
struct MavlinkParameters final {
  uint8_t system_id = 25U; /**< MAVLink 系统 ID。 */
  uint8_t component_id = 1U; /**< MAVLink 组件 ID。 */
  uint8_t vehicle_type = 2U; /**< 飞行器类型。 */
  uint8_t autopilot_type = 0U; /**< 飞控类型。 */
};

/**
 * @brief 控制器参数分组。
 */
struct ControlParameters final {
  Pid::Config speed_pid {
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
      Pid::DerivativeMode::kOnMeasurement}; /**< 速度环 PID 配置。 */
  Pid::Config angle_pid {
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
      Pid::DerivativeMode::kOnMeasurement}; /**< 角度环 PID 配置。 */
  Pid::Config position_pid {
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
      Pid::DerivativeMode::kOnMeasurement}; /**< 位置环 PID 配置。 */
};

/**
 * @brief 电机输出参数分组。
 */
struct MotorParameters final {
  uint16_t min_pwm = 1000U; /**< 电机最小输出 PWM。 */
  uint16_t idle_pwm = 1050U; /**< 电机怠速输出 PWM。 */
  uint16_t max_pwm = 2000U; /**< 电机最大输出 PWM。 */
};

/**
 * @brief 电池参数分组。
 */
struct BatteryParameters final {
  int32_t cell_count = 0; /**< 电池串数，`0` 表示自动识别。 */
  float empty_voltage = 3.5f; /**< 单节空电电压，单位为 V。 */
  float charged_voltage = 4.2f; /**< 单节满电电压，单位为 V。 */
  float capacity_mah = -1.0f; /**< 电池容量，单位为 mAh。 */
};

/**
 * @brief RC 通道映射参数分组。
 */
struct RcMapParameters final {
  int32_t roll = 1; /**< 横滚通道编号。 */
  int32_t pitch = 2; /**< 俯仰通道编号。 */
  int32_t throttle = 3; /**< 油门通道编号。 */
  int32_t yaw = 4; /**< 偏航通道编号。 */
};

/**
 * @brief 全工程参数根结构。
 */
struct ProjectParameters final {
  MavlinkParameters mavlink {}; /**< MAVLink 基础参数集合。 */
  ControlParameters control {}; /**< 控制器参数集合。 */
  MotorParameters motor {}; /**< 电机输出参数集合。 */
  BatteryParameters battery {}; /**< 电池参数集合。 */
  RcMapParameters rc_map {}; /**< RC 通道映射参数集合。 */
};

/**
 * @brief 工程参数在 MAVLink 参数协议中的数据类型。
 */
enum class ProjectParameterType : uint8_t {
  kBytes = 0U, /**< 原始字节块。 */
  kBool, /**< 布尔型。 */
  kUint8, /**< 无符号 8 位整型。 */
  kUint16, /**< 无符号 16 位整型。 */
  kUint32, /**< 无符号 32 位整型。 */
  kInt32, /**< 有符号 32 位整型。 */
  kFloat /**< 32 位浮点型。 */
};

/**
 * @brief 单个工程参数的静态绑定描述。
 */
struct ProjectParameterBinding final {
  const char *name = nullptr; /**< 参数名，例如 `SPD_PID_P`。 */
  const char *help = nullptr; /**< 参数说明文本。 */
  uint32_t offset = 0U; /**< 参数在 `ProjectParameters` 中的字节偏移。 */
  uint32_t size = 0U; /**< 参数占用的字节数。 */
  bool read_only = false; /**< 是否只读。 */
  ProjectParameterType type = ProjectParameterType::kBytes; /**< MAVLink 参数类型。 */
  bool mavlink_visible = false; /**< 是否通过 MAVLink 参数协议暴露。 */
};

/**
 * @brief 生成一份默认工程参数。
 *
 * @return 默认参数树对象。
 */
ProjectParameters MakeDefaultProjectParameters();

/**
 * @brief 获取工程参数树中的参数分组。
 *
 * @param parameters 工程参数树。
 * @param group 参数分组成员指针。
 * @return 参数分组只读引用。
 */
template <typename Group>
const Group &GetProjectParameter(const ProjectParameters &parameters,
                                 Group ProjectParameters::*group) {
  return parameters.*group;
}

/**
 * @brief 获取工程参数树中的指定参数。
 *
 * @param parameters 工程参数树。
 * @param group 参数分组成员指针。
 * @param member 分组内参数成员指针。
 * @return 指定参数只读引用。
 */
template <typename Group, typename Member>
const Member &GetProjectParameter(const ProjectParameters &parameters,
                                  Group ProjectParameters::*group,
                                  Member Group::*member) {
  return (parameters.*group).*member;
}

/**
 * @brief 获取工程参数绑定表。
 *
 * @param count 输出绑定项数量，可为 `nullptr`。
 * @return 指向只读静态绑定表的首地址。
 */
const ProjectParameterBinding *GetProjectParameterBindings(uint16_t *count);

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_PROJECT_PARAMETERS_HPP */
