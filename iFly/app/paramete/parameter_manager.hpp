#ifndef IFLY_APP_PARAMETE_PARAMETER_MANAGER_HPP
#define IFLY_APP_PARAMETE_PARAMETER_MANAGER_HPP

#include <stdint.h>

#include "shell.hpp"

namespace iFly {

// 固定容量参数注册表：
// 1. 不做动态内存分配，适合 MCU 场景。
// 2. 参数值直接绑定到业务变量，读写路径短，延迟低。
// 3. 新增参数只需要补充一条描述，不需要重复写 getter/setter 胶水代码。
class ParameterManager final {
public:
  static constexpr uint8_t kMaxParameterCount = Shell::kMaxParameterCount;

  using ChangeHandler = void (*)(void *context);

  // 可读写 float 参数描述。
  struct FloatSpec final {
    const char *name = nullptr;
    const char *help = nullptr;
    float *value = nullptr;
    float min_value = 0.0f;
    float max_value = 0.0f;
    bool has_range = false;
    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  // 可读写 uint32 参数描述。
  struct U32Spec final {
    const char *name = nullptr;
    const char *help = nullptr;
    uint32_t *value = nullptr;
    uint32_t min_value = 0U;
    uint32_t max_value = 0U;
    bool has_range = false;
    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  // 可读写 bool 参数描述。
  struct BoolSpec final {
    const char *name = nullptr;
    const char *help = nullptr;
    bool *value = nullptr;
    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  // 自定义回调参数描述：
  // 用于只读参数，或者需要特殊读写逻辑的参数。
  struct CallbackSpec final {
    const char *name = nullptr;
    const char *help = nullptr;
    Shell::ParameterGetter getter = nullptr;
    Shell::ParameterSetter setter = nullptr;
    void *context = nullptr;
    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  void Clear();
  uint8_t Count() const {
    return count_;
  }

  bool AddFloat(const FloatSpec &spec);
  bool AddU32(const U32Spec &spec);
  bool AddBool(const BoolSpec &spec);
  bool AddCallback(const CallbackSpec &spec);

  bool RegisterToShell(Shell *shell);

private:
  // 参数最终统一落成一种内部表项，Shell 只感知一套 getter/setter。
  enum class EntryKind : uint8_t {
    kUnused = 0U,
    kFloat,
    kU32,
    kBool,
    kCallback,
  };

  struct Entry final {
    // 直接给 Shell 用的参数对象。
    Shell::Parameter parameter {};
    EntryKind kind = EntryKind::kUnused;

    // 指向真实业务变量的地址，避免中间拷贝。
    void *storage = nullptr;

    float float_min = 0.0f;
    float float_max = 0.0f;
    uint32_t u32_min = 0U;
    uint32_t u32_max = 0U;
    bool has_range = false;

    Shell::ParameterGetter custom_getter = nullptr;
    Shell::ParameterSetter custom_setter = nullptr;
    void *custom_context = nullptr;

    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  Entry *AllocateEntry();
  static Entry *AsEntry(void *context);

  static bool GetValue(void *context, char *buffer, uint32_t bufferSize);
  static bool SetValue(void *context, const char *value);

  static bool ParseFloat(const char *text, float *value);
  static bool ParseU32(const char *text, uint32_t *value);
  static bool ParseBool(const char *text, bool *value);
  static bool FormatFloat(char *buffer, uint32_t bufferSize, float value);
  static bool FormatU32(char *buffer, uint32_t bufferSize, uint32_t value);
  static bool FormatBool(char *buffer, uint32_t bufferSize, bool value);
  static void NotifyUpdated(Entry *entry);

private:
  // 固定大小数组，容量由 Shell 参数上限约束。
  Entry entries_[kMaxParameterCount] {};
  uint8_t count_ = 0U;
};

} // namespace iFly

#endif /* IFLY_APP_PARAMETE_PARAMETER_MANAGER_HPP */
