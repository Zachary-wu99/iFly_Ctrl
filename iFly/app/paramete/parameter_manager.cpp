#include "parameter_manager.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace iFly {

void ParameterManager::Clear()
{
  // 直接整体复位内部表项，初始化成本可控且实现简单。
  uint8_t index = 0U;
  for (index = 0U; index < kMaxParameterCount; ++index) {
    entries_[index] = Entry {};
  }
  count_ = 0U;
}

bool ParameterManager::AddFloat(const FloatSpec &spec)
{
  if ((spec.name == nullptr) || (spec.value == nullptr)) {
    return false;
  }

  Entry *entry = AllocateEntry();
  if (entry == nullptr) {
    return false;
  }

  entry->parameter.name = spec.name;
  entry->parameter.help = spec.help;
  // Shell 侧始终走统一入口，内部再按 kind 分发。
  entry->parameter.getter = &ParameterManager::GetValue;
  entry->parameter.setter = &ParameterManager::SetValue;
  entry->parameter.context = entry;
  entry->kind = EntryKind::kFloat;
  // 直接保存业务变量地址，避免包装对象层层转发。
  entry->storage = spec.value;
  entry->float_min = spec.min_value;
  entry->float_max = spec.max_value;
  entry->has_range = spec.has_range;
  if (entry->has_range && (entry->float_min > entry->float_max)) {
    const float temp = entry->float_min;
    entry->float_min = entry->float_max;
    entry->float_max = temp;
  }
  entry->on_updated = spec.on_updated;
  entry->on_updated_context = spec.on_updated_context;
  return true;
}

bool ParameterManager::AddU32(const U32Spec &spec)
{
  if ((spec.name == nullptr) || (spec.value == nullptr)) {
    return false;
  }

  Entry *entry = AllocateEntry();
  if (entry == nullptr) {
    return false;
  }

  entry->parameter.name = spec.name;
  entry->parameter.help = spec.help;
  entry->parameter.getter = &ParameterManager::GetValue;
  entry->parameter.setter = &ParameterManager::SetValue;
  entry->parameter.context = entry;
  entry->kind = EntryKind::kU32;
  entry->storage = spec.value;
  entry->u32_min = spec.min_value;
  entry->u32_max = spec.max_value;
  entry->has_range = spec.has_range;
  if (entry->has_range && (entry->u32_min > entry->u32_max)) {
    const uint32_t temp = entry->u32_min;
    entry->u32_min = entry->u32_max;
    entry->u32_max = temp;
  }
  entry->on_updated = spec.on_updated;
  entry->on_updated_context = spec.on_updated_context;
  return true;
}

bool ParameterManager::AddBool(const BoolSpec &spec)
{
  if ((spec.name == nullptr) || (spec.value == nullptr)) {
    return false;
  }

  Entry *entry = AllocateEntry();
  if (entry == nullptr) {
    return false;
  }

  entry->parameter.name = spec.name;
  entry->parameter.help = spec.help;
  entry->parameter.getter = &ParameterManager::GetValue;
  entry->parameter.setter = &ParameterManager::SetValue;
  entry->parameter.context = entry;
  entry->kind = EntryKind::kBool;
  entry->storage = spec.value;
  entry->on_updated = spec.on_updated;
  entry->on_updated_context = spec.on_updated_context;
  return true;
}

bool ParameterManager::AddCallback(const CallbackSpec &spec)
{
  if ((spec.name == nullptr) || (spec.getter == nullptr)) {
    return false;
  }

  Entry *entry = AllocateEntry();
  if (entry == nullptr) {
    return false;
  }

  entry->parameter.name = spec.name;
  entry->parameter.help = spec.help;
  entry->parameter.getter = &ParameterManager::GetValue;
  entry->parameter.setter = (spec.setter != nullptr) ? &ParameterManager::SetValue : nullptr;
  entry->parameter.context = entry;
  entry->kind = EntryKind::kCallback;
  // 自定义参数保留外部 getter/setter，实现特殊逻辑复用。
  entry->custom_getter = spec.getter;
  entry->custom_setter = spec.setter;
  entry->custom_context = spec.context;
  entry->on_updated = spec.on_updated;
  entry->on_updated_context = spec.on_updated_context;
  return true;
}

bool ParameterManager::RegisterToShell(Shell *shell)
{
  if (shell == nullptr) {
    return false;
  }

  // 启动阶段一次性注册，运行时不再做查表构建。
  bool success = true;
  uint8_t index = 0U;
  for (index = 0U; index < count_; ++index) {
    if (!shell->RegisterParameter(entries_[index].parameter)) {
      success = false;
    }
  }

  return success;
}

ParameterManager::Entry *ParameterManager::AllocateEntry()
{
  if (count_ >= kMaxParameterCount) {
    return nullptr;
  }

  // 顺序分配数组槽位，没有堆分配和链表操作。
  Entry *entry = &entries_[count_];
  *entry = Entry {};
  ++count_;
  return entry;
}

ParameterManager::Entry *ParameterManager::AsEntry(void *context)
{
  return reinterpret_cast<Entry *>(context);
}

bool ParameterManager::GetValue(void *context, char *buffer, uint32_t bufferSize)
{
  Entry *entry = AsEntry(context);
  if ((entry == nullptr) || (buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  // 统一 getter：外部只有一套接口，内部按参数类型走最短路径。
  switch (entry->kind) {
    case EntryKind::kFloat:
      return FormatFloat(buffer, bufferSize,
                         *reinterpret_cast<float *>(entry->storage));

    case EntryKind::kU32:
      return FormatU32(buffer, bufferSize,
                       *reinterpret_cast<uint32_t *>(entry->storage));

    case EntryKind::kBool:
      return FormatBool(buffer, bufferSize,
                        *reinterpret_cast<bool *>(entry->storage));

    case EntryKind::kCallback:
      return (entry->custom_getter != nullptr)
                 ? entry->custom_getter(entry->custom_context, buffer, bufferSize)
                 : false;

    default:
      return false;
  }
}

bool ParameterManager::SetValue(void *context, const char *value)
{
  Entry *entry = AsEntry(context);
  if ((entry == nullptr) || (value == nullptr)) {
    return false;
  }

  // 统一 setter：解析、范围校验、写回和更新回调都在这里收口。
  switch (entry->kind) {
    case EntryKind::kFloat: {
      float parsed = 0.0f;
      if (!ParseFloat(value, &parsed)) {
        return false;
      }

      if (entry->has_range &&
          ((parsed < entry->float_min) || (parsed > entry->float_max))) {
        return false;
      }

      // 直接写回绑定变量，不引入中间缓存。
      *reinterpret_cast<float *>(entry->storage) = parsed;
      NotifyUpdated(entry);
      return true;
    }

    case EntryKind::kU32: {
      uint32_t parsed = 0U;
      if (!ParseU32(value, &parsed)) {
        return false;
      }

      if (entry->has_range &&
          ((parsed < entry->u32_min) || (parsed > entry->u32_max))) {
        return false;
      }

      *reinterpret_cast<uint32_t *>(entry->storage) = parsed;
      NotifyUpdated(entry);
      return true;
    }

    case EntryKind::kBool: {
      bool parsed = false;
      if (!ParseBool(value, &parsed)) {
        return false;
      }

      *reinterpret_cast<bool *>(entry->storage) = parsed;
      NotifyUpdated(entry);
      return true;
    }

    case EntryKind::kCallback: {
      if (entry->custom_setter == nullptr) {
        return false;
      }

      // 特殊参数把写入逻辑回交给业务层。
      if (!entry->custom_setter(entry->custom_context, value)) {
        return false;
      }

      NotifyUpdated(entry);
      return true;
    }

    default:
      return false;
  }
}

bool ParameterManager::ParseFloat(const char *text, float *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  // 统一做尾部字符校验，避免 "1.2abc" 之类脏输入混进来。
  char *end = nullptr;
  const float parsed = strtof(text, &end);
  if ((end == text) || (end == nullptr)) {
    return false;
  }

  while ((*end == ' ') || (*end == '\t')) {
    ++end;
  }

  if ((*end != '\0') || (isfinite(parsed) == 0)) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParameterManager::ParseU32(const char *text, uint32_t *value)
{
  if ((text == nullptr) || (value == nullptr) || (text[0] == '-')) {
    return false;
  }

  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if ((end == text) || (end == nullptr)) {
    return false;
  }

  while ((*end == ' ') || (*end == '\t')) {
    ++end;
  }

  if ((*end != '\0') || (parsed > 0xFFFFFFFFUL)) {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool ParameterManager::ParseBool(const char *text, bool *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  // 兼容 CLI 常见布尔写法，减少交互摩擦。
  if ((strcmp(text, "1") == 0) || (strcmp(text, "true") == 0) ||
      (strcmp(text, "on") == 0) || (strcmp(text, "yes") == 0) ||
      (strcmp(text, "lock") == 0)) {
    *value = true;
    return true;
  }

  if ((strcmp(text, "0") == 0) || (strcmp(text, "false") == 0) ||
      (strcmp(text, "off") == 0) || (strcmp(text, "no") == 0) ||
      (strcmp(text, "unlock") == 0)) {
    *value = false;
    return true;
  }

  return false;
}

bool ParameterManager::FormatFloat(char *buffer, uint32_t bufferSize, float value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%.6g",
                               static_cast<double>(value));
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool ParameterManager::FormatU32(char *buffer, uint32_t bufferSize,
                                 uint32_t value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%lu",
                               static_cast<unsigned long>(value));
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool ParameterManager::FormatBool(char *buffer, uint32_t bufferSize, bool value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%s",
                               value ? "true" : "false");
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

void ParameterManager::NotifyUpdated(Entry *entry)
{
  if ((entry == nullptr) || (entry->on_updated == nullptr)) {
    return;
  }

  // 参数值真正写回后再触发业务同步，例如重配 PID。
  entry->on_updated(entry->on_updated_context);
}

} // namespace iFly
