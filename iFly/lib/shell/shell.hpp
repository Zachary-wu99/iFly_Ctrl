#ifndef IFLY_SHELL_HPP
#define IFLY_SHELL_HPP

#include <stdint.h>

#include "serial_io_base.hpp"

namespace iFly {

class Shell final {
public:
  static constexpr uint8_t kMaxCommandCount = 12U;
  static constexpr uint8_t kMaxFunctionCount = 16U;
  static constexpr uint8_t kMaxParameterCount = 24U;
  static constexpr uint8_t kMaxArgumentCount = 10U;
  static constexpr uint16_t kInputLineBufferSize = 128U;
  static constexpr uint16_t kValueBufferSize = 96U;
  static constexpr uint8_t kMaxPasswordLength = 31U;

  using CommandHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc, const char *const *argv);
  using FunctionHandler =
      bool (*)(Shell *shell, void *context, uint8_t argc, const char *const *argv);
  using ParameterGetter = bool (*)(void *context, char *buffer, uint32_t bufferSize);
  using ParameterSetter = bool (*)(void *context, const char *value);
  using SessionAnimation = void (*)(Shell *shell, void *context);

  struct Command final {
    const char *name = nullptr;
    const char *help = nullptr;
    CommandHandler handler = nullptr;
    void *context = nullptr;
  };

  struct Function final {
    const char *name = nullptr;
    const char *help = nullptr;
    FunctionHandler handler = nullptr;
    void *context = nullptr;
  };

  struct Parameter final {
    const char *name = nullptr;
    const char *help = nullptr;
    ParameterGetter getter = nullptr;
    ParameterSetter setter = nullptr;
    void *context = nullptr;
  };

  Shell();

  void BindIo(SerialIoBase *io);
  SerialIoBase *BoundIo() const {
    return io_;
  }

  void SetBanner(const char *title, const char *subtitle);
  void SetPrompt(const char *prompt);
  void SetPassword(const char *password);
  void ClearPassword();
  void SetActivationKey(uint8_t key, const char *promptText);
  void DisableActivationKey();
  void SetSessionAnimation(SessionAnimation animation, void *context);

  bool RegisterCommand(const Command &command);
  bool RegisterFunction(const Function &function);
  bool RegisterParameter(const Parameter &parameter);
  void ClearRegistrations();

  void Poll();
  void ResetSession();

  bool IsLoggedIn() const;
  bool HasPassword() const;
  bool IsConnected() const;

  void Write(const char *text);
  void WriteLine(const char *text);
  void Printf(const char *format, ...);

private:
  enum class SessionState : uint8_t {
    kDisconnected = 0U,
    kActivationPrompt,
    kPasswordPrompt,
    kReady,
  };

  void StartConnectedSession();
  void ResetInputLine();
  void ProcessByte(uint8_t byteValue);
  void HandleActivationTrigger();
  void HandleCompletedLine();
  void HandlePasswordLine();
  void HandleCommandLine();
  uint8_t Tokenize(char *line, const char *argv[]) const;

  bool ExecuteBuiltin(uint8_t argc, const char *const *argv);
  bool ExecuteCommand(uint8_t argc, const char *const *argv);

  bool HandleHelpCommand(uint8_t argc, const char *const *argv);
  bool HandleLogoutCommand(uint8_t argc, const char *const *argv);
  bool HandlePasswordCommand(uint8_t argc, const char *const *argv);
  bool HandleClearCommand(uint8_t argc, const char *const *argv);
  bool HandleFunctionCommand(uint8_t argc, const char *const *argv);
  bool HandleFunctionCallAlias(uint8_t argc, const char *const *argv);
  bool HandleParameterCommand(uint8_t argc, const char *const *argv);
  bool HandleParameterGetAlias(uint8_t argc, const char *const *argv);
  bool HandleParameterSetAlias(uint8_t argc, const char *const *argv);
  bool HandleParameterListAlias(uint8_t argc, const char *const *argv);

  const Command *FindCommand(const char *name) const;
  const Function *FindFunction(const char *name) const;
  const Parameter *FindParameter(const char *name) const;

  void PrintBanner();
  void PrintPrompt();
  void PrintPasswordPrompt();
  void PrintHelpSummary() const;
  void PrintCommandList() const;
  void PrintFunctionList() const;
  void PrintParameterList() const;
  void PrintParameterValue(const Parameter &parameter) const;

  uint32_t WriteBytes(const uint8_t *data, uint32_t length);
  static void CopyString(const char *source, char *destination,
                         uint32_t destinationSize);
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
