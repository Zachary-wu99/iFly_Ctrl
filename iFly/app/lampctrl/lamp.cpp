/**
 * @file lamp.cpp
 * @brief 灯光控制模块实现。
 */
#include "lamp.hpp"

#include "led.hpp"

namespace {

/**
 * @brief 将灯光颜色转换为 LED 控制器索引。
 *
 * @param LampId 灯光颜色。
 * @return 对应的 LED 索引。
 */
constexpr uint8_t LampId(iFly::LampColor LampId)
{
  return static_cast<uint8_t>(LampId);
}

/** 受控 LED 对象数组。 */
iFly::Led leds[LampId(iFly::LampColor::KLampCount)];

/** 灯光 LED 批量控制器。 */
iFly::LedController ledController;

/** 灯光硬件绑定配置表。 */
const iFly::LedConfig lampConfigs[LampId(iFly::LampColor::KLampCount)] = {
  {iFly::GpioPortId::kA, iFly::GpioPinId::kPin0, iFly::LedActiveLevel::kHigh, false},
  {iFly::GpioPortId::kA, iFly::GpioPinId::kPin1, iFly::LedActiveLevel::kHigh, false},
  {iFly::GpioPortId::kA, iFly::GpioPinId::kPin2, iFly::LedActiveLevel::kHigh, false},
};

} // namespace

namespace iFly {

bool LampInit()
{
  return ledController.Init(leds, LampId(LampColor::KLampCount), lampConfigs, LampId(LampColor::KLampCount), false);
}

void LampDeinit()
{
  ledController.Deinit();
}

uint32_t LampCount()
{
  return ledController.Count();
}

bool LampOn(iFly::LampColor lamp)
{
  return ledController.On(LampId(lamp));
}

bool LampOff(iFly::LampColor lamp)
{
  return ledController.Off(LampId(lamp));
}

bool LampToggle(iFly::LampColor lamp)
{
  return ledController.Toggle(LampId(lamp));
}

bool LampIsOn(iFly::LampColor lamp)
{
  return ledController.IsOn(LampId(lamp));
}

void LampAllOn()
{
  ledController.AllOn();
}

void LampAllOff()
{
  ledController.AllOff();
}

} // namespace iFly
