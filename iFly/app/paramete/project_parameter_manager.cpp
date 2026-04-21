#include "project_parameter_manager.hpp"

namespace iFly {

namespace {

bool IsNameValid(const char *name) {
  return (name != nullptr) && (name[0] != '\0');
}

} // namespace

ProjectParameterManager &ProjectParameterManager::Instance() {
  static ProjectParameterManager instance;
  return instance;
}

ProjectParameterManager::ProjectParameterManager()
    : data_(MakeDefaultProjectParameters()) {
  (void)BuildDefaultRegistry();
}

void ProjectParameterManager::ResetToDefaults() {
  data_ = MakeDefaultProjectParameters();
}

bool ProjectParameterManager::Contains(const char *name) const {
  return FindEntry(name) != nullptr;
}

const ProjectParameterManager::EntryView *ProjectParameterManager::Find(const char *name) const {
  const Entry *entry = FindEntry(name);
  return (entry != nullptr) ? &entry->view : nullptr;
}

uint32_t ProjectParameterManager::SizeOf(const char *name) const {
  const Entry *entry = FindEntry(name);
  return (entry != nullptr) ? entry->view.size : 0U;
}

bool ProjectParameterManager::ReadRaw(const char *name, void *buffer, uint32_t bufferSize) const {
  const Entry *entry = FindEntry(name);
  if ((entry == nullptr) || (buffer == nullptr) || (bufferSize < entry->view.size) ||
      (entry->view.storage == nullptr)) {
    return false;
  }

  (void)memcpy(buffer, entry->view.storage, entry->view.size);
  return true;
}

bool ProjectParameterManager::WriteRaw(const char *name, const void *data, uint32_t dataSize) {
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

bool ProjectParameterManager::SetChangeHandler(const char *name,
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

bool ProjectParameterManager::ClearChangeHandler(const char *name) {
  return SetChangeHandler(name, nullptr, nullptr);
}

bool ProjectParameterManager::BuildDefaultRegistry() {
  uint16_t binding_count = 0U;
  const ProjectParameterBinding *bindings = GetProjectParameterBindings(&binding_count);
  if ((bindings == nullptr) || (binding_count == 0U)) {
    return false;
  }

  count_ = 0U;
  for (uint16_t index = 0U; index < kMaxEntryCount; ++index) {
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

bool ProjectParameterManager::Register(const ProjectParameterBinding &binding) {
  if (!IsNameValid(binding.name) || (binding.size == 0U) || (count_ >= kMaxEntryCount)) {
    return false;
  }

  if (Contains(binding.name)) {
    return false;
  }

  if ((binding.offset + binding.size) > sizeof(ProjectParameters)) {
    return false;
  }

  Entry &entry = entries_[count_];
  entry = Entry {};
  entry.view.name = binding.name;
  entry.view.help = binding.help;
  entry.view.storage = reinterpret_cast<const uint8_t *>(&data_) + binding.offset;
  entry.view.size = binding.size;
  entry.view.access = binding.read_only ? AccessMode::kReadOnly : AccessMode::kReadWrite;

  ++count_;
  return true;
}

ProjectParameterManager::Entry *ProjectParameterManager::FindEntry(const char *name) {
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

const ProjectParameterManager::Entry *ProjectParameterManager::FindEntry(const char *name) const {
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

void ProjectParameterManager::NotifyUpdated(const Entry &entry) {
  if (entry.on_updated == nullptr) {
    return;
  }

  entry.on_updated(entry.view.name, entry.on_updated_context);
}

} // namespace iFly
