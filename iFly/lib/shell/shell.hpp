// 命令行 Shell 接口。
// 提供命令、函数、参数和登录流程的统一控制台能力。
#ifndef IFLY_SHELL_HPP
#define IFLY_SHELL_HPP

#include <stdint.h>

#include "serial_io_base.hpp"

namespace iFly {

/**
 * @brief 交互式命令行 Shell。
 *
 * @details
 * 负责管理串口会话、登录流程、命令/函数/参数注册表以及输入行解析，
 * 适合作为 MCU 调试控制台的统一入口。
 */
class Shell final {
public:
  static constexpr uint8_t kMaxCommandCount = 12U;
  static constexpr uint8_t kMaxFunctionCount = 16U;
  static constexpr uint8_t kMaxParameterCount = 24U;
  static constexpr uint8_t kMaxArgumentCount = 10U;
  static constexpr uint16_t kInputLineBufferSize = 128U;
  static constexpr uint16_t kValueBufferSize = 96U;
  static constexpr uint8_t kMaxPasswordLength = 31U;

  /** @brief 命令处理函数签名。 */
  using CommandHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc, const char *const *argv);
  /** @brief 功能调用处理函数签名。 */
  using FunctionHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc, const char *const *argv);
  /** @brief 参数读取回调签名。 */
  using ParameterGetter = bool (*)(void *context, char *buffer, uint32_t bufferSize);
  /** @brief 参数写入回调签名。 */
  using ParameterSetter = bool (*)(void *context, const char *value);
  /** @brief 会话动画回调签名，返回 true 表示动画结束。 */
  using SessionAnimation = bool (*)(Shell *shell, void *context, bool start);

  /** @brief 一条普通命令的注册信息。 */
  struct Command final {
    const char *name = nullptr;
    const char *help = nullptr;
    CommandHandler handler = nullptr;
    void *context = nullptr;
  };

  /** @brief 一条可通过 `call` 触发的功能函数注册信息。 */
  struct Function final {
    const char *name = nullptr;
    const char *help = nullptr;
    FunctionHandler handler = nullptr;
    void *context = nullptr;
  };

  /** @brief 一个可读/可写参数的注册信息。 */
  struct Parameter final {
    const char *name = nullptr;
    const char *help = nullptr;
    ParameterGetter getter = nullptr;
    ParameterSetter setter = nullptr;
    void *context = nullptr;
  };

  /** @brief 构造一个默认未绑定 IO 的 Shell。 */
  Shell();

  /** @brief 绑定底层串口或 USB IO。 */
  void BindIo(SerialIoBase *io);
  /** @brief 返回当前绑定的底层 IO。 */
  SerialIoBase *BoundIo() const {
    return io_;
  }

  /** @brief 设置 Shell 横幅标题和副标题。 */
  void SetBanner(const char *title, const char *subtitle);
  /** @brief 设置命令提示符。 */
  void SetPrompt(const char *prompt);
  /** @brief 设置登录密码。 */
  void SetPassword(const char *password);
  /** @brief 清空登录密码。 */
  void ClearPassword();
  /** @brief 设置激活按键及其提示文案。 */
  void SetActivationKey(uint8_t key, const char *promptText);
  /** @brief 关闭激活按键机制。 */
  void DisableActivationKey();
  /** @brief 设置连接成功后播放的会话动画。 */
  void SetSessionAnimation(SessionAnimation animation, void *context);

  /** @brief 注册一条普通命令。 */
  bool RegisterCommand(const Command &command);
  /** @brief 注册一条功能函数。 */
  bool RegisterFunction(const Function &function);
  /** @brief 注册一个参数。 */
  bool RegisterParameter(const Parameter &parameter);
  /** @brief 清空全部命令、函数和参数注册。 */
  void ClearRegistrations();

  /** @brief 轮询输入输出并驱动会话状态机。 */
  void Poll();
  /** @brief 重置当前会话，重新进入未登录流程。 */
  void ResetSession();

  /** @brief 当前是否已经通过登录流程。 */
  bool IsLoggedIn() const;
  /** @brief 当前是否配置了密码。 */
  bool HasPassword() const;
  /** @brief 当前底层链路是否可用。 */
  bool IsConnected() const;

  /** @brief 输出一段原始文本。 */
  void Write(const char *text);
  /** @brief 输出一行文本。 */
  void WriteLine(const char *text);
  /** @brief 按 `printf` 风格格式化输出。 */
  void Printf(const char *format, ...);

private:
  /** @brief Shell 会话内部状态。 */
  enum class SessionState : uint8_t {
    kDisconnected = 0U,
    kActivationPrompt,
    kSessionAnimation,
    kPasswordPrompt,
    kReady,
  };

  /** @brief 检测到底层连接建立后启动新会话。 */
  void StartConnectedSession();
  /** @brief 清空当前输入行缓存。 */
  void ResetInputLine();
  /** @brief 逐字节处理输入流。 */
  void ProcessByte(uint8_t byteValue);
  /** @brief 处理激活按键。 */
  void HandleActivationTrigger();
  /** @brief 动画结束后推进到下一会话阶段。 */
  void AdvanceAfterAnimation();
  /** @brief 处理一整行输入完成后的分发。 */
  void HandleCompletedLine();
  /** @brief 处理密码输入。 */
  void HandlePasswordLine();
  /** @brief 处理命令输入。 */
  void HandleCommandLine();
  /** @brief 按空白把输入行拆成 argv。 */
  uint8_t Tokenize(char *line, const char *argv[]) const;

  /** @brief 执行内建命令。 */
  bool ExecuteBuiltin(uint8_t argc, const char *const *argv);
  /** @brief 执行用户注册命令。 */
  bool ExecuteCommand(uint8_t argc, const char *const *argv);

  /** @brief `help` 命令实现。 */
  bool HandleHelpCommand(uint8_t argc, const char *const *argv);
  /** @brief `logout` 命令实现。 */
  bool HandleLogoutCommand(uint8_t argc, const char *const *argv);
  /** @brief `password` 命令实现。 */
  bool HandlePasswordCommand(uint8_t argc, const char *const *argv);
  /** @brief `clear` 命令实现。 */
  bool HandleClearCommand(uint8_t argc, const char *const *argv);
  /** @brief `call` 命令实现。 */
  bool HandleFunctionCommand(uint8_t argc, const char *const *argv);
  /** @brief 直接按函数名调用的别名入口。 */
  bool HandleFunctionCallAlias(uint8_t argc, const char *const *argv);
  /** @brief `param` 命令实现。 */
  bool HandleParameterCommand(uint8_t argc, const char *const *argv);
  /** @brief 参数读取别名入口。 */
  bool HandleParameterGetAlias(uint8_t argc, const char *const *argv);
  /** @brief 参数写入别名入口。 */
  bool HandleParameterSetAlias(uint8_t argc, const char *const *argv);
  /** @brief 参数列表别名入口。 */
  bool HandleParameterListAlias(uint8_t argc, const char *const *argv);

  /** @brief 按名称查找普通命令。 */
  const Command *FindCommand(const char *name) const;
  /** @brief 按名称查找功能函数。 */
  const Function *FindFunction(const char *name) const;
  /** @brief 按名称查找参数。 */
  const Parameter *FindParameter(const char *name) const;

  /** @brief 打印横幅。 */
  void PrintBanner();
  /** @brief 打印命令提示符。 */
  void PrintPrompt();
  /** @brief 打印密码提示符。 */
  void PrintPasswordPrompt();
  /** @brief 打印帮助总览。 */
  void PrintHelpSummary() const;
  /** @brief 打印命令列表。 */
  void PrintCommandList() const;
  /** @brief 打印函数列表。 */
  void PrintFunctionList() const;
  /** @brief 打印参数列表。 */
  void PrintParameterList() const;
  /** @brief 打印指定参数的当前值。 */
  void PrintParameterValue(const Parameter &parameter) const;

  /** @brief 向底层 IO 实际写入字节流。 */
  uint32_t WriteBytes(const uint8_t *data, uint32_t length);
  /** @brief 安全复制 C 字符串到固定缓冲区。 */
  static void CopyString(const char *source, char *destination,
                         uint32_t destinationSize);
  /** @brief 判断字符是否为空白。 */
  static bool IsWhitespace(char character);

private:
  SerialIoBase *io_ = nullptr;
  SessionState sessionState_ = SessionState::kDisconnected;
  bool connectionActive_ = false;
  bool suppressNextLf_ = false;

  const char *bannerTitle_ = "iFly Shell";
  const char *bannerSubtitle_ = nullptr;
  const char *prompt_ = "ifly> ";
  const char *activation_prompt_ = "Press SPACE to continue.";
  uint8_t activation_key_ = 0U;
  SessionAnimation session_animation_ = nullptr;
  void *session_animation_context_ = nullptr;

  char password_[kMaxPasswordLength + 1U] {};
  char inputLine_[kInputLineBufferSize] {};
  uint16_t inputLength_ = 0U;

  Command commands_[kMaxCommandCount] {};
  Function functions_[kMaxFunctionCount] {};
  Parameter parameters_[kMaxParameterCount] {};
  uint8_t commandCount_ = 0U;
  uint8_t functionCount_ = 0U;
  uint8_t parameterCount_ = 0U;
};

} // namespace iFly

#endif /* IFLY_SHELL_HPP */
