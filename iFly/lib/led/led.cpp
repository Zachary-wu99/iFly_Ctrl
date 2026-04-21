#include "led.hpp"

namespace {

// 返回两个数量中的较小值，用于避免对象数组和配置表长度不一致时越界。
uint32_t MinCount(uint32_t left, uint32_t right) {
  return (left < right) ? left : right;
}

} // namespace

namespace iFly {

Led::Led(const LedConfig &config)
    : config_(config),
      initialized_((config.port != nullptr) && (config.pin != 0U)) {
}

// 绑定新的 LED 配置，并根据需要立即把 GPIO 输出到默认状态。
bool Led::Init(const LedConfig &config, bool applyDefaultState) {
  config_ = config;
  initialized_ = IsConfigValid();
  if (!initialized_) {
    return false;
  }

  if (applyDefaultState) {
    return Set(config_.defaultOn);
  }

  return true;
}

// 清除当前配置，让对象回到未初始化状态。
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

// 按“逻辑亮灭”控制 LED，不直接暴露底层高低电平给业务层。
bool Led::Set(bool on) const {
  if (!initialized_) {
    return false;
  }

  HAL_GPIO_WritePin(config_.port, config_.pin, LogicalToPhysical(on));
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

  // 先读取当前逻辑状态，再反向写回。
  return Set(!IsOn());
}

// 判断 LED 是否点亮时，需要结合有效电平配置做一次逻辑换算。
bool Led::IsOn() const {
  if (!initialized_) {
    return false;
  }

  return ReadPin() == LogicalToPhysical(true);
}

GPIO_PinState Led::ReadPin() const {
  if (!initialized_) {
    return GPIO_PIN_RESET;
  }

  return HAL_GPIO_ReadPin(config_.port, config_.pin);
}

// 将“亮/灭”的逻辑语义映射成真实 GPIO 输出电平。
GPIO_PinState Led::LogicalToPhysical(bool on) const {
  if (config_.activeLevel == LedActiveLevel::kLow) {
    return on ? GPIO_PIN_RESET : GPIO_PIN_SET;
  }

  return on ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

// 当前最基础的合法性判断：端口和引脚都必须有效。
bool Led::IsConfigValid() const {
  return (config_.port != nullptr) && (config_.pin != 0U);
}

bool LedController::Init(Led *leds,
                         uint32_t ledCount,
                         const LedConfig *configs,
                         uint32_t configCount,
                         bool applyDefaultState) {
  // 先清掉上一轮绑定，避免重复初始化时残留旧状态。
  Deinit();

  if ((leds == nullptr) || (configs == nullptr) || (ledCount == 0U) || (configCount == 0U)) {
    return false;
  }

  leds_ = leds;
  // 对象数组和配置表长度不一致时，只初始化两者都覆盖到的那部分。
  count_ = MinCount(ledCount, configCount);

  bool allReady = (ledCount == configCount);
  for (uint32_t index = 0U; index < count_; ++index) {
    if (!leds_[index].Init(configs[index], applyDefaultState)) {
      allReady = false;
    }
  }

  return allReady;
}

// 逐个释放已接管的 LED 对象绑定关系。
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

// 批量操作时忽略单个 LED 的返回值，适合上层做统一开关控制。
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
