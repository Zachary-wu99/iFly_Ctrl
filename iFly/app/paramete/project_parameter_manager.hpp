/**
 * @file project_parameter_manager.hpp
 * @brief 工程参数中心接口。
 */
#ifndef IFLY_APP_PARAMETE_PROJECT_PARAMETER_MANAGER_HPP
#define IFLY_APP_PARAMETE_PROJECT_PARAMETER_MANAGER_HPP

#include <stdint.h>
#include <string.h>
#include <type_traits>

#include "project_parameters.hpp"

namespace iFly {

/**
 * @brief 工程级参数中心。
 *
 * @details
 * 该类统一持有整棵工程参数树，并提供基于名字的读写、查询与变更通知能力。
 */
class ProjectParameterManager final {
public:
  static constexpr uint16_t kMaxEntryCount = 160U; /**< 最大参数表项数量。 */

  /**
   * @brief 参数访问权限。
   */
  enum class AccessMode : uint8_t {
    kReadOnly = 0U, /**< 只允许读取。 */
    kReadWrite = 1U /**< 允许读写。 */
  };

  using ChangeHandler = void (*)(const char *name, void *context); /**< 参数变更回调签名。 */

  /**
   * @brief 对外暴露的只读参数项视图。
   */
  struct EntryView final {
    const char *name = nullptr; /**< 参数名。 */
    const char *help = nullptr; /**< 参数帮助文本。 */
    const void *storage = nullptr; /**< 参数实际存储地址。 */
    uint32_t size = 0U; /**< 参数占用的字节数。 */
    ProjectParameterType type = ProjectParameterType::kBytes; /**< MAVLink 参数类型。 */
    AccessMode access = AccessMode::kReadWrite; /**< 当前参数访问权限。 */
    bool mavlink_visible = false; /**< 是否通过 MAVLink 参数协议暴露。 */
  };

  /**
   * @brief 获取全局唯一参数中心实例。
   *
   * @return 单例引用。
   */
  static ProjectParameterManager &Instance();

  /**
   * @brief 将参数树恢复为默认值。
   */
  void ResetToDefaults();

  /**
   * @brief 获取参数树的只读引用。
   *
   * @return 只读参数树引用。
   */
  const ProjectParameters &Data() const {
    return data_;
  }

  /**
   * @brief 获取参数树的可写引用。
   *
   * @return 可写参数树引用。
   */
  ProjectParameters &MutableData() {
    return data_;
  }

  /**
   * @brief 获取当前已注册参数数量。
   *
   * @return 参数表项数量。
   */
  uint16_t Count() const {
    return count_;
  }

  /**
   * @brief 获取指定索引的参数视图。
   *
   * @param index 参数表索引。
   * @return 参数视图地址，索引越界返回 `nullptr`。
   */
  const EntryView *At(uint16_t index) const;

  /**
   * @brief 获取 MAVLink 可见参数数量。
   *
   * @return MAVLink 参数表项数量。
   */
  uint16_t MavlinkCount() const;

  /**
   * @brief 获取指定 MAVLink 索引的参数视图。
   *
   * @param index MAVLink 参数索引。
   * @return 参数视图地址，索引越界返回 `nullptr`。
   */
  const EntryView *MavlinkAt(uint16_t index) const;

  /**
   * @brief 查找 MAVLink 参数索引。
   *
   * @param name 参数名。
   * @return 参数索引，未找到返回 `-1`。
   */
  int16_t MavlinkIndexOf(const char *name) const;

  /**
   * @brief 判断参数名是否存在。
   *
   * @param name 参数名。
   * @return 存在返回 `true`。
   */
  bool Contains(const char *name) const;

  /**
   * @brief 查找参数的只读视图。
   *
   * @param name 参数名。
   * @return 找到时返回视图地址，否则返回 `nullptr`。
   */
  const EntryView *Find(const char *name) const;

  /**
   * @brief 查询参数的存储大小。
   *
   * @param name 参数名。
   * @return 参数大小，未找到时返回 `0`。
   */
  uint32_t SizeOf(const char *name) const;

  /**
   * @brief 读取参数的原始二进制内容。
   *
   * @param name 参数名。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 读取成功返回 `true`。
   */
  bool ReadRaw(const char *name, void *buffer, uint32_t bufferSize) const;

  /**
   * @brief 写入参数的原始二进制内容。
   *
   * @param name 参数名。
   * @param data 输入数据首地址。
   * @param dataSize 输入数据大小。
   * @return 写入成功返回 `true`。
   */
  bool WriteRaw(const char *name, const void *data, uint32_t dataSize);

  /**
   * @brief 写入 MAVLink 参数值。
   *
   * @param name 参数名。
   * @param value MAVLink 浮点参数值。
   * @param type MAVLink 参数类型。
   * @return 写入成功返回 `true`。
   */
  bool WriteMavlinkValue(const char *name,
                         float value,
                         ProjectParameterType type);

  /**
   * @brief 为指定参数绑定更新回调。
   *
   * @param name 参数名。
   * @param handler 变更回调函数。
   * @param context 回调上下文指针。
   * @return 绑定成功返回 `true`。
   */
  bool SetChangeHandler(const char *name, ChangeHandler handler, void *context);

  /**
   * @brief 清除指定参数的更新回调。
   *
   * @param name 参数名。
   * @return 清除成功返回 `true`。
   */
  bool ClearChangeHandler(const char *name);

  /**
   * @brief 按指定类型读取参数。
   *
   * @tparam T 可平凡拷贝类型。
   * @param name 参数名。
   * @param value 输出对象地址。
   * @return 读取成功返回 `true`。
   */
  template <typename T>
  bool Read(const char *name, T *value) const {
    static_assert(std::is_trivially_copyable<T>::value,
                  "ProjectParameterManager::Read only supports trivially copyable types.");
    return ReadRaw(name, value, sizeof(T));
  }

  /**
   * @brief 按指定类型写入参数。
   *
   * @tparam T 可平凡拷贝类型。
   * @param name 参数名。
   * @param value 输入对象引用。
   * @return 写入成功返回 `true`。
   */
  template <typename T>
  bool Write(const char *name, const T &value) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "ProjectParameterManager::Write only supports trivially copyable types.");
    return WriteRaw(name, &value, sizeof(T));
  }

private:
  /**
   * @brief 内部参数表项。
   */
  struct Entry final {
    EntryView view {}; /**< 对外暴露的只读视图。 */
    ChangeHandler on_updated = nullptr; /**< 参数更新后的回调函数。 */
    void *on_updated_context = nullptr; /**< 回调函数的上下文指针。 */
  };

  ProjectParameterManager();

  /**
   * @brief 构建默认参数绑定表。
   *
   * @return 构建成功返回 `true`。
   */
  bool BuildDefaultRegistry();

  /**
   * @brief 注册单个参数绑定。
   *
   * @param binding 参数绑定描述。
   * @return 注册成功返回 `true`。
   */
  bool Register(const ProjectParameterBinding &binding);

  /**
   * @brief 查找可写参数表项。
   *
   * @param name 参数名。
   * @return 找到时返回表项地址，否则返回 `nullptr`。
   */
  Entry *FindEntry(const char *name);

  /**
   * @brief 查找只读参数表项。
   *
   * @param name 参数名。
   * @return 找到时返回表项地址，否则返回 `nullptr`。
   */
  const Entry *FindEntry(const char *name) const;

  /**
   * @brief 触发参数更新通知。
   *
   * @param entry 已更新的参数表项。
   */
  void NotifyUpdated(const Entry &entry);

  ProjectParameters data_ {}; /**< 当前持有的工程参数树。 */
  Entry entries_[kMaxEntryCount] {}; /**< 参数注册表。 */
  uint16_t count_ = 0U; /**< 当前已注册参数数量。 */
};

} // namespace iFly

#endif /* IFLY_APP_PARAMETE_PROJECT_PARAMETER_MANAGER_HPP */
