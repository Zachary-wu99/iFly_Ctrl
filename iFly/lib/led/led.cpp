#include "led.hpp"

#include "gpio.h"
#include "usermath.hpp"

namespace {

constexpr GPIO_TypeDef *GpioPort(iFly::GpioPortId port) {
  switch (port) {
    case iFly::GpioPortId::kA:
      return GPIOA;
    case iFly::GpioPortId::kB:
      return GPIOB;
    case iFly::GpioPortId::kC:
      return GPIOC;
    case iFly::GpioPortId::kD:
      return GPIOD;
    case iFly::GpioPortId::kE:
      return GPIOE;
    case iFly::GpioPortId::kF:
      return GPIOF;
    case iFly::GpioPortId::kG:
      return GPIOG;
    case iFly::GpioPortId::kH:
      return GPIOH;
    case iFly::GpioPortId::kI:
      return GPIOI;
    default:
      return nullptr;
  }
}

constexpr uint16_t GpioPin(iFly::GpioPinId pin) {
  switch (pin) {
    case iFly::GpioPinId::kPin0:
      return GPIO_PIN_0;
    case iFly::GpioPinId::kPin1:
      return GPIO_PIN_1;
    case iFly::GpioPinId::kPin2:
      return GPIO_PIN_2;
    case iFly::GpioPinId::kPin3:
      return GPIO_PIN_3;
    case iFly::GpioPinId::kPin4:
      return GPIO_PIN_4;
    case iFly::GpioPinId::kPin5:
      return GPIO_PIN_5;
    case iFly::GpioPinId::kPin6:
      return GPIO_PIN_6;
    case iFly::GpioPinId::kPin7:
      return GPIO_PIN_7;
    case iFly::GpioPinId::kPin8:
      return GPIO_PIN_8;
    case iFly::GpioPinId::kPin9:
      return GPIO_PIN_9;
    case iFly::GpioPinId::kPin10:
      return GPIO_PIN_10;
    case iFly::GpioPinId::kPin11:
      return GPIO_PIN_11;
    case iFly::GpioPinId::kPin12:
      return GPIO_PIN_12;
    case iFly::GpioPinId::kPin13:
      return GPIO_PIN_13;
    case iFly::GpioPinId::kPin14:
      return GPIO_PIN_14;
    case iFly::GpioPinId::kPin15:
      return GPIO_PIN_15;
    default:
      return 0U;
  }
}

constexpr GPIO_PinState ToHalPinState(iFly::LedPinState state) {
  return (state == iFly::LedPinState::kSet) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

constexpr iFly::LedPinState FromHalPinState(GPIO_PinState state) {
  return (state == GPIO_PIN_SET) ? iFly::LedPinState::kSet : iFly::LedPinState::kReset;
}

} // namespace

namespace iFly {

Led::Led(const LedConfig &config) {
  (void)Init(config, false);
}

bool Led::Init(const LedConfig &config, bool applyDefaultState) {
  config_.activeLevel = config.activeLevel;
  config_.defaultOn = config.defaultOn;
  AttachHardware(config.port, config.pin);
  if (!initialized_) {
    return false;
  }

  if (applyDefaultState) {
    return Set(config_.defaultOn);
  }

  return true;
}

void Led::Deinit() {
  config_ = {};
  initialized_ = false;
}

bool Led::IsReady() const {
  return initialized_;
}

const LedConfig *Led::GetConfig() const {
  return initialized_ ? &config_ : nullptr;
}

void Led::AttachHardware(GpioPortId port, GpioPinId pin) {
  config_.port = port;
  config_.pin = pin;
  initialized_ = IsConfigValid();
}

GpioPortId Led::Handle() const {
  return config_.port;
}

GpioPinId Led::Pin() const {
  return config_.pin;
}

bool Led::Set(bool on) const {
  if (!initialized_) {
    return false;
  }

  HAL_GPIO_WritePin(GpioPort(config_.port), GpioPin(config_.pin), ToHalPinState(LogicalToPhysical(on)));
  return true;
}

bool Led::On() const {
  return Set(true);
}

bool Led::Off() const {
  return Set(false);
}

bool Led::Toggle() const {
  if (!initialized_) {
    return false;
  }

  return Set(!IsOn());
}

bool Led::IsOn() const {
  if (!initialized_) {
    return false;
  }

  return ReadPin() == LogicalToPhysical(true);
}

LedPinState Led::ReadPin() const {
  if (!initialized_) {
    return LedPinState::kReset;
  }

  return FromHalPinState(HAL_GPIO_ReadPin(GpioPort(config_.port), GpioPin(config_.pin)));
}

LedPinState Led::LogicalToPhysical(bool on) const {
  if (config_.activeLevel == LedActiveLevel::kLow) {
    return on ? LedPinState::kReset : LedPinState::kSet;
  }

  return on ? LedPinState::kSet : LedPinState::kReset;
}

bool Led::IsConfigValid() const {
  const GPIO_TypeDef *port = GpioPort(config_.port);
  return (port != nullptr) && (GpioPin(config_.pin) != 0U);
}

bool LedController::Init(Led *leds,
                         uint32_t ledCount,
                         const LedConfig *configs,
                         uint32_t configCount,
                         bool applyDefaultState) {
  Deinit();

  if ((leds == nullptr) || (configs == nullptr) || (ledCount == 0U) || (configCount == 0U)) {
    return false;
  }

  leds_ = leds;
  count_ = iFly::usermath::Min<uint32_t>(ledCount, configCount);

  bool allReady = (ledCount == configCount);
  for (uint32_t index = 0U; index < count_; ++index) {
    if (!leds_[index].Init(configs[index], applyDefaultState)) {
      allReady = false;
    }
  }

  return allReady;
}

void LedController::Deinit() {
  if (leds_ != nullptr) {
    for (uint32_t index = 0U; index < count_; ++index) {
      leds_[index].Deinit();
    }
  }

  leds_ = nullptr;
  count_ = 0U;
}

uint32_t LedController::Count() const {
  return count_;
}

Led *LedController::At(uint32_t index) const {
  if ((leds_ == nullptr) || (index >= count_)) {
    return nullptr;
  }

  return &leds_[index];
}

bool LedController::Set(uint32_t index, bool on) const {
  Led *led = At(index);
  return (led != nullptr) ? led->Set(on) : false;
}

bool LedController::On(uint32_t index) const {
  return Set(index, true);
}

bool LedController::Off(uint32_t index) const {
  return Set(index, false);
}

bool LedController::Toggle(uint32_t index) const {
  Led *led = At(index);
  return (led != nullptr) ? led->Toggle() : false;
}

bool LedController::IsOn(uint32_t index) const {
  Led *led = At(index);
  return (led != nullptr) ? led->IsOn() : false;
}

void LedController::SetAll(bool on) const {
  if (leds_ == nullptr) {
    return;
  }

  for (uint32_t index = 0U; index < count_; ++index) {
    (void)leds_[index].Set(on);
  }
}

void LedController::AllOn() const {
  SetAll(true);
}

void LedController::AllOff() const {
  SetAll(false);
}

} // namespace iFly
