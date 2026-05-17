/**
 * @file mavlink_parameter_service.hpp
 * @brief MAVLink 参数适配服务接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_PARAMETER_SERVICE_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_PARAMETER_SERVICE_HPP

#include <stdint.h>

namespace iFly {

class ParameterManager;

/**
 * @brief MAVLink 参数值快照。
 */
struct MavlinkParameterValue final {
  const char *name = nullptr; /**< MAVLink 参数名。 */
  float value = 0.0f; /**< MAVLink 浮点参数值。 */
  uint8_t type = 0U; /**< MAVLink 参数类型。 */
  uint16_t count = 0U; /**< MAVLink 参数总数。 */
  uint16_t index = 0U; /**< 当前 MAVLink 参数索引。 */
};

/**
 * @brief MAVLink 参数协议到系统参数中心的适配服务。
 *
 * @details MAVLink 参数名会先转换为系统内部参数名，再访问参数中心。
 */
class MavlinkParameterService final {
public:
  /**
   * @brief 构造 MAVLink 参数适配服务对象。
   *
   * @param parameters 系统参数中心，传入 `nullptr` 时使用全局实例。
   */
  explicit MavlinkParameterService(ParameterManager *parameters = nullptr);

  /**
   * @brief 绑定系统参数中心。
   *
   * @param parameters 系统参数中心，传入 `nullptr` 时使用全局实例。
   */
  void BindParameterManager(ParameterManager *parameters);

  /**
   * @brief 获取 MAVLink 参数数量。
   *
   * @return MAVLink 参数数量。
   */
  uint16_t Count() const;

  /**
   * @brief 按索引读取 MAVLink 参数值。
   *
   * @param index MAVLink 参数索引。
   * @param parameter 参数值输出。
   * @return 读取成功返回 `true`。
   */
  bool ReadByIndex(uint16_t index, MavlinkParameterValue *parameter) const;

  /**
   * @brief 按名称读取 MAVLink 参数值。
   *
   * @param name MAVLink 参数名。
   * @param parameter 参数值输出。
   * @return 读取成功返回 `true`。
   */
  bool ReadByName(const char *name, MavlinkParameterValue *parameter) const;

  /**
   * @brief 查找 MAVLink 参数索引。
   *
   * @param name MAVLink 参数名。
   * @return 参数索引，未找到返回 `-1`。
   */
  int16_t IndexOf(const char *name) const;

  /**
   * @brief 写入 MAVLink 参数值。
   *
   * @param name MAVLink 参数名。
   * @param value MAVLink 浮点参数值。
   * @param type MAVLink 参数类型。
   * @return 写入成功返回 `true`。
   */
  bool WriteValue(const char *name, float value, uint8_t type);

private:
  /**
   * @brief 获取当前使用的系统参数中心。
   *
   * @return 系统参数中心引用。
   */
  ParameterManager &Parameters() const;

  ParameterManager *parameters_ = nullptr; /**< 当前绑定的系统参数中心。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_PARAMETER_SERVICE_HPP */

