#include "pwm.hpp"

#include "lib/platform/platform_handle.hpp"

namespace iFly {

namespace {

TIM_HandleTypeDef *TimHandle(void *handle) {
  return iFly::platform::AsTimHandle(handle);
}

const TIM_HandleTypeDef *TimHandle(const void *handle) {
  return iFly::platform::AsTimHandle(handle);
}

} // namespace

/* 把逻辑 PWM 通道号转换成便于日志/调试查看的文本。 */
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

/* 允许通过配置直接构造，内部复用统一的 Init() 路径。 */
PwmChannel::PwmChannel(const Config &config) {
  (void)Init(config);
}

/*
 * 初始化流程：
 * 1. 先记录 compare 限幅和初始值；
 * 2. 再绑定到底层 TIM 句柄与通道；
 * 3. 如果要求自动启动，则继续调用 Start()。
 */
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

/* 反初始化时先停输出，再清空绑定关系和缓存状态。 */
void PwmChannel::Deinit() {
  Stop();
  htim_ = nullptr;
  channel_ = 0U;
  min_compare_ = 0U;
  max_compare_ = 0U;
  compare_ = 0U;
}

void PwmChannel::AttachHardware(void *htim, PwmChannelId channel) {
  AttachHardware(htim, ToHalChannel(channel));
}

/*
 * 重新绑定硬件时，如果旧通道正在输出，先停掉旧输出，避免对象迁移后
 * 旧定时器通道继续保持 PWM 状态。
 */
void PwmChannel::AttachHardware(void *htim, uint32_t hal_channel) {
  TIM_HandleTypeDef *old_tim = TimHandle(htim_);
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

  TIM_HandleTypeDef *tim = TimHandle(htim_);
  compare_ = ClampCompare(compare_);
  __HAL_TIM_SET_COMPARE(tim, channel_, compare_);
}

/* 启动当前 PWM 通道；如果已经启动，则直接视为成功。 */
bool PwmChannel::Start() {
  if (!IsReady()) {
    return false;
  }

  if (IsStarted()) {
    return true;
  }

  return HAL_TIM_PWM_Start(TimHandle(htim_), channel_) == HAL_OK;
}

/* 停止当前 PWM 通道输出。 */
void PwmChannel::Stop() {
  if (!IsReady() || !IsStarted()) {
    return;
  }

  (void)HAL_TIM_PWM_Stop(TimHandle(htim_), channel_);
}

/* 直接按 compare 值设置输出，并执行限幅。 */
bool PwmChannel::SetCompare(uint32_t compare) {
  if (!IsReady()) {
    return false;
  }

  TIM_HandleTypeDef *tim = TimHandle(htim_);
  compare_ = ClampCompare(compare);
  __HAL_TIM_SET_COMPARE(tim, channel_, compare_);
  return true;
}

/*
 * 归一化占空比控制：
 * - `0.0f` 对应最小 compare；
 * - `1.0f` 对应最大 compare；
 * - 中间值按线性比例映射。
 */
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

/* 只有句柄、底层 TIM 实例和通道都有效时，才认为对象处于可用状态。 */
bool PwmChannel::IsReady() const {
  const TIM_HandleTypeDef *tim = TimHandle(static_cast<const void *>(htim_));
  return (tim != nullptr) && (tim->Instance != nullptr) &&
         IsSupportedChannel(channel_);
}

/* 通过 HAL 的通道状态判断当前 PWM 是否已经启动。 */
bool PwmChannel::IsStarted() const {
  if (!IsReady()) {
    return false;
  }

  return HAL_TIM_GetChannelState(TimHandle(static_cast<const void *>(htim_)), channel_) ==
         HAL_TIM_CHANNEL_STATE_BUSY;
}

void *PwmChannel::Handle() const {
  return htim_;
}

uint32_t PwmChannel::HalChannel() const {
  return channel_;
}

/* 优先读取实际寄存器值，避免只返回缓存值导致状态不同步。 */
uint32_t PwmChannel::Compare() const {
  if (!IsReady()) {
    return compare_;
  }

  return __HAL_TIM_GET_COMPARE(TimHandle(static_cast<const void *>(htim_)), channel_);
}

uint32_t PwmChannel::Period() const {
  if (!IsReady()) {
    return 0U;
  }

  return __HAL_TIM_GET_AUTORELOAD(TimHandle(static_cast<const void *>(htim_)));
}

uint32_t PwmChannel::MinCompare() const {
  return EffectiveMinCompare();
}

uint32_t PwmChannel::MaxCompare() const {
  return EffectiveMaxCompare();
}

/* 把当前 compare 反算成 0.0f ~ 1.0f 的占空比表示。 */
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

/* 判断一个 HAL 通道值是否在当前类支持的四路普通 PWM 通道内。 */
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

/* 最小 compare 不能超过当前定时器 ARR。 */
uint32_t PwmChannel::EffectiveMinCompare() const {
  if (!IsReady()) {
    return 0U;
  }

  const uint32_t period = Period();
  return (min_compare_ > period) ? period : min_compare_;
}

/*
 * 最大 compare 的处理规则：
 * - `max_compare_ == 0` 时，默认把 ARR 当作上限；
 * - 用户给的上限如果超过 ARR，也会被压回 ARR；
 * - 如果上限反而小于下限，则最终与下限对齐。
 */
uint32_t PwmChannel::EffectiveMaxCompare() const {
  if (!IsReady()) {
    return 0U;
  }

  const uint32_t period = Period();
  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare =
      ((max_compare_ == 0U) || (max_compare_ > period)) ? period : max_compare_;
  return (max_compare < min_compare) ? min_compare : max_compare;
}

/* 对 compare 做统一夹紧，避免输出超出当前合法区间。 */
uint32_t PwmChannel::ClampCompare(uint32_t compare) const {
  const uint32_t min_compare = EffectiveMinCompare();
  const uint32_t max_compare = EffectiveMaxCompare();
  if (compare < min_compare) {
    return min_compare;
  }

  if (compare > max_compare) {
    return max_compare;
  }

  return compare;
}

} // namespace iFly
