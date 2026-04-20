// 命令行 Shell 实现。
// 包括会话状态机、命令解析、参数访问和输出格式控制。
#include "shell.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace iFly {

namespace {

constexpr uint8_t kBackspace = 0x08U;
constexpr uint8_t kDelete = 0x7FU;
constexpr uint8_t kCarriageReturn = '\r';
constexpr uint8_t kLineFeed = '\n';
constexpr uint8_t kEscape = 0x1BU;
constexpr uint32_t kMaxWriteAttempts = 16U;

} // namespace

// 构造Shell并初始化默认成员状态。
Shell::Shell()
{
  ClearPassword();
  ResetSession();
}

// 绑定当前 Shell 使用的底层 IO。
void Shell::BindIo(SerialIoBase *io)
{
  io_ = io;
  ResetSession();
}

// 设置 Shell 标题和副标题。
void Shell::SetBanner(const char *title, const char *subtitle)
{
  bannerTitle_ = title;
  bannerSubtitle_ = subtitle;
}

// 设置命令提示符文本。
void Shell::SetPrompt(const char *prompt)
{
  prompt_ = prompt;
}

// 设置登录密码。
void Shell::SetPassword(const char *password)
{
  CopyString(password, password_, sizeof(password_));
}

// 设置唤起终端会话的激活键。
void Shell::SetActivationKey(uint8_t key, const char *promptText)
{
  activation_key_ = key;
  if ((promptText != nullptr) && (promptText[0] != '\0')) {
    activation_prompt_ = promptText;
  }
}

// 禁用终端激活键。
void Shell::DisableActivationKey()
{
  activation_key_ = 0U;
}

// 设置会话开场动画回调。
void Shell::SetSessionAnimation(SessionAnimation animation, void *context)
{
  session_animation_ = animation;
  session_animation_context_ = context;
}

// 清空当前密码配置。
void Shell::ClearPassword()
{
  password_[0] = '\0';
}

// 注册内建命令。
bool Shell::RegisterCommand(const Command &command)
{
  if ((command.name == nullptr) || (command.handler == nullptr) ||
      (commandCount_ >= kMaxCommandCount)) {
    return false;
  }

  commands_[commandCount_] = command;
  ++commandCount_;
  return true;
}

// 注册函数式命令。
bool Shell::RegisterFunction(const Function &function)
{
  if ((function.name == nullptr) || (function.handler == nullptr) ||
      (functionCount_ >= kMaxFunctionCount)) {
    return false;
  }

  functions_[functionCount_] = function;
  ++functionCount_;
  return true;
}

// 注册参数项。
bool Shell::RegisterParameter(const Parameter &parameter)
{
  if ((parameter.name == nullptr) || (parameter.getter == nullptr) ||
      (parameterCount_ >= kMaxParameterCount)) {
    return false;
  }

  parameters_[parameterCount_] = parameter;
  ++parameterCount_;
  return true;
}

// 清空所有已注册命令和参数。
void Shell::ClearRegistrations()
{
  commandCount_ = 0U;
  functionCount_ = 0U;
  parameterCount_ = 0U;
}

// 轮询并处理当前输入输出状态。
void Shell::Poll()
{
  if (io_ == nullptr) {
    return;
  }

  const bool connected = io_->IsConnected();
  if (!connected) {
    connectionActive_ = false;
    sessionState_ = SessionState::kDisconnected;
    ResetInputLine();
    suppressNextLf_ = false;
    return;
  }

  if (!connectionActive_) {
    StartConnectedSession();
  }

  uint8_t rxBuffer[32] {};
  uint32_t readLength = io_->Read(rxBuffer, sizeof(rxBuffer));
  while (readLength > 0U) {
    uint32_t index = 0U;
    for (index = 0U; index < readLength; ++index) {
      ProcessByte(rxBuffer[index]);
    }

    readLength = io_->Read(rxBuffer, sizeof(rxBuffer));
  }

  if (sessionState_ == SessionState::kSessionAnimation) {
    if ((session_animation_ == nullptr) ||
        session_animation_(this, session_animation_context_, false)) {
      AdvanceAfterAnimation();
    }
  }
}

// 重置当前会话状态。
void Shell::ResetSession()
{
  connectionActive_ = false;
  sessionState_ = SessionState::kDisconnected;
  suppressNextLf_ = false;
  ResetInputLine();
}

// 返回当前是否已经登录。
bool Shell::IsLoggedIn() const
{
  return sessionState_ == SessionState::kReady;
}

// 返回当前是否配置了密码。
bool Shell::HasPassword() const
{
  return password_[0] != '\0';
}

// 返回当前是否已经连接可用。
bool Shell::IsConnected() const
{
  return (io_ != nullptr) && io_->IsConnected();
}

// 向发送路径写入数据。
void Shell::Write(const char *text)
{
  if (text == nullptr) {
    return;
  }

  (void)WriteBytes(reinterpret_cast<const uint8_t *>(text),
                   static_cast<uint32_t>(strlen(text)));
}

// 输出一行文本并追加换行。
void Shell::WriteLine(const char *text)
{
  if (text != nullptr) {
    Write(text);
  }
  Write("\r\n");
}

// 按格式化方式输出文本。
void Shell::Printf(const char *format, ...)
{
  if (format == nullptr) {
    return;
  }

  char buffer[192] {};
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (written <= 0) {
    return;
  }

  buffer[sizeof(buffer) - 1U] = '\0';
  Write(buffer);
}

// 启动一个已连接的交互会话。
void Shell::StartConnectedSession()
{
  connectionActive_ = true;
  suppressNextLf_ = false;
  ResetInputLine();

  PrintBanner();
  if (activation_key_ != 0U) {
    sessionState_ = SessionState::kActivationPrompt;
    if ((activation_prompt_ != nullptr) && (activation_prompt_[0] != '\0')) {
      WriteLine(activation_prompt_);
    }
  } else if (HasPassword()) {
    sessionState_ = SessionState::kPasswordPrompt;
    PrintPasswordPrompt();
  } else {
    sessionState_ = SessionState::kReady;
    PrintPrompt();
  }
}

// 清空当前输入行缓冲。
void Shell::ResetInputLine()
{
  inputLength_ = 0U;
  inputLine_[0] = '\0';
}

// 处理单个输入字节。
void Shell::ProcessByte(uint8_t byteValue)
{
  if (sessionState_ == SessionState::kActivationPrompt) {
    if (byteValue == activation_key_) {
      HandleActivationTrigger();
    }
    return;
  }

  if (sessionState_ == SessionState::kSessionAnimation) {
    return;
  }

  if (suppressNextLf_ && (byteValue == kLineFeed)) {
    suppressNextLf_ = false;
    return;
  }

  if ((byteValue == kCarriageReturn) || (byteValue == kLineFeed)) {
    suppressNextLf_ = (byteValue == kCarriageReturn);
    HandleCompletedLine();
    return;
  }

  if ((byteValue == kBackspace) || (byteValue == kDelete)) {
    if (inputLength_ > 0U) {
      --inputLength_;
      inputLine_[inputLength_] = '\0';
      Write("\b \b");
    }
    return;
  }

  if ((byteValue < 32U) || (byteValue > 126U)) {
    return;
  }

  if (inputLength_ >= (kInputLineBufferSize - 1U)) {
    return;
  }

  inputLine_[inputLength_] = static_cast<char>(byteValue);
  ++inputLength_;
  inputLine_[inputLength_] = '\0';

  if (sessionState_ == SessionState::kPasswordPrompt) {
    Write("*");
  } else {
    char text[2] = {static_cast<char>(byteValue), '\0'};
    Write(text);
  }
}

// 处理终端激活按键。
void Shell::HandleActivationTrigger()
{
  Write("\r\n");
  ResetInputLine();

  if (session_animation_ != nullptr) {
    sessionState_ = SessionState::kSessionAnimation;
    if (session_animation_(this, session_animation_context_, true)) {
      AdvanceAfterAnimation();
    }
    return;
  }

  AdvanceAfterAnimation();
}

// 在动画结束后推进会话状态。
void Shell::AdvanceAfterAnimation()
{
  if (!connectionActive_) {
    return;
  }

  if (HasPassword()) {
    sessionState_ = SessionState::kPasswordPrompt;
    PrintPasswordPrompt();
    return;
  }

  sessionState_ = SessionState::kReady;
  PrintPrompt();
}

// 处理一整行已完成输入的文本。
void Shell::HandleCompletedLine()
{
  Write("\r\n");

  if (sessionState_ == SessionState::kPasswordPrompt) {
    HandlePasswordLine();
  } else if (sessionState_ == SessionState::kReady) {
    HandleCommandLine();
  }

  ResetInputLine();

  if (!connectionActive_) {
    return;
  }

  if (sessionState_ == SessionState::kPasswordPrompt) {
    PrintPasswordPrompt();
  } else if (sessionState_ == SessionState::kReady) {
    PrintPrompt();
  }
}

// 处理密码输入行。
void Shell::HandlePasswordLine()
{
  if (strcmp(inputLine_, password_) == 0) {
    sessionState_ = SessionState::kReady;
    WriteLine("Login successful.");
  } else {
    WriteLine("Password incorrect.");
  }
}

// 处理普通命令输入行。
void Shell::HandleCommandLine()
{
  const char *argv[kMaxArgumentCount] {};
  uint8_t argc = Tokenize(inputLine_, argv);

  if (argc == 0U) {
    return;
  }

  if (ExecuteBuiltin(argc, argv)) {
    return;
  }

  if (ExecuteCommand(argc, argv)) {
    return;
  }

  Printf("Unknown command: %s\r\n", argv[0]);
}

// 把输入行拆分为参数数组。
uint8_t Shell::Tokenize(char *line, const char *argv[]) const
{
  if ((line == nullptr) || (argv == nullptr)) {
    return 0U;
  }

  uint8_t argc = 0U;
  char *cursor = line;
  while (*cursor != '\0') {
    while (IsWhitespace(*cursor)) {
      *cursor = '\0';
      ++cursor;
    }

    if (*cursor == '\0') {
      break;
    }

    if (argc >= kMaxArgumentCount) {
      break;
    }

    if (*cursor == '"') {
      ++cursor;
      argv[argc] = cursor;
      ++argc;

      while ((*cursor != '\0') && (*cursor != '"')) {
        ++cursor;
      }

      if (*cursor == '"') {
        *cursor = '\0';
        ++cursor;
      }
      continue;
    }

    argv[argc] = cursor;
    ++argc;

    while ((*cursor != '\0') && !IsWhitespace(*cursor)) {
      ++cursor;
    }
  }

  return argc;
}

// 尝试执行内建命令。
bool Shell::ExecuteBuiltin(uint8_t argc, const char *const *argv)
{
  if ((argc == 0U) || (argv == nullptr) || (argv[0] == nullptr)) {
    return false;
  }

  if ((strcmp(argv[0], "help") == 0) || (strcmp(argv[0], "?") == 0)) {
    return HandleHelpCommand(argc, argv);
  }

  if (strcmp(argv[0], "logout") == 0) {
    return HandleLogoutCommand(argc, argv);
  }

  if (strcmp(argv[0], "passwd") == 0) {
    return HandlePasswordCommand(argc, argv);
  }

  if (strcmp(argv[0], "clear") == 0) {
    return HandleClearCommand(argc, argv);
  }

  if (strcmp(argv[0], "func") == 0) {
    return HandleFunctionCommand(argc, argv);
  }

  if (strcmp(argv[0], "call") == 0) {
    return HandleFunctionCallAlias(argc, argv);
  }

  if (strcmp(argv[0], "param") == 0) {
    return HandleParameterCommand(argc, argv);
  }

  if (strcmp(argv[0], "get") == 0) {
    return HandleParameterGetAlias(argc, argv);
  }

  if (strcmp(argv[0], "set") == 0) {
    return HandleParameterSetAlias(argc, argv);
  }

  if ((strcmp(argv[0], "params") == 0) || (strcmp(argv[0], "print") == 0)) {
    return HandleParameterListAlias(argc, argv);
  }

  return false;
}

// 执行注册的命令或函数。
bool Shell::ExecuteCommand(uint8_t argc, const char *const *argv)
{
  const Command *command = FindCommand(argv[0]);
  if (command == nullptr) {
    return false;
  }

  if (!command->handler(this, command->context, argc, argv)) {
    Printf("Command failed: %s\r\n", command->name);
  }
  return true;
}

// 处理 `help` 命令。
bool Shell::HandleHelpCommand(uint8_t argc, const char *const *argv)
{
  if (argc <= 1U) {
    PrintHelpSummary();
    return true;
  }

  if (strcmp(argv[1], "commands") == 0) {
    PrintCommandList();
    return true;
  }

  if (strcmp(argv[1], "functions") == 0) {
    PrintFunctionList();
    return true;
  }

  if (strcmp(argv[1], "params") == 0) {
    PrintParameterList();
    return true;
  }

  PrintHelpSummary();
  return true;
}

// 处理 `logout` 命令。
bool Shell::HandleLogoutCommand(uint8_t argc, const char *const *argv)
{
  (void)argc;
  (void)argv;

  if (!HasPassword()) {
    WriteLine("Password login is disabled.");
    return true;
  }

  sessionState_ = SessionState::kPasswordPrompt;
  WriteLine("Logged out.");
  return true;
}

// 处理 `password` 命令。
bool Shell::HandlePasswordCommand(uint8_t argc, const char *const *argv)
{
  if (!HasPassword()) {
    WriteLine("Password login is disabled.");
    return true;
  }

  if (argc != 3U) {
    WriteLine("Usage: passwd <old_password> <new_password>");
    return true;
  }

  if (strcmp(argv[1], password_) != 0) {
    WriteLine("Current password is incorrect.");
    return true;
  }

  SetPassword(argv[2]);
  WriteLine("Password updated.");
  return true;
}

// 处理 `clear` 命令。
bool Shell::HandleClearCommand(uint8_t argc, const char *const *argv)
{
  (void)argc;
  (void)argv;

  char sequence[8] {};
  sequence[0] = static_cast<char>(kEscape);
  sequence[1] = '[';
  sequence[2] = '2';
  sequence[3] = 'J';
  sequence[4] = static_cast<char>(kEscape);
  sequence[5] = '[';
  sequence[6] = 'H';
  sequence[7] = '\0';
  Write(sequence);
  return true;
}

// 处理 `function` 子命令。
bool Shell::HandleFunctionCommand(uint8_t argc, const char *const *argv)
{
  if ((argc == 1U) || (strcmp(argv[1], "list") == 0)) {
    PrintFunctionList();
    return true;
  }

  if ((strcmp(argv[1], "call") == 0) && (argc >= 3U)) {
    const Function *function = FindFunction(argv[2]);
    if (function == nullptr) {
      Printf("Unknown function: %s\r\n", argv[2]);
      return true;
    }

    if (!function->handler(this, function->context, static_cast<uint8_t>(argc - 3U),
                           argv + 3U)) {
      Printf("Function failed: %s\r\n", function->name);
    }
    return true;
  }

  WriteLine("Usage: func list");
  WriteLine("       func call <name> [args...]");
  return true;
}

// 处理函数调用别名。
bool Shell::HandleFunctionCallAlias(uint8_t argc, const char *const *argv)
{
  if (argc < 2U) {
    WriteLine("Usage: call <name> [args...]");
    return true;
  }

  const Function *function = FindFunction(argv[1]);
  if (function == nullptr) {
    Printf("Unknown function: %s\r\n", argv[1]);
    return true;
  }

  if (!function->handler(this, function->context, static_cast<uint8_t>(argc - 2U),
                         argv + 2U)) {
    Printf("Function failed: %s\r\n", function->name);
  }
  return true;
}

// 处理 `parameter` 子命令。
bool Shell::HandleParameterCommand(uint8_t argc, const char *const *argv)
{
  if ((argc == 1U) || (strcmp(argv[1], "list") == 0) ||
      (strcmp(argv[1], "print") == 0)) {
    PrintParameterList();
    return true;
  }

  if ((strcmp(argv[1], "get") == 0) && (argc >= 3U)) {
    const Parameter *parameter = FindParameter(argv[2]);
    if (parameter == nullptr) {
      Printf("Unknown parameter: %s\r\n", argv[2]);
      return true;
    }

    PrintParameterValue(*parameter);
    return true;
  }

  if ((strcmp(argv[1], "set") == 0) && (argc >= 4U)) {
    const Parameter *parameter = FindParameter(argv[2]);
    if (parameter == nullptr) {
      Printf("Unknown parameter: %s\r\n", argv[2]);
      return true;
    }

    if (parameter->setter == nullptr) {
      Printf("Parameter is read-only: %s\r\n", parameter->name);
      return true;
    }

    if (!parameter->setter(parameter->context, argv[3])) {
      Printf("Parameter set failed: %s\r\n", parameter->name);
      return true;
    }

    PrintParameterValue(*parameter);
    return true;
  }

  WriteLine("Usage: param list");
  WriteLine("       param get <name>");
  WriteLine("       param set <name> <value>");
  return true;
}

// 处理参数读取别名。
bool Shell::HandleParameterGetAlias(uint8_t argc, const char *const *argv)
{
  if (argc != 2U) {
    WriteLine("Usage: get <name>");
    return true;
  }

  const Parameter *parameter = FindParameter(argv[1]);
  if (parameter == nullptr) {
    Printf("Unknown parameter: %s\r\n", argv[1]);
    return true;
  }

  PrintParameterValue(*parameter);
  return true;
}

// 处理参数写入别名。
bool Shell::HandleParameterSetAlias(uint8_t argc, const char *const *argv)
{
  if (argc != 3U) {
    WriteLine("Usage: set <name> <value>");
    return true;
  }

  const Parameter *parameter = FindParameter(argv[1]);
  if (parameter == nullptr) {
    Printf("Unknown parameter: %s\r\n", argv[1]);
    return true;
  }

  if (parameter->setter == nullptr) {
    Printf("Parameter is read-only: %s\r\n", parameter->name);
    return true;
  }

  if (!parameter->setter(parameter->context, argv[2])) {
    Printf("Parameter set failed: %s\r\n", parameter->name);
    return true;
  }

  PrintParameterValue(*parameter);
  return true;
}

// 处理参数列表别名。
bool Shell::HandleParameterListAlias(uint8_t argc, const char *const *argv)
{
  (void)argc;
  (void)argv;

  PrintParameterList();
  return true;
}

// 按名称查找已注册命令。
const Shell::Command *Shell::FindCommand(const char *name) const
{
  if (name == nullptr) {
    return nullptr;
  }

  uint8_t index = 0U;
  for (index = 0U; index < commandCount_; ++index) {
    if ((commands_[index].name != nullptr) &&
        (strcmp(commands_[index].name, name) == 0)) {
      return &commands_[index];
    }
  }

  return nullptr;
}

// 按名称查找已注册函数。
const Shell::Function *Shell::FindFunction(const char *name) const
{
  if (name == nullptr) {
    return nullptr;
  }

  uint8_t index = 0U;
  for (index = 0U; index < functionCount_; ++index) {
    if ((functions_[index].name != nullptr) &&
        (strcmp(functions_[index].name, name) == 0)) {
      return &functions_[index];
    }
  }

  return nullptr;
}

// 按名称查找已注册参数。
const Shell::Parameter *Shell::FindParameter(const char *name) const
{
  if (name == nullptr) {
    return nullptr;
  }

  uint8_t index = 0U;
  for (index = 0U; index < parameterCount_; ++index) {
    if ((parameters_[index].name != nullptr) &&
        (strcmp(parameters_[index].name, name) == 0)) {
      return &parameters_[index];
    }
  }

  return nullptr;
}

// 输出欢迎横幅。
void Shell::PrintBanner()
{
  WriteLine("");
  WriteLine("========================================");
  if (bannerTitle_ != nullptr) {
    WriteLine(bannerTitle_);
  }
  if ((bannerSubtitle_ != nullptr) && (bannerSubtitle_[0] != '\0')) {
    WriteLine(bannerSubtitle_);
  }
  if (HasPassword()) {
    WriteLine("Login required.");
  } else {
    WriteLine("Password login disabled.");
  }
  WriteLine("Type 'help' after login.");
  WriteLine("========================================");
}

// 输出命令提示符。
void Shell::PrintPrompt()
{
  if ((prompt_ != nullptr) && (prompt_[0] != '\0')) {
    Write(prompt_);
  }
}

// 输出密码提示符。
void Shell::PrintPasswordPrompt()
{
  Write("Password: ");
}

// 输出帮助摘要。
void Shell::PrintHelpSummary() const
{
  Shell *shell = const_cast<Shell *>(this);
  shell->WriteLine("Built-in commands:");
  shell->WriteLine("  help [commands|functions|params]");
  shell->WriteLine("  clear");
  shell->WriteLine("  logout");
  shell->WriteLine("  passwd <old_password> <new_password>");
  shell->WriteLine("  func list");
  shell->WriteLine("  func call <name> [args...]");
  shell->WriteLine("  call <name> [args...]");
  shell->WriteLine("  param list");
  shell->WriteLine("  param get <name>");
  shell->WriteLine("  param set <name> <value>");
  shell->WriteLine("  get <name>");
  shell->WriteLine("  set <name> <value>");
  shell->WriteLine("  params");

  shell->Printf("Registered commands: %u\r\n", static_cast<unsigned>(commandCount_));
  shell->Printf("Registered functions: %u\r\n", static_cast<unsigned>(functionCount_));
  shell->Printf("Registered parameters: %u\r\n", static_cast<unsigned>(parameterCount_));
}

// 输出命令列表。
void Shell::PrintCommandList() const
{
  Shell *shell = const_cast<Shell *>(this);
  shell->WriteLine("Commands:");

  if (commandCount_ == 0U) {
    shell->WriteLine("  (none)");
    return;
  }

  uint8_t index = 0U;
  for (index = 0U; index < commandCount_; ++index) {
    shell->Printf("  %s", commands_[index].name);
    if ((commands_[index].help != nullptr) && (commands_[index].help[0] != '\0')) {
      shell->Printf(" : %s", commands_[index].help);
    }
    shell->Write("\r\n");
  }
}

// 输出函数列表。
void Shell::PrintFunctionList() const
{
  Shell *shell = const_cast<Shell *>(this);
  shell->WriteLine("Functions:");

  if (functionCount_ == 0U) {
    shell->WriteLine("  (none)");
    return;
  }

  uint8_t index = 0U;
  for (index = 0U; index < functionCount_; ++index) {
    shell->Printf("  %s", functions_[index].name);
    if ((functions_[index].help != nullptr) && (functions_[index].help[0] != '\0')) {
      shell->Printf(" : %s", functions_[index].help);
    }
    shell->Write("\r\n");
  }
}

// 输出参数列表。
void Shell::PrintParameterList() const
{
  Shell *shell = const_cast<Shell *>(this);
  shell->WriteLine("Parameters:");

  if (parameterCount_ == 0U) {
    shell->WriteLine("  (none)");
    return;
  }

  uint8_t index = 0U;
  for (index = 0U; index < parameterCount_; ++index) {
    shell->PrintParameterValue(parameters_[index]);
  }
}

// 输出指定参数的当前值。
void Shell::PrintParameterValue(const Parameter &parameter) const
{
  if ((parameter.name == nullptr) || (parameter.getter == nullptr)) {
    return;
  }

  Shell *shell = const_cast<Shell *>(this);
  char valueBuffer[kValueBufferSize] {};
  if (!parameter.getter(parameter.context, valueBuffer, sizeof(valueBuffer))) {
    shell->Printf("  %s = <unavailable>\r\n", parameter.name);
    return;
  }

  shell->Printf("  %s = %s", parameter.name, valueBuffer);
  if ((parameter.help != nullptr) && (parameter.help[0] != '\0')) {
    shell->Printf(" : %s", parameter.help);
  }
  if (parameter.setter == nullptr) {
    shell->Write(" [RO]");
  }
  shell->Write("\r\n");
}

// 把字节流写到底层 IO。
uint32_t Shell::WriteBytes(const uint8_t *data, uint32_t length)
{
  if ((io_ == nullptr) || (data == nullptr) || (length == 0U)) {
    return 0U;
  }

  uint32_t offset = 0U;
  uint32_t attempts = 0U;
  while ((offset < length) && (attempts < kMaxWriteAttempts)) {
    const uint32_t written = io_->Write(data + offset, length - offset);
    if (written == 0U) {
      ++attempts;
      continue;
    }

    offset += written;
    attempts = 0U;
  }

  return offset;
}

// 安全拷贝字符串到目标缓冲区。
void Shell::CopyString(const char *source, char *destination,
                       uint32_t destinationSize)
{
  if ((destination == nullptr) || (destinationSize == 0U)) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  uint32_t index = 0U;
  for (index = 0U; (index + 1U) < destinationSize; ++index) {
    destination[index] = source[index];
    if (source[index] == '\0') {
      return;
    }
  }

  destination[destinationSize - 1U] = '\0';
}

// 判断字符是否为空白符。
bool Shell::IsWhitespace(char character)
{
  return (character == ' ') || (character == '\t');
}

} // namespace iFly
