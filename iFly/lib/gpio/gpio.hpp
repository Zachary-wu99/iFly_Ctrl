/**
 * @file gpio.hpp
 * @brief GPIO control interface.
 */
#ifndef IFLY_GPIO_HPP
#define IFLY_GPIO_HPP

#include <stdint.h>

#include "gpio.h"

namespace iFly {

enum class GpioPortId : uint8_t {
  kA = 0U,
  kB = 1U,
  kC = 2U,
  kD = 3U,
  kE = 4U,
  kF = 5U,
  kG = 6U,
  kH = 7U,
  kI = 8U,
  kNone = 0xFFU
};

enum class GpioPinId : uint8_t {
  kPin0 = 0U,
  kPin1 = 1U,
  kPin2 = 2U,
  kPin3 = 3U,
  kPin4 = 4U,
  kPin5 = 5U,
  kPin6 = 6U,
  kPin7 = 7U,
  kPin8 = 8U,
  kPin9 = 9U,
  kPin10 = 10U,
  kPin11 = 11U,
  kPin12 = 12U,
  kPin13 = 13U,
  kPin14 = 14U,
  kPin15 = 15U,
  kNone = 0xFFU
};

enum class GpioPinState : uint8_t {
  kReset = 0U,
  kSet = 1U
};

namespace gpio_detail {

inline GPIO_TypeDef *ToHalPort(GpioPortId port) {
  switch (port) {
    case GpioPortId::kA:
      return GPIOA;
    case GpioPortId::kB:
      return GPIOB;
    case GpioPortId::kC:
      return GPIOC;
    case GpioPortId::kD:
      return GPIOD;
    case GpioPortId::kE:
      return GPIOE;
    case GpioPortId::kF:
      return GPIOF;
    case GpioPortId::kG:
      return GPIOG;
    case GpioPortId::kH:
      return GPIOH;
    case GpioPortId::kI:
      return GPIOI;
    default:
      return nullptr;
  }
}

constexpr uint16_t ToHalPin(GpioPinId pin) {
  switch (pin) {
    case GpioPinId::kPin0:
      return GPIO_PIN_0;
    case GpioPinId::kPin1:
      return GPIO_PIN_1;
    case GpioPinId::kPin2:
      return GPIO_PIN_2;
    case GpioPinId::kPin3:
      return GPIO_PIN_3;
    case GpioPinId::kPin4:
      return GPIO_PIN_4;
    case GpioPinId::kPin5:
      return GPIO_PIN_5;
    case GpioPinId::kPin6:
      return GPIO_PIN_6;
    case GpioPinId::kPin7:
      return GPIO_PIN_7;
    case GpioPinId::kPin8:
      return GPIO_PIN_8;
    case GpioPinId::kPin9:
      return GPIO_PIN_9;
    case GpioPinId::kPin10:
      return GPIO_PIN_10;
    case GpioPinId::kPin11:
      return GPIO_PIN_11;
    case GpioPinId::kPin12:
      return GPIO_PIN_12;
    case GpioPinId::kPin13:
      return GPIO_PIN_13;
    case GpioPinId::kPin14:
      return GPIO_PIN_14;
    case GpioPinId::kPin15:
      return GPIO_PIN_15;
    default:
      return 0U;
  }
}

constexpr GPIO_PinState ToHalPinState(GpioPinState state) {
  return (state == GpioPinState::kSet) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

constexpr GpioPinState FromHalPinState(GPIO_PinState state) {
  return (state == GPIO_PIN_SET) ? GpioPinState::kSet : GpioPinState::kReset;
}

} // namespace gpio_detail

template <GpioPortId PortId, GpioPinId PinId, GpioPinState DefaultState = GpioPinState::kReset>
class Gpio final {
public:
  static constexpr GpioPortId kPort = PortId;
  static constexpr GpioPinId kPin = PinId;
  static constexpr GpioPinState kDefaultState = DefaultState;

  Gpio() = default;

  bool Init(bool applyDefaultState = true) {
    initialized_ = IsConfigValid();
    if (!initialized_) {
      return false;
    }

    if (applyDefaultState) {
      return Write(kDefaultState);
    }

    return true;
  }

  void Deinit() {
    initialized_ = false;
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

  static constexpr GpioPinState DefaultPinState() {
    return kDefaultState;
  }

  bool Write(GpioPinState state) const {
    if (!initialized_) {
      return false;
    }

    HAL_GPIO_WritePin(gpio_detail::ToHalPort(kPort),
                      gpio_detail::ToHalPin(kPin),
                      gpio_detail::ToHalPinState(state));
    return true;
  }

  bool Set() const {
    return Write(GpioPinState::kSet);
  }

  bool Reset() const {
    return Write(GpioPinState::kReset);
  }

  bool Toggle() const {
    if (!initialized_) {
      return false;
    }

    return Write(IsSet() ? GpioPinState::kReset : GpioPinState::kSet);
  }

  bool IsSet() const {
    if (!initialized_) {
      return false;
    }

    return Read() == GpioPinState::kSet;
  }

  GpioPinState Read() const {
    if (!initialized_) {
      return GpioPinState::kReset;
    }

    return gpio_detail::FromHalPinState(
      HAL_GPIO_ReadPin(gpio_detail::ToHalPort(kPort), gpio_detail::ToHalPin(kPin)));
  }

private:
  static bool IsConfigValid() {
    return (gpio_detail::ToHalPort(kPort) != nullptr) && (gpio_detail::ToHalPin(kPin) != 0U);
  }

  bool initialized_ = false;
};

} // namespace iFly

#endif /* IFLY_GPIO_HPP */

