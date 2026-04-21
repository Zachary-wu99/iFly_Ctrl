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
 * 这个模块和现有只服务 Shell/CLI 的 `ParameterManager` 不同，
 * 它的职责是：
 * - 统一持有整棵工程参数树
 * - 为各模块提供集中读取、修改接口
 * - 通过参数名建立“名字 -> 内存块”的静态映射
 * - 支持基础类型、枚举、结构体、定长数组等可平凡拷贝类型
 *
 * 设计目标：
 * - 不依赖动态内存，适合 MCU 工程
 * - 尽量减少全局变量散落
 * - 模块既可以整组读取参数，也可以按名字读写单个参数
 */
class ProjectParameterManager final {
public:
  static constexpr uint16_t kMaxEntryCount = 64U;

  /** @brief 参数访问权限。*/
  enum class AccessMode : uint8_t {
    kReadOnly = 0U,
    kReadWrite = 1U
  };

  /** @brief 参数更新后的通知回调签名。*/
  using ChangeHandler = void (*)(const char *name, void *context);

  /**
   * @brief 对外暴露的只读参数表项视图。
   *
   * @details
   * 这里不暴露内部管理字段，只返回查询和调试真正需要的信息。
   */
  struct EntryView final {
    const char *name = nullptr;
    const char *help = nullptr;
    const void *storage = nullptr;
    uint32_t size = 0U;
    AccessMode access = AccessMode::kReadWrite;
  };

  /** @brief 获取全局唯一的工程参数中心。*/
  static ProjectParameterManager &Instance();

  /**
   * @brief 恢复整棵参数树到默认值。
   *
   * @details
   * 这里只重置参数值本身，不重建绑定表。
   * 绑定表在构造阶段初始化一次即可。
   */
  void ResetToDefaults();

  /** @brief 返回整棵工程参数树的只读引用。*/
  const ProjectParameters &Data() const {
    return data_;
  }

  /**
   * @brief 返回整棵工程参数树的可写引用。
   *
   * @details
   * 这个接口适合初始化阶段或高频路径直接使用。
   * 如果业务需要统一的“参数更新通知”，优先使用 `Write()` / `WriteRaw()`。
   */
  ProjectParameters &MutableData() {
    return data_;
  }

  /** @brief 返回当前已注册的参数表项数量。*/
  uint16_t Count() const {
    return count_;
  }

  /** @brief 判断某个参数名是否存在。*/
  bool Contains(const char *name) const;
  /** @brief 查找某个参数的只读描述信息。*/
  const EntryView *Find(const char *name) const;
  /** @brief 查询参数的存储大小，未找到时返回 0。*/
  uint32_t SizeOf(const char *name) const;

  /**
   * @brief 读取指定参数的原始二进制内容。
   *
   * @param name       参数名
   * @param buffer     调用方提供的输出缓冲区
   * @param bufferSize 输出缓冲区大小，必须不小于参数真实大小
   */
  bool ReadRaw(const char *name, void *buffer, uint32_t bufferSize) const;

  /**
   * @brief 写入指定参数的原始二进制内容。
   *
   * @details
   * 为了避免部分写入导致对象内容损坏，这里要求 `dataSize`
   * 必须与参数真实大小完全一致。
   */
  bool WriteRaw(const char *name, const void *data, uint32_t dataSize);

  /** @brief 为某个参数绑定更新回调。*/
  bool SetChangeHandler(const char *name, ChangeHandler handler, void *context);
  /** @brief 清除某个参数绑定的更新回调。*/
  bool ClearChangeHandler(const char *name);

  /**
   * @brief 读取指定类型的参数。
   *
   * @tparam T 可平凡拷贝类型，例如基础类型、枚举、结构体、定长数组封装结构等。
   */
  template <typename T>
  bool Read(const char *name, T *value) const {
    static_assert(std::is_trivially_copyable<T>::value,
                  "ProjectParameterManager::Read only supports trivially copyable types.");
    return ReadRaw(name, value, sizeof(T));
  }

  /**
   * @brief 写入指定类型的参数。
   *
   * @tparam T 可平凡拷贝类型，例如基础类型、枚举、结构体、定长数组封装结构等。
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
   *
   * @details
   * 在只读视图基础上，额外保存更新回调信息，
   * 便于参数变更后做同步处理。
   */
  struct Entry final {
    EntryView view {};
    ChangeHandler on_updated = nullptr;
    void *on_updated_context = nullptr;
  };

  ProjectParameterManager();

  bool BuildDefaultRegistry();
  bool Register(const ProjectParameterBinding &binding);
  Entry *FindEntry(const char *name);
  const Entry *FindEntry(const char *name) const;
  void NotifyUpdated(const Entry &entry);

private:
  ProjectParameters data_ {};
  Entry entries_[kMaxEntryCount] {};
  uint16_t count_ = 0U;
};

} // namespace iFly

#endif /* IFLY_APP_PARAMETE_PROJECT_PARAMETER_MANAGER_HPP */
