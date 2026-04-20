// 飞控 CLI 模块接口。
// 负责串口控制台、参数注册、命令注册和启动动画管理。
#ifndef IFLY_FLIGHT_CTRL_CLI_HPP
#define IFLY_FLIGHT_CTRL_CLI_HPP

#include <stdint.h>

#include "parameter_manager.hpp"
#include "pid.hpp"
#include "shell.hpp"
#include "tick.hpp"

namespace iFly {

/**
 * @brief 飞控命令行控制台。
 *
 * @details
 * 把 `Shell`、参数管理器和 PID 示例调试入口组合在一起，
 * 对上提供统一的调试终端，对下支持按名字切换不同串口传输介质。
 */
class FlightCtrlCli final {
public:
  /** @brief 最多允许注册的 CLI 传输通道数量。 */
  static constexpr uint8_t kMaxTransportCount = 4U;

  /** @brief 构造 CLI，并加载默认运行时配置。 */
  FlightCtrlCli();

  /** @brief 初始化 Shell、参数注册、函数注册与开场动画。 */
  void Init();

  /** @brief 注册一个可切换的 CLI 传输通道。 */
  bool RegisterTransport(const char *name, SerialIoBase *io);
  /** @brief 按名字切换当前激活的 CLI 传输通道。 */
  bool UseTransport(const char *name);

  /** @brief 在主循环中轮询 Shell。 */
  void Poll();

  /** @brief 返回可直接访问的 Shell 控制台对象。 */
  Shell &Console() {
    return shell_;
  }

  /** @brief 返回只读 Shell 控制台对象。 */
  const Shell &Console() const {
    return shell_;
  }

private:
  /** @brief CLI 运行时可调参数快照。 */
  struct RuntimeConfig final {
    uint32_t control_loop_hz = 1000U;
    bool arm_locked = true;
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

  /** @brief 传输通道注册项。 */
  struct TransportBinding final {
    const char *name = nullptr;
    SerialIoBase *io = nullptr;
  };

  /** @brief 开场动画的阶段定义。 */
  enum class IntroAnimationPhase : uint8_t {
    kIdle = 0U,
    kBootMessage,
    kTransportSpinner,
    kRegistrySpinner,
    kProgressBar,
    kWelcomeMessage,
    kOnlineMessage,
    kCompleted,
  };

  /** @brief 开场动画状态机的当前状态。 */
  struct IntroAnimationState final {
    IntroAnimationPhase phase = IntroAnimationPhase::kIdle;
    tick::NonBlockingDelayNs delay {};
    uint32_t element_index = 0U;
    bool phase_started = false;
  };

  /** @brief 注册飞控运行时参数。 */
  void RegisterParameters();
  /** @brief 注册飞控相关的 CLI 功能函数。 */
  void RegisterFunctions();
  /** @brief 根据当前传输通道刷新 Shell 横幅。 */
  void UpdateShellBanner();
  /** @brief 把配置同步到实际 PID 控制器实例。 */
  void ApplyPidConfiguration();
  /** @brief 重置开场动画状态机。 */
  void ResetIntroAnimation();
  /** @brief 推进到开场动画的下一个阶段。 */
  void AdvanceIntroAnimation(IntroAnimationPhase next_phase);
  /** @brief 驱动整个开场动画，返回 true 表示动画已完成。 */
  bool UpdateIntroAnimation(Shell *shell, bool start);
  /** @brief 逐字输出一行文本，用于打字机动画。 */
  bool StepTypewriterLine(Shell *shell, uint64_t now_ns, const char *text,
                          uint32_t delay_ms);
  /** @brief 输出旋转指示器动画。 */
  bool StepSpinnerLine(Shell *shell, uint64_t now_ns, const char *label,
                       uint8_t rounds, uint32_t frame_delay_ms);
  /** @brief 输出进度条动画。 */
  bool StepProgressLine(Shell *shell, uint64_t now_ns, const char *label,
                        uint8_t steps, uint32_t step_delay_ms);

  /** @brief 在注册表中按名称查找传输通道。 */
  const TransportBinding *FindTransport(const char *name) const;

  /** @brief 只读参数：返回当前激活的传输通道名。 */
  static bool GetTransportParameter(void *context, char *buffer,
                                    uint32_t bufferSize);
  /** @brief 只读参数：返回系统运行时间。 */
  static bool GetUptimeParameter(void *context, char *buffer,
                                 uint32_t bufferSize);

  /** @brief 输出飞控状态摘要。 */
  static bool StatusFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  /** @brief 触发系统软件复位。 */
  static bool RebootFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  /** @brief 重置 PID 控制器运行态。 */
  static bool PidResetFunction(Shell *shell, void *context, uint8_t argc,
                               const char *const *argv);
  /** @brief 执行一次 PID 示例计算并打印结果。 */
  static bool PidSampleFunction(Shell *shell, void *context, uint8_t argc,
                                const char *const *argv);
  /** @brief 列出全部已注册的 CLI 传输通道。 */
  static bool TransportListFunction(Shell *shell, void *context, uint8_t argc,
                                    const char *const *argv);
  /** @brief 切换当前 CLI 传输通道。 */
  static bool TransportUseFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv);
  /** @brief Shell 会话动画回调入口。 */
  static bool IntroAnimation(Shell *shell, void *context, bool start);

  /** @brief 当 PID 参数变化时重新应用配置。 */
  static void OnPidParameterUpdated(void *context);

private:
  RuntimeConfig runtime_ {};
  Pid rate_pid_;
  ParameterManager parameter_manager_ {};
  Shell shell_ {};

  TransportBinding transports_[kMaxTransportCount] {};
  uint8_t transport_count_ = 0U;
  const char *active_transport_name_ = "unbound";

  char banner_subtitle_[64] {};
  IntroAnimationState intro_animation_ {};
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
