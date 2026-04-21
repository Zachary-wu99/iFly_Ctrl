/**
 * @file flight_ctrl_cli.hpp
 * @brief 飞控 CLI 接口。
 */
#ifndef IFLY_FLIGHT_CTRL_CLI_HPP
#define IFLY_FLIGHT_CTRL_CLI_HPP

#include <stdint.h>

#include "pid.hpp"
#include "project_parameter_manager.hpp"
#include "shell.hpp"
#include "tick.hpp"

namespace iFly {

/**
 * @brief 飞控命令行控制台。
 */
class FlightCtrlCli final {
public:
  static constexpr uint8_t kMaxTransportCount = 4U; /**< 最大传输通道数量。 */

  FlightCtrlCli();
  void Init();
  bool RegisterTransport(const char *name, SerialIoBase *io);
  bool UseTransport(const char *name);
  void Poll();

  /**
   * @brief 获取可写 Shell 控制台。
   *
   * @return Shell 对象引用。
   */
  Shell &Console() {
    return shell_;
  }

  /**
   * @brief 获取只读 Shell 控制台。
   *
   * @return 只读 Shell 对象引用。
   */
  const Shell &Console() const {
    return shell_;
  }

private:
  static constexpr uint8_t kManagedParameterCount = 11U; /**< 受控参数数量。 */

  /**
   * @brief 传输通道绑定信息。
   */
  struct TransportBinding final {
    const char *name = nullptr; /**< 传输通道名称。 */
    SerialIoBase *io = nullptr; /**< 对应的串行 IO 对象。 */
  };

  /**
   * @brief 受控参数类型。
   */
  enum class ManagedParameterType : uint8_t {
    kFloat = 0U, /**< 浮点型参数。 */
    kUint32, /**< 无符号 32 位整型参数。 */
    kBool, /**< 布尔型参数。 */
  };

  /**
   * @brief 受控参数运行时上下文。
   */
  struct ManagedParameterContext final {
    FlightCtrlCli *owner = nullptr; /**< 所属 CLI 对象。 */
    const char *project_name = nullptr; /**< 参数在工程参数中心中的名字。 */
    ManagedParameterType type = ManagedParameterType::kFloat; /**< 参数类型。 */
    float min_float = 0.0f; /**< 浮点参数最小值。 */
    float max_float = 0.0f; /**< 浮点参数最大值。 */
    uint32_t min_u32 = 0U; /**< 无符号整型最小值。 */
    uint32_t max_u32 = 0U; /**< 无符号整型最大值。 */
  };

  /**
   * @brief 开机动画阶段定义。
   */
  enum class IntroAnimationPhase : uint8_t {
    kIdle = 0U, /**< 空闲阶段。 */
    kBootMessage, /**< 启动提示阶段。 */
    kTransportSpinner, /**< 传输通道检查阶段。 */
    kRegistrySpinner, /**< 参数注册检查阶段。 */
    kProgressBar, /**< 进度条阶段。 */
    kWelcomeMessage, /**< 欢迎信息阶段。 */
    kOnlineMessage, /**< 上线提示阶段。 */
    kCompleted, /**< 动画完成阶段。 */
  };

  /**
   * @brief 开机动画运行状态。
   */
  struct IntroAnimationState final {
    IntroAnimationPhase phase = IntroAnimationPhase::kIdle; /**< 当前阶段。 */
    tick::NonBlockingDelayNs delay {}; /**< 阶段内节拍延时器。 */
    uint32_t element_index = 0U; /**< 当前输出元素索引。 */
    bool phase_started = false; /**< 当前阶段是否已经开始。 */
  };

  void RegisterParameters();
  void RegisterFunctions();
  void UpdateShellBanner();
  void ApplyPidConfiguration();
  void ResetIntroAnimation();
  void AdvanceIntroAnimation(IntroAnimationPhase next_phase);
  bool UpdateIntroAnimation(Shell *shell, bool start);
  bool StepTypewriterLine(Shell *shell, uint64_t now_ns, const char *text,
                          uint32_t delay_ms);
  bool StepSpinnerLine(Shell *shell, uint64_t now_ns, const char *label,
                       uint8_t rounds, uint32_t frame_delay_ms);
  bool StepProgressLine(Shell *shell, uint64_t now_ns, const char *label,
                        uint8_t steps, uint32_t step_delay_ms);

  const TransportBinding *FindTransport(const char *name) const;

  static bool GetTransportParameter(void *context, char *buffer,
                                    uint32_t bufferSize);
  static bool GetUptimeParameter(void *context, char *buffer,
                                 uint32_t bufferSize);
  static bool GetManagedParameter(void *context, char *buffer,
                                  uint32_t bufferSize);
  static bool SetManagedParameter(void *context, const char *value);

  static bool StatusFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  static bool RebootFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);
  static bool PidResetFunction(Shell *shell, void *context, uint8_t argc,
                               const char *const *argv);
  static bool PidSampleFunction(Shell *shell, void *context, uint8_t argc,
                                const char *const *argv);
  static bool TransportListFunction(Shell *shell, void *context, uint8_t argc,
                                    const char *const *argv);
  static bool TransportUseFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv);
  static bool IntroAnimation(Shell *shell, void *context, bool start);
  static void OnProjectParameterUpdated(const char *name, void *context);

  ProjectParameterManager &parameter_manager_; /**< 工程参数中心引用。 */
  Pid rate_pid_; /**< CLI 内部使用的 PID 控制器对象。 */
  Shell shell_ {}; /**< 命令行 Shell 实例。 */
  ManagedParameterContext managed_parameter_contexts_[kManagedParameterCount] {}; /**< 受控参数上下文表。 */

  TransportBinding transports_[kMaxTransportCount] {}; /**< 已注册传输通道表。 */
  uint8_t transport_count_ = 0U; /**< 当前已注册通道数。 */
  const char *active_transport_name_ = "unbound"; /**< 当前激活的传输通道名称。 */

  char banner_subtitle_[64] {}; /**< Shell 横幅副标题缓冲区。 */
  IntroAnimationState intro_animation_ {}; /**< 开机动画运行状态。 */
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
