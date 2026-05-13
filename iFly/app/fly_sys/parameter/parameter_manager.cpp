#include "parameter_manager.hpp"

namespace iFly {

namespace {

bool IsNameValid(const char *name) {
  return (name != nullptr) && (name[0] != '\0');
}

} // namespace

ParameterManager &ParameterManager::Instance() {
  static ParameterManager instance;
  return instance;
}

ParameterManager::ParameterManager()
    : data_(MakeDefaultSysParameters()) {
  // 启动时按默认参数表注册全部绑定项。
  (void)BuildDefaultRegistry();
}

void ParameterManager::ResetToDefaults() {
  data_ = MakeDefaultSysParameters();
}

bool ParameterManager::Contains(const char *name) const {
  return FindEntry(name) != nullptr;
}

const ParameterManager::EntryView *ParameterManager::Find(const char *name) const {
  const Entry *entry = FindEntry(name);
  return (entry != nullptr) ? &entry->view : nullptr;
}

const ParameterManager::EntryView *ParameterManager::At(uint16_t index) const {
  return (index < count_) ? &entries_[index].view : nullptr;
}

int16_t ParameterManager::IndexOf(const char *name) const {
  if (!IsNameValid(name)) {
    return -1;
  }

  for (uint16_t index = 0U; index < count_; ++index) {
    if ((entries_[index].view.name != nullptr) &&
        (strcmp(entries_[index].view.name, name) == 0)) {
      return static_cast<int16_t>(index);
    }
  }

  return -1;
}

uint32_t ParameterManager::SizeOf(const char *name) const {
  const Entry *entry = FindEntry(name);
  return (entry != nullptr) ? entry->view.size : 0U;
}

bool ParameterManager::ReadRaw(const char *name, void *buffer, uint32_t bufferSize) const {
  const Entry *entry = FindEntry(name);
  if ((entry == nullptr) || (buffer == nullptr) || (bufferSize < entry->view.size) ||
      (entry->view.storage == nullptr)) {
    return false;
  }

  (void)memcpy(buffer, entry->view.storage, entry->view.size);
  return true;
}

bool ParameterManager::WriteRaw(const char *name, const void *data, uint32_t dataSize) {
  Entry *entry = FindEntry(name);
  if ((entry == nullptr) || (data == nullptr) || (entry->view.storage == nullptr) ||
      (entry->view.access != AccessMode::kReadWrite) ||
      (dataSize != entry->view.size)) {
    return false;
  }

  (void)memcpy(const_cast<void *>(entry->view.storage), data, entry->view.size);
  NotifyUpdated(*entry);
  return true;
}

bool ParameterManager::SetChangeHandler(const char *name,
                                               ChangeHandler handler,
                                               void *context) {
  Entry *entry = FindEntry(name);
  if (entry == nullptr) {
    return false;
  }

  entry->on_updated = handler;
  entry->on_updated_context = context;
  return true;
}

bool ParameterManager::ClearChangeHandler(const char *name) {
  return SetChangeHandler(name, nullptr, nullptr);
}

bool ParameterManager::BuildDefaultRegistry() {
  uint16_t binding_count = 0U;
  const ParameterBinding *bindings = GetSysParameterBindings(&binding_count);
  if ((bindings == nullptr) || (binding_count == 0U)) {
    return false;
  }

  count_ = 0U;
  for (uint16_t index = 0U; index < kMaxEntryCount; ++index) {
    // 清空旧表项，恢复到默认空状态。
    entries_[index] = Entry {};
  }

  bool success = true;
  for (uint16_t index = 0U; index < binding_count; ++index) {
    if (!Register(bindings[index])) {
      success = false;
    }
  }

  return success;
}

bool ParameterManager::Register(const ParameterBinding &binding) {
  if (!IsNameValid(binding.name) || (binding.size == 0U) || (count_ >= kMaxEntryCount)) {
    return false;
  }

  if (Contains(binding.name)) {
    return false;
  }

  if ((binding.offset + binding.size) > sizeof(SysParameters)) {
    return false;
  }

  Entry &entry = entries_[count_];
  entry = Entry {};
  entry.view.name = binding.name;
  entry.view.help = binding.help;
  entry.view.storage = reinterpret_cast<const uint8_t *>(&data_) + binding.offset;
  entry.view.size = binding.size;
  entry.view.type = binding.type;
  entry.view.access = binding.read_only ? AccessMode::kReadOnly : AccessMode::kReadWrite;

  ++count_;
  return true;
}

ParameterManager::Entry *ParameterManager::FindEntry(const char *name) {
  if (!IsNameValid(name)) {
    return nullptr;
  }

  for (uint16_t index = 0U; index < count_; ++index) {
    if ((entries_[index].view.name != nullptr) &&
        (strcmp(entries_[index].view.name, name) == 0)) {
      return &entries_[index];
    }
  }

  return nullptr;
}

const ParameterManager::Entry *ParameterManager::FindEntry(const char *name) const {
  if (!IsNameValid(name)) {
    return nullptr;
  }

  for (uint16_t index = 0U; index < count_; ++index) {
    if ((entries_[index].view.name != nullptr) &&
        (strcmp(entries_[index].view.name, name) == 0)) {
      return &entries_[index];
    }
  }

  return nullptr;
}

void ParameterManager::NotifyUpdated(const Entry &entry) {
  if (entry.on_updated == nullptr) {
    return;
  }

  entry.on_updated(entry.view.name, entry.on_updated_context);
}

} // namespace iFly

