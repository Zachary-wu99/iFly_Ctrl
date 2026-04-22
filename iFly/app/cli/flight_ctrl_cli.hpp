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

  /**
   * @brief 构造飞控 CLI 对象。
   */
  FlightCtrlCli();

  /**
   * @brief 初始化 CLI、参数入口和功能入口。
   */
  void Init();

  /**
   * @brief 注册一个可用的传输通道。
   *
   * @param name 传输通道名称。
   * @param io 对应的串行 IO 对象。
   * @return 注册成功返回 `true`。
   */
  bool RegisterTransport(const char *name, SerialIoBase *io);

  /**
   * @brief 切换当前使用的传输通道。
   *
   * @param name 目标传输通道名称。
   * @return 切换成功返回 `true`。
   */
  bool UseTransport(const char *name);

  /**
   * @brief 轮询驱动 CLI 运行。
   */
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
  static constexpr uint8_t kManagedParameterCount = 11U; /**< 受管参数数量。 */

  /**
   * @brief 传输通道绑定信息。
   */
  struct TransportBinding final {
    const char *name = nullptr; /**< 传输通道名称。 */
    SerialIoBase *io = nullptr; /**< 对应的串行 IO 对象。 */
  };

  /**
   * @brief 受管参数类型。
   */
  enum class ManagedParameterType : uint8_t {
    kFloat = 0U, /**< 浮点型参数。 */
    kUint32, /**< 无符号 32 位整型参数。 */
    kBool, /**< 布尔型参数。 */
  };

  /**
   * @brief 受管参数运行时上下文。
   */
  struct ManagedParameterContext final {
    FlightCtrlCli *owner = nullptr; /**< 所属 CLI 对象。 */
    const char *project_name = nullptr; /**< 工程参数中心中的参数名。 */
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
    bool phase_started = false; /**< 当前阶段是否已开始。 */
  };

  /**
   * @brief 注册所有参数入口。
   */
  void RegisterParameters();

  /**
   * @brief 注册所有功能入口。
   */
  void RegisterFunctions();

  /**
   * @brief 根据当前状态刷新 Shell 横幅。
   */
  void UpdateShellBanner();

  /**
   * @brief 将参数中心中的 PID 配置同步到本地控制器。
   */
  void ApplyPidConfiguration();

  /**
   * @brief 重置开机动画状态。
   */
  void ResetIntroAnimation();

  /**
   * @brief 推进开机动画到下一个阶段。
   *
   * @param next_phase 下一个动画阶段。
   */
  void AdvanceIntroAnimation(IntroAnimationPhase next_phase);

  /**
   * @brief 更新开机动画。
   *
   * @param shell 当前 Shell 对象。
   * @param start 是否为动画开始时刻。
   * @return 动画结束返回 `true`。
   */
  bool UpdateIntroAnimation(Shell *shell, bool start);

  /**
   * @brief 以打字机效果输出一行文本。
   *
   * @param shell 当前 Shell 对象。
   * @param now_ns 当前纳秒时间戳。
   * @param text 目标文本。
   * @param delay_ms 帧间延时，单位为毫秒。
   * @return 当前文本输出完成返回 `true`。
   */
  bool StepTypewriterLine(Shell *shell, uint64_t now_ns, const char *text,
                          uint32_t delay_ms);

  /**
   * @brief 以转圈动画输出一行文本。
   *
   * @param shell 当前 Shell 对象。
   * @param now_ns 当前纳秒时间戳。
   * @param label 标签文本。
   * @param rounds 转动轮数。
   * @param frame_delay_ms 帧间延时，单位为毫秒。
   * @return 当前动画输出完成返回 `true`。
   */
  bool StepSpinnerLine(Shell *shell, uint64_t now_ns, const char *label,
                       uint8_t rounds, uint32_t frame_delay_ms);

  /**
   * @brief 以进度条效果输出一行文本。
   *
   * @param shell 当前 Shell 对象。
   * @param now_ns 当前纳秒时间戳。
   * @param label 标签文本。
   * @param steps 进度步数。
   * @param step_delay_ms 步进延时，单位为毫秒。
   * @return 当前动画输出完成返回 `true`。
   */
  bool StepProgressLine(Shell *shell, uint64_t now_ns, const char *label,
                        uint8_t steps, uint32_t step_delay_ms);

  /**
   * @brief 按名称查找已注册传输通道。
   *
   * @param name 传输通道名称。
   * @return 找到时返回绑定对象指针，否则返回 `nullptr`。
   */
  const TransportBinding *FindTransport(const char *name) const;

  /**
   * @brief 获取当前传输通道参数值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetTransportParameter(void *context, char *buffer,
                                    uint32_t bufferSize);

  /**
   * @brief 获取系统运行时间参数值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetUptimeParameter(void *context, char *buffer,
                                 uint32_t bufferSize);

  /**
   * @brief 获取受管参数的当前值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetManagedParameter(void *context, char *buffer,
                                  uint32_t bufferSize);

  /**
   * @brief 设置受管参数的新值。
   *
   * @param context 回调上下文。
   * @param value 输入文本值。
   * @return 设置成功返回 `true`。
   */
  static bool SetManagedParameter(void *context, const char *value);

  /**
   * @brief `status` 功能实现。
   */
  static bool StatusFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);

  /**
   * @brief `reboot` 功能实现。
   */
  static bool RebootFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);

  /**
   * @brief `pid_reset` 功能实现。
   */
  static bool PidResetFunction(Shell *shell, void *context, uint8_t argc,
                               const char *const *argv);

  /**
   * @brief `pid_sample` 功能实现。
   */
  static bool PidSampleFunction(Shell *shell, void *context, uint8_t argc,
                                const char *const *argv);

  /**
   * @brief `transport_list` 功能实现。
   */
  static bool TransportListFunction(Shell *shell, void *context, uint8_t argc,
                                    const char *const *argv);

  /**
   * @brief `transport_use` 功能实现。
   */
  static bool TransportUseFunction(Shell *shell, void *context, uint8_t argc,
                                   const char *const *argv);

  /**
   * @brief Shell 会话动画回调入口。
   */
  static bool IntroAnimation(Shell *shell, void *context, bool start);

  /**
   * @brief 工程参数更新回调。
   */
  static void OnProjectParameterUpdated(const char *name, void *context);

  ProjectParameterManager &parameter_manager_; /**< 工程参数中心引用。 */
  Pid rate_pid_; /**< CLI 内部使用的 PID 控制器。 */
  Shell shell_ {}; /**< 命令行 Shell 实例。 */
  ManagedParameterContext managed_parameter_contexts_[kManagedParameterCount] {}; /**< 受管参数上下文表。 */

  TransportBinding transports_[kMaxTransportCount] {}; /**< 已注册传输通道表。 */
  uint8_t transport_count_ = 0U; /**< 当前已注册通道数。 */
  const char *active_transport_name_ = "unbound"; /**< 当前激活的传输通道名称。 */

  char banner_subtitle_[64] {}; /**< Shell 横幅副标题缓冲区。 */
  IntroAnimationState intro_animation_ {}; /**< 开机动画运行状态。 */
};

} // namespace iFly

#endif /* IFLY_FLIGHT_CTRL_CLI_HPP */
