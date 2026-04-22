#include "led.hpp"

#include "platform_handle.hpp"
#include "usermath.hpp"

namespace {

GPIO_TypeDef *GpioPort(void *handle) {
  return iFly::platform::AsGpioPort(handle);
}

const GPIO_TypeDef *GpioPort(const void *handle) {
  return iFly::platform::AsGpioPort(handle);
}

GPIO_PinState ToHalPinState(iFly::LedPinState state) {
  return (state == iFly::LedPinState::kSet) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

iFly::LedPinState FromHalPinState(GPIO_PinState state) {
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

void Led::AttachHardware(void *port, uint16_t pin) {
  config_.port = port;
  config_.pin = pin;
  initialized_ = IsConfigValid();
}

void *Led::Handle() const {
  return config_.port;
}

uint16_t Led::Pin() const {
  return config_.pin;
}

bool Led::Set(bool on) const {
  if (!initialized_) {
    return false;
  }

  HAL_GPIO_WritePin(GpioPort(config_.port), config_.pin, ToHalPinState(LogicalToPhysical(on)));
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

  return FromHalPinState(HAL_GPIO_ReadPin(GpioPort(config_.port), config_.pin));
}

LedPinState Led::LogicalToPhysical(bool on) const {
  if (config_.activeLevel == LedActiveLevel::kLow) {
    return on ? LedPinState::kReset : LedPinState::kSet;
  }

  return on ? LedPinState::kSet : LedPinState::kReset;
}

bool Led::IsConfigValid() const {
  const GPIO_TypeDef *port = GpioPort(static_cast<const void *>(config_.port));
  return (port != nullptr) && (config_.pin != 0U);
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
