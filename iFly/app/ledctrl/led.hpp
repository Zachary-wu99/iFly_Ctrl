/**
 * @file led.hpp
 * @brief LED control interface.
 */
#ifndef IFLY_APP_LED_HPP
#define IFLY_APP_LED_HPP

#include <stdint.h>

#include "gpio.hpp"

namespace iFly {

enum class LedActiveLevel : uint8_t {
  kHigh = 0U,
  kLow = 1U
};

namespace led_detail {

constexpr GpioPinState LogicalToPhysical(LedActiveLevel activeLevel, bool on) {
  if (activeLevel == LedActiveLevel::kLow) {
    return on ? GpioPinState::kReset : GpioPinState::kSet;
  }

  return on ? GpioPinState::kSet : GpioPinState::kReset;
}

} // namespace led_detail

template <GpioPortId PortId,
          GpioPinId PinId,
          LedActiveLevel ActiveLevel = LedActiveLevel::kHigh,
          bool DefaultOn = false>
class Led final {
public:
  using GpioType = Gpio<PortId, PinId, led_detail::LogicalToPhysical(ActiveLevel, DefaultOn)>;

  static constexpr GpioPortId kPort = PortId;
  static constexpr GpioPinId kPin = PinId;
  static constexpr LedActiveLevel kActiveLevel = ActiveLevel;
  static constexpr bool kDefaultOn = DefaultOn;

  Led(bool applyDefaultState = true) {
    (void)Init(applyDefaultState);
  }

  ~Led() {
    Deinit();
  }

  bool IsReady() const {
    return initialized_;
  }

  static constexpr GpioPortId Port() {
    return kPort;
  }

  static constexpr GpioPinId Pin() {
    return kPin;
  }

  static constexpr LedActiveLevel ActiveLevelValue() {
    return kActiveLevel;
  }

  static constexpr bool DefaultOnValue() {
    return kDefaultOn;
  }

  bool Set(bool on) const {
    if (!initialized_) {
      return false;
    }

    return gpio_.Write(LogicalToPhysical(on));
  }

  bool On() const {
    return Set(true);
  }

  bool Off() const {
    return Set(false);
  }

  bool Toggle() const {
    if (!initialized_) {
      return false;
    }

    return Set(!IsOn());
  }

  bool IsOn() const {
    if (!initialized_) {
      return false;
    }

    return ReadPin() == LogicalToPhysical(true);
  }

  GpioPinState ReadPin() const {
    if (!initialized_) {
      return GpioPinState::kReset;
    }

    return gpio_.Read();
  }

private:

  bool Init(bool applyDefaultState = true) {
    initialized_ = gpio_.Init(false);
    if (!initialized_) {
      return false;
    }

    if (applyDefaultState) {
      return Set(kDefaultOn);
    }

    return true;
  }

  void Deinit() {
    gpio_.Deinit();
    initialized_ = false;
  }

  static constexpr GpioPinState LogicalToPhysical(bool on) {
    return led_detail::LogicalToPhysical(kActiveLevel, on);
  }

  GpioType gpio_ {};
  bool initialized_ = false;
};

} // namespace iFly

#endif /* IFLY_APP_LED_HPP */
