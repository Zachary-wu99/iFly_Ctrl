/**
 * @file lamp.hpp
 * @brief 灯光控制接口。
 */
#ifndef IFLY_LAMP_HPP
#define IFLY_LAMP_HPP

#include "led.hpp"

namespace iFly {

/**
 * @brief 统一定义的灯光颜色。
 */
enum class LampColor : uint8_t {
  kRed = 0U, /**< 红色灯光。 */
  kGreen = 1U, /**< 绿色灯光。 */
  kBlue = 2U, /**< 蓝色灯光。 */
  KLampCount = 3U, /**< 灯光数量。 */
};

/**
 * @brief 初始化灯光控制模块。
 *
 * @return 初始化成功返回 `true`。
 */
bool LampInit();

/**
 * @brief 解除灯光控制模块绑定。
 */
void LampDeinit();

/**
 * @brief 获取当前受控灯光数量。
 *
 * @return 灯光数量。
 */
uint32_t LampCount();

/**
 * @brief 点亮指定颜色灯光。
 *
 * @param lamp 目标灯光颜色。
 * @return 操作成功返回 `true`。
 */
bool LampOn(LampColor lamp);

/**
 * @brief 熄灭指定颜色灯光。
 *
 * @param lamp 目标灯光颜色。
 * @return 操作成功返回 `true`。
 */
bool LampOff(LampColor lamp);

/**
 * @brief 翻转指定颜色灯光状态。
 *
 * @param lamp 目标灯光颜色。
 * @return 操作成功返回 `true`。
 */
bool LampToggle(LampColor lamp);

/**
 * @brief 查询指定颜色灯光是否处于点亮状态。
 *
 * @param lamp 目标灯光颜色。
 * @return 点亮返回 `true`，否则返回 `false`。
 */
bool LampIsOn(LampColor lamp);

/**
 * @brief 点亮全部灯光。
 */
void LampAllOn();

/**
 * @brief 熄灭全部灯光。
 */
void LampAllOff();

} // namespace iFly

#endif /* IFLY_LAMP_HPP */
