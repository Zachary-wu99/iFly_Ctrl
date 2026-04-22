/**
 * @file shell.hpp
 * @brief 命令行 Shell 接口。
 */
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
  static constexpr uint8_t kMaxCommandCount = 12U; /**< 最大普通命令数量。 */
  static constexpr uint8_t kMaxFunctionCount = 16U; /**< 最大功能函数数量。 */
  static constexpr uint8_t kMaxParameterCount = 24U; /**< 最大参数数量。 */
  static constexpr uint8_t kMaxArgumentCount = 10U; /**< 单条命令最大参数个数。 */
  static constexpr uint16_t kInputLineBufferSize = 128U; /**< 输入行缓冲区大小。 */
  static constexpr uint16_t kValueBufferSize = 96U; /**< 参数值格式化缓冲区大小。 */
  static constexpr uint8_t kMaxPasswordLength = 31U; /**< 最大密码长度。 */

  using CommandHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc,
               const char *const *argv); /**< 命令处理函数签名。 */
  using FunctionHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc,
               const char *const *argv); /**< 功能调用处理函数签名。 */
  using ParameterGetter =
      bool (*)(void *context, char *buffer,
               uint32_t bufferSize); /**< 参数读取回调签名。 */
  using ParameterSetter =
      bool (*)(void *context, const char *value); /**< 参数写入回调签名。 */
  using SessionAnimation =
      bool (*)(Shell *shell, void *context,
               bool start); /**< 会话动画回调签名。 */

  /**
   * @brief 一条普通命令的注册信息。
   */
  struct Command final {
    const char *name = nullptr; /**< 命令名称。 */
    const char *help = nullptr; /**< 命令帮助文本。 */
    CommandHandler handler = nullptr; /**< 命令处理回调。 */
    void *context = nullptr; /**< 命令处理回调上下文。 */
  };

  /**
   * @brief 一条可通过 `call` 触发的功能函数注册信息。
   */
  struct Function final {
    const char *name = nullptr; /**< 功能名称。 */
    const char *help = nullptr; /**< 功能帮助文本。 */
    FunctionHandler handler = nullptr; /**< 功能处理回调。 */
    void *context = nullptr; /**< 功能处理回调上下文。 */
  };

  /**
   * @brief 一个可读写参数的注册信息。
   */
  struct Parameter final {
    const char *name = nullptr; /**< 参数名称。 */
    const char *help = nullptr; /**< 参数帮助文本。 */
    ParameterGetter getter = nullptr; /**< 参数读取回调。 */
    ParameterSetter setter = nullptr; /**< 参数写入回调。 */
    void *context = nullptr; /**< 参数回调上下文。 */
  };

  /**
   * @brief 构造一个默认未绑定 IO 的 Shell。
   */
  Shell();

  /**
   * @brief 绑定底层串口或 USB IO。
   *
   * @param io 底层串行 IO 对象。
   */
  void BindIo(SerialIoBase *io);

  /**
   * @brief 获取当前绑定的底层 IO。
   *
   * @return 当前绑定的 IO 指针，未绑定时返回 `nullptr`。
   */
  SerialIoBase *BoundIo() const {
    return io_;
  }

  /**
   * @brief 设置 Shell 横幅标题和副标题。
   *
   * @param title 横幅标题。
   * @param subtitle 横幅副标题。
   */
  void SetBanner(const char *title, const char *subtitle);

  /**
   * @brief 设置命令提示符。
   *
   * @param prompt 提示符文本。
   */
  void SetPrompt(const char *prompt);

  /**
   * @brief 设置登录密码。
   *
   * @param password 新密码文本。
   */
  void SetPassword(const char *password);

  /**
   * @brief 清空登录密码。
   */
  void ClearPassword();

  /**
   * @brief 设置激活按键及其提示文案。
   *
   * @param key 激活键值。
   * @param promptText 激活提示文本。
   */
  void SetActivationKey(uint8_t key, const char *promptText);

  /**
   * @brief 关闭激活按键机制。
   */
  void DisableActivationKey();

  /**
   * @brief 设置连接成功后播放的会话动画。
   *
   * @param animation 动画回调函数。
   * @param context 动画回调上下文。
   */
  void SetSessionAnimation(SessionAnimation animation, void *context);

  /**
   * @brief 注册一条普通命令。
   *
   * @param command 命令注册信息。
   * @return 注册成功返回 `true`。
   */
  bool RegisterCommand(const Command &command);

  /**
   * @brief 注册一条功能函数。
   *
   * @param function 功能注册信息。
   * @return 注册成功返回 `true`。
   */
  bool RegisterFunction(const Function &function);

  /**
   * @brief 注册一个参数。
   *
   * @param parameter 参数注册信息。
   * @return 注册成功返回 `true`。
   */
  bool RegisterParameter(const Parameter &parameter);

  /**
   * @brief 清空全部命令、函数和参数注册。
   */
  void ClearRegistrations();

  /**
   * @brief 轮询输入输出并驱动会话状态机。
   */
  void Poll();

  /**
   * @brief 重置当前会话并重新进入未登录流程。
   */
  void ResetSession();

  /**
   * @brief 判断当前是否已经通过登录流程。
   *
   * @return 已登录返回 `true`。
   */
  bool IsLoggedIn() const;

  /**
   * @brief 判断当前是否配置了密码。
   *
   * @return 已设置密码返回 `true`。
   */
  bool HasPassword() const;

  /**
   * @brief 判断当前底层链路是否可用。
   *
   * @return 链路可用返回 `true`。
   */
  bool IsConnected() const;

  /**
   * @brief 输出一段原始文本。
   *
   * @param text 待输出文本。
   */
  void Write(const char *text);

  /**
   * @brief 输出一行文本。
   *
   * @param text 待输出文本。
   */
  void WriteLine(const char *text);

  /**
   * @brief 按 `printf` 风格格式化输出。
   *
   * @param format 格式化字符串。
   */
  void Printf(const char *format, ...);

private:
  /**
   * @brief Shell 会话内部状态。
   */
  enum class SessionState : uint8_t {
    kDisconnected = 0U, /**< 未连接状态。 */
    kActivationPrompt, /**< 等待激活按键状态。 */
    kSessionAnimation, /**< 正在播放会话动画状态。 */
    kPasswordPrompt, /**< 等待密码输入状态。 */
    kReady, /**< 会话就绪状态。 */
  };

  /**
   * @brief 检测到底层连接建立后启动新会话。
   */
  void StartConnectedSession();

  /**
   * @brief 清空当前输入行缓存。
   */
  void ResetInputLine();

  /**
   * @brief 逐字节处理输入流。
   *
   * @param byteValue 当前输入字节。
   */
  void ProcessByte(uint8_t byteValue);

  /**
   * @brief 处理激活按键。
   */
  void HandleActivationTrigger();

  /**
   * @brief 动画结束后推进到下一会话阶段。
   */
  void AdvanceAfterAnimation();

  /**
   * @brief 处理一整行输入完成后的分发。
   */
  void HandleCompletedLine();

  /**
   * @brief 处理密码输入。
   */
  void HandlePasswordLine();

  /**
   * @brief 处理命令输入。
   */
  void HandleCommandLine();

  /**
   * @brief 按空白字符把输入行拆成参数数组。
   *
   * @param line 待拆分的输入行缓冲区。
   * @param argv 输出参数数组。
   * @return 实际拆分得到的参数个数。
   */
  uint8_t Tokenize(char *line, const char *argv[]) const;

  /**
   * @brief 执行内建命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool ExecuteBuiltin(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行用户注册命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool ExecuteCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `help` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleHelpCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `logout` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleLogoutCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `password` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandlePasswordCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `clear` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleClearCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `call` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleFunctionCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 通过函数别名直接调用功能。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleFunctionCallAlias(uint8_t argc, const char *const *argv);

  /**
   * @brief 执行 `param` 命令。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleParameterCommand(uint8_t argc, const char *const *argv);

  /**
   * @brief 处理参数读取别名入口。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleParameterGetAlias(uint8_t argc, const char *const *argv);

  /**
   * @brief 处理参数写入别名入口。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleParameterSetAlias(uint8_t argc, const char *const *argv);

  /**
   * @brief 处理参数列表别名入口。
   *
   * @param argc 参数个数。
   * @param argv 参数数组。
   * @return 执行成功返回 `true`。
   */
  bool HandleParameterListAlias(uint8_t argc, const char *const *argv);

  /**
   * @brief 按名称查找普通命令。
   *
   * @param name 命令名称。
   * @return 找到时返回命令注册项指针，否则返回 `nullptr`。
   */
  const Command *FindCommand(const char *name) const;

  /**
   * @brief 按名称查找功能函数。
   *
   * @param name 功能名称。
   * @return 找到时返回功能注册项指针，否则返回 `nullptr`。
   */
  const Function *FindFunction(const char *name) const;

  /**
   * @brief 按名称查找参数。
   *
   * @param name 参数名称。
   * @return 找到时返回参数注册项指针，否则返回 `nullptr`。
   */
  const Parameter *FindParameter(const char *name) const;

  /**
   * @brief 打印横幅。
   */
  void PrintBanner();

  /**
   * @brief 打印命令提示符。
   */
  void PrintPrompt();

  /**
   * @brief 打印密码提示符。
   */
  void PrintPasswordPrompt();

  /**
   * @brief 打印帮助总览。
   */
  void PrintHelpSummary() const;

  /**
   * @brief 打印命令列表。
   */
  void PrintCommandList() const;

  /**
   * @brief 打印功能列表。
   */
  void PrintFunctionList() const;

  /**
   * @brief 打印参数列表。
   */
  void PrintParameterList() const;

  /**
   * @brief 打印指定参数的当前值。
   *
   * @param parameter 目标参数注册项。
   */
  void PrintParameterValue(const Parameter &parameter) const;

  /**
   * @brief 向底层 IO 实际写入字节流。
   *
   * @param data 待写入数据首地址。
   * @param length 待写入字节数。
   * @return 实际写入的字节数。
   */
  uint32_t WriteBytes(const uint8_t *data, uint32_t length);

  /**
   * @brief 安全复制 C 字符串到固定缓冲区。
   *
   * @param source 输入字符串。
   * @param destination 输出缓冲区。
   * @param destinationSize 输出缓冲区大小。
   */
  static void CopyString(const char *source, char *destination,
                         uint32_t destinationSize);

  /**
   * @brief 判断字符是否为空白字符。
   *
   * @param character 待判断字符。
   * @return 是空白字符返回 `true`。
   */
  static bool IsWhitespace(char character);

private:
  SerialIoBase *io_ = nullptr; /**< 当前绑定的底层串行 IO。 */
  SessionState sessionState_ = SessionState::kDisconnected; /**< 当前会话状态。 */
  bool connectionActive_ = false; /**< 是否已经感知到底层链路连接。 */
  bool suppressNextLf_ = false; /**< 是否抑制下一个换行字符。 */

  const char *bannerTitle_ = "iFly Shell"; /**< 横幅标题。 */
  const char *bannerSubtitle_ = nullptr; /**< 横幅副标题。 */
  const char *prompt_ = "ifly> "; /**< 当前命令提示符。 */
  const char *activation_prompt_ = "Press SPACE to continue."; /**< 激活提示文本。 */
  uint8_t activation_key_ = 0U; /**< 激活按键值。 */
  SessionAnimation session_animation_ = nullptr; /**< 会话动画回调。 */
  void *session_animation_context_ = nullptr; /**< 会话动画回调上下文。 */

  char password_[kMaxPasswordLength + 1U] {}; /**< 当前登录密码缓冲区。 */
  char inputLine_[kInputLineBufferSize] {}; /**< 当前输入行缓冲区。 */
  uint16_t inputLength_ = 0U; /**< 当前输入行长度。 */

  Command commands_[kMaxCommandCount] {}; /**< 普通命令注册表。 */
  Function functions_[kMaxFunctionCount] {}; /**< 功能函数注册表。 */
  Parameter parameters_[kMaxParameterCount] {}; /**< 参数注册表。 */
  uint8_t commandCount_ = 0U; /**< 当前普通命令数量。 */
  uint8_t functionCount_ = 0U; /**< 当前功能函数数量。 */
  uint8_t parameterCount_ = 0U; /**< 当前参数数量。 */
};

} // namespace iFly

#endif /* IFLY_SHELL_HPP */
