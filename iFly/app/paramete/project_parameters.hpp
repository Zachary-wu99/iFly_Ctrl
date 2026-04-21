#ifndef IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP
#define IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP

#include <stdint.h>

#include "pid.hpp"

namespace iFly {

/**
 * @brief 系统级参数分组。
 *
 * @details
 * 这里存放和系统主运行逻辑强相关的基础参数，
 * 例如主控制频率、上锁状态等。
 */
struct SystemParameters final {
  uint32_t control_loop_hz = 1000U;
  bool arm_locked = true;
};

/**
 * @brief 任务调度相关参数分组。
 *
 * @details
 * 这类参数通常决定主循环、CLI 轮询等基础任务的节拍，
 * 适合统一收口，避免各处散落硬编码常量。
 */
struct TaskParameters final {
  uint32_t main_loop_delay_ms = 1U;
  uint32_t cli_poll_period_ms = 50U;
};

/**
 * @brief CLI 相关参数分组。
 *
 * @details
 * 这里既包含数值参数，也包含定长字符数组，
 * 用来示范参数中心对“基础类型 + 结构体/数组类型”的统一承载能力。
 */
struct CliParameters final {
  uint32_t rx_queue_size = 1024U;
  char default_transport[8] = "usb";
  char password[8] = "ifly";
};

/**
 * @brief 控制器参数分组。
 *
 * @details
 * 直接把 `Pid::Config` 这类结构体放进工程参数树中，
 * 模块使用时可以整组读取，也可以读取其中单个字段。
 */
struct ControlParameters final {
  Pid::Config rate_pid {
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
      Pid::DerivativeMode::kOnMeasurement};
};

/**
 * @brief 电机输出参数分组。
 */
struct MotorParameters final {
  uint16_t min_pwm = 1000U;
  uint16_t idle_pwm = 1050U;
  uint16_t max_pwm = 2000U;
};

/**
 * @brief 调试与诊断相关参数分组。
 */
struct DebugParameters final {
  bool enable_cli = true;
  bool verbose_shell = false;
};

/**
 * @brief 全工程参数根结构。
 *
 * @details
 * 后续如果新增模块参数，优先在这里继续分组扩展，
 * 保持“所有工程参数都有统一归属”。
 */
struct ProjectParameters final {
  SystemParameters system {};
  TaskParameters task {};
  CliParameters cli {};
  ControlParameters control {};
  MotorParameters motor {};
  DebugParameters debug {};
};

/**
 * @brief 单个工程参数绑定描述。
 *
 * @details
 * 这里描述的是“参数名字 -> 参数树内某块内存”的静态映射关系。
 * `ProjectParameterManager` 会根据这些绑定描述完成统一注册。
 */
struct ProjectParameterBinding final {
  const char *name = nullptr;
  const char *help = nullptr;
  uint32_t offset = 0U;
  uint32_t size = 0U;
  bool read_only = false;
};

/**
 * @brief 构造一份默认工程参数。
 *
 * @details
 * 用函数而不是全局可写对象返回默认值，便于后续做：
 * - 恢复出厂参数
 * - 从 Flash 读取失败后的兜底回退
 * - 单元测试中的重复初始化
 */
ProjectParameters MakeDefaultProjectParameters();

/**
 * @brief 返回全工程参数绑定表。
 *
 * @param count 返回绑定表项数量，可为 `nullptr`。
 * @return 指向只读静态表的首地址。
 */
const ProjectParameterBinding *GetProjectParameterBindings(uint16_t *count);

} // namespace iFly

#endif /* IFLY_APP_PARAMETE_PROJECT_PARAMETERS_HPP */
