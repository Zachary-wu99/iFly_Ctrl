/**
 * @file project_parameters.hpp
 * @brief 工程参数树定义。
 */
#ifndef IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP
#define IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP

#include <stdint.h>

#include "pid.hpp"

namespace iFly {

/**
 * @brief 系统级参数分组。
 */
struct SystemParameters final {
  uint32_t control_loop_hz = 1000U; /**< 主控制循环频率，单位为 Hz。 */
  bool arm_locked = true; /**< 上锁状态标志，`true` 表示禁止解锁。 */
};

/**
 * @brief 任务调度相关参数分组。
 */
struct TaskParameters final {
  uint32_t main_loop_delay_ms = 1U; /**< 主循环阻塞延时，单位为毫秒。 */
  uint32_t cli_poll_period_ms = 50U; /**< CLI 轮询周期，单位为毫秒。 */
};

/**
 * @brief CLI 相关参数分组。
 */
struct CliParameters final {
  uint32_t rx_queue_size = 1024U; /**< CLI 接收队列容量，单位为字节。 */
  char default_transport[8] = "usb"; /**< 默认 CLI 传输通道名称。 */
  char password[8] = "ifly"; /**< CLI 登录密码。 */
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
 * @brief 调试与诊断参数分组。
 */
struct DebugParameters final {
  bool enable_cli = true; /**< 是否启用 CLI 服务。 */
  bool verbose_shell = false; /**< 是否输出详细 Shell 日志。 */
};

/**
 * @brief 全工程参数根结构。
 */
struct ProjectParameters final {
  SystemParameters system {}; /**< 系统级参数集合。 */
  TaskParameters task {}; /**< 任务调度参数集合。 */
  CliParameters cli {}; /**< CLI 相关参数集合。 */
  ControlParameters control {}; /**< 控制器参数集合。 */
  MotorParameters motor {}; /**< 电机输出参数集合。 */
  DebugParameters debug {}; /**< 调试参数集合。 */
};

/**
 * @brief 单个工程参数的静态绑定描述。
 */
struct ProjectParameterBinding final {
  const char *name = nullptr; /**< 参数名，例如 `control.speed_pid.kp`。 */
  const char *help = nullptr; /**< 参数说明文本。 */
  uint32_t offset = 0U; /**< 参数在 `ProjectParameters` 中的字节偏移。 */
  uint32_t size = 0U; /**< 参数占用的字节数。 */
  bool read_only = false; /**< 是否只读。 */
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

#endif /* IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP */
