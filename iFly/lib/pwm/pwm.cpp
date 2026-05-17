#include "pwm.hpp"

#include "tim.h"
#include "usermath.hpp"

namespace iFly {

const char *ToString(PwmChannelId channel) {
  switch (channel) {
    case PwmChannelId::kChannel1:
      return "CH1";
    case PwmChannelId::kChannel2:
      return "CH2";
    case PwmChannelId::kChannel3:
      return "CH3";
    case PwmChannelId::kChannel4:
      return "CH4";
    default:
      return "CH";
  }
}

PwmChannel::PwmChannel(const Config &config) {
  (void)Init(config);
}

bool PwmChannel::Init(const Config &config) {
  min_compare_ = config.min_compare;
  max_compare_ = config.max_compare;
  compare_ = config.initial_compare;
  AttachHardware(config.htim, config.channel);
  if (!IsReady()) {
    return false;
  }

  return !config.auto_start || Start();
}

void PwmChannel::Deinit() {
  Stop();
  htim_ = nullptr;
  channel_ = 0U;
  min_compare_ = 0U;
  max_compare_ = 0U;
  compare_ = 0U;
}

void PwmChannel::AttachHardware(TIM_HandleTypeDef *htim, PwmChannelId channel) {
  AttachHardware(htim, ToHalChannel(channel));
}

void PwmChannel::AttachHardware(TIM_HandleTypeDef *htim, uint32_t hal_channel) {
  TIM_HandleTypeDef *old_tim = htim_;
  if ((old_tim != nullptr) && (old_tim->Instance != nullptr) &&
      IsSupportedChannel(channel_) &&
      (HAL_TIM_GetChannelState(old_tim, channel_) == HAL_TIM_CHANNEL_STATE_BUSY)) {
    (void)HAL_TIM_PWM_Stop(old_tim, channel_);
  }

  htim_ = htim;
  channel_ = hal_channel;
  if (!IsReady()) {
    return;
  }

  TIM_HandleTypeDef *tim = htim_;
  compare_ = ClampCompare(compare_);
  __HAL_TIM_SET_COMPARE(tim, channel_, compare_);
}

bool PwmChannel::Start() {
  if (!IsReady()) {
    return false;
  }

  if (IsStarted()) {
    return true;
  }

  return HAL_TIM_PWM_Start(htim_, channel_) == HAL_OK;
}

void PwmChannel::Stop() {
  if (!IsReady() || !IsStarted()) {
    return;
  }

  (void)HAL_TIM_PWM_Stop(htim_, channel_);
}

bool PwmChannel::SetCompare(uint32_t compare) {
  if (!IsReady()) {
    return false;
  }

  TIM_HandleTypeDef *tim = htim_;
  compare_ = ClampCompare(compare);
  __HAL_TIM_SET_COMPARE(tim, channel_, compare_);
  return true;
}

bool PwmChannel::SetDutyCycle(float duty_cycle) {
  if (!IsReady() || (duty_cycle < 0.0f) || (duty_cycle > 1.0f)) {
    return false;
  }

  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare = EffectiveMaxCompare();
  const uint32_t compare =
      min_compare + static_cast<uint32_t>(
                        (static_cast<double>(max_compare - min_compare) *
                         static_cast<double>(duty_cycle)) +
                        0.5);
  return SetCompare(compare);
}

bool PwmChannel::IsReady() const {
  const TIM_HandleTypeDef *tim = htim_;
  return (tim != nullptr) && (tim->Instance != nullptr) &&
         IsSupportedChannel(channel_);
}

bool PwmChannel::IsStarted() const {
  if (!IsReady()) {
    return false;
  }

  return HAL_TIM_GetChannelState(htim_, channel_) ==
         HAL_TIM_CHANNEL_STATE_BUSY;
}

TIM_HandleTypeDef *PwmChannel::Handle() const {
  return htim_;
}

uint32_t PwmChannel::HalChannel() const {
  return channel_;
}

uint32_t PwmChannel::Compare() const {
  if (!IsReady()) {
    return compare_;
  }

  return __HAL_TIM_GET_COMPARE(htim_, channel_);
}

uint32_t PwmChannel::Period() const {
  if (!IsReady()) {
    return 0U;
  }

  return __HAL_TIM_GET_AUTORELOAD(htim_);
}

uint32_t PwmChannel::MinCompare() const {
  return EffectiveMinCompare();
}

uint32_t PwmChannel::MaxCompare() const {
  return EffectiveMaxCompare();
}

float PwmChannel::DutyCycle() const {
  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare = EffectiveMaxCompare();
  if (max_compare <= min_compare) {
    return 0.0f;
  }

  return static_cast<float>(static_cast<double>(Compare() - min_compare) /
                            static_cast<double>(max_compare - min_compare));
}

uint32_t PwmChannel::ToHalChannel(PwmChannelId channel) {
  switch (channel) {
    case PwmChannelId::kChannel1:
      return TIM_CHANNEL_1;
    case PwmChannelId::kChannel2:
      return TIM_CHANNEL_2;
    case PwmChannelId::kChannel3:
      return TIM_CHANNEL_3;
    case PwmChannelId::kChannel4:
      return TIM_CHANNEL_4;
    default:
      return 0U;
  }
}

bool PwmChannel::IsSupportedChannel(uint32_t hal_channel) {
  switch (hal_channel) {
    case TIM_CHANNEL_1:
    case TIM_CHANNEL_2:
    case TIM_CHANNEL_3:
    case TIM_CHANNEL_4:
      return true;
    default:
      return false;
  }
}

uint32_t PwmChannel::EffectiveMinCompare() const {
  if (!IsReady()) {
    return 0U;
  }

  const uint32_t period = Period();
  return usermath::Min<uint32_t>(min_compare_, period);
}

uint32_t PwmChannel::EffectiveMaxCompare() const {
  if (!IsReady()) {
    return 0U;
  }

  const uint32_t period = Period();
  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare =
      ((max_compare_ == 0U) || (max_compare_ > period)) ? period : max_compare_;
  return usermath::Max<uint32_t>(max_compare, min_compare);
}

uint32_t PwmChannel::ClampCompare(uint32_t compare) const {
  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare = EffectiveMaxCompare();
  return usermath::Clamp<uint32_t>(compare, min_compare, max_compare);
}

} // namespace iFly

