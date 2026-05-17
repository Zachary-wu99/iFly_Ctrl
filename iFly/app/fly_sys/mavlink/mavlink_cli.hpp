/**
 * @file mavlink_cli.hpp
 * @brief MAVLink CLI 接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CLI_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CLI_HPP

#include <stdint.h>

#include "mavlink_parameter_service.hpp"
#include "shell.hpp"

namespace iFly {

/**
 * @brief MAVLink 命令行控制台。
 */
class MavlinkCliService final {
public:
  /**
   * @brief 构造 MAVLink CLI 对象。
   */
  MavlinkCliService();

  /**
   * @brief 初始化 CLI、参数入口和功能入口。
   */
  void Init();

  /**
   * @brief 设置 CLI 输出回调。
   *
   * @param output 输出回调函数。
   * @param context 输出回调上下文。
   */
  void SetOutput(Shell::OutputHandler output, void *context);

  /**
   * @brief 设置 CLI 链路连接状态。
   *
   * @param connected 新的连接状态。
   */
  void SetConnected(bool connected);

  /**
   * @brief 推入 CLI 输入字节。
   *
   * @param data 输入字节。
   * @param length 输入长度。
   */
  void ProcessInput(const uint8_t *data, uint32_t length);

  /**
   * @brief 获取可写 Shell 控制台。
   *
   * @return Shell 对象引用。
   */
  Shell &Console() {
    return shell_;
  }

  /**
   * @brief 获取只读 Shell 控制台。
   *
   * @return 只读 Shell 对象引用。
   */
  const Shell &Console() const {
    return shell_;
  }

private:
  static constexpr uint8_t kManagedParameterCount = 64U; /**< 受管参数数量。 */

  /**
   * @brief 受管参数运行时上下文。
   */
  struct ManagedParameterContext final {
    MavlinkCliService *owner = nullptr; /**< 所属 CLI 对象。 */
    uint16_t mavlink_index = 0U; /**< MAVLink 参数索引。 */
  };

  /**
   * @brief 注册所有参数入口。
   */
  void RegisterParameters();

  /**
   * @brief 注册所有功能入口。
   */
  void RegisterFunctions();

  /**
   * @brief 根据当前状态刷新 Shell 横幅。
   */
  void UpdateShellBanner();

  /**
   * @brief 获取当前传输通道参数值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetTransportParameter(void *context, char *buffer,
                                    uint32_t bufferSize);

  /**
   * @brief 获取系统运行时间参数值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetUptimeParameter(void *context, char *buffer,
                                 uint32_t bufferSize);

  /**
   * @brief 获取受管参数的当前值。
   *
   * @param context 回调上下文。
   * @param buffer 输出缓冲区。
   * @param bufferSize 输出缓冲区大小。
   * @return 获取成功返回 `true`。
   */
  static bool GetManagedParameter(void *context, char *buffer,
                                  uint32_t bufferSize);

  /**
   * @brief 设置受管参数的新值。
   *
   * @param context 回调上下文。
   * @param value 输入文本值。
   * @return 设置成功返回 `true`。
   */
  static bool SetManagedParameter(void *context, const char *value);

  /**
   * @brief `status` 功能实现。
   */
  static bool StatusFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);

  /**
   * @brief `reboot` 功能实现。
   */
  static bool RebootFunction(Shell *shell, void *context, uint8_t argc,
                             const char *const *argv);

  MavlinkParameterService parameter_service_ {}; /**< MAVLink 参数适配服务。 */
  Shell shell_ {}; /**< 命令行 Shell 实例。 */
  ManagedParameterContext managed_parameter_contexts_[kManagedParameterCount] {}; /**< 受管参数上下文表。 */

  const char *active_transport_name_ = "mavlink"; /**< 当前激活的传输通道名称。 */

  char banner_subtitle_[64] {}; /**< Shell 横幅副标题缓冲区。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CLI_HPP */

