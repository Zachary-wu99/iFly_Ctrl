/**
 * @file rc_main.cpp
 * @brief RC 通道处理逻辑。
 */

#include "rc_main.hpp"

#include "parameter_manager.hpp"

namespace iFly {

namespace {

Rc *g_rc = nullptr;
RcInput g_input {};

constexpr uint16_t kRcRawMin = 172U; /**< 常见 11 位 RC 通道最小值。 */
constexpr uint16_t kRcRawMax = 1811U; /**< 常见 11 位 RC 通道最大值。 */

/**
 * @brief 将协议原始通道值转换为 PWM 通道值。
 *
 * @param value 协议解包后的 11 位通道值。
 * @return 转换后的 PWM 值，单位为 us。
 */
uint16_t RawToPwm(uint16_t value)
{
  if (value <= kRcRawMin) {
    return 1000U;
  }
  if (value >= kRcRawMax) {
    return 2000U;
  }

  return static_cast<uint16_t>(1000U +
                               ((static_cast<uint32_t>(value - kRcRawMin) * 1000U) /
                                static_cast<uint32_t>(kRcRawMax - kRcRawMin)));
}

/**
 * @brief 按 1 基通道编号读取通道值。
 *
 * @param input 当前 RC 输入。
 * @param index 1 基通道编号。
 * @return 通道值，编号无效时返回中位值。
 */
uint16_t ChannelFromIndex(const RcInput &input, int32_t index)
{
  if ((index <= 0) || (index > static_cast<int32_t>(RcInput::kChannelCount))) {
    return 1500U;
  }

  return input.channel[static_cast<uint8_t>(index - 1)];
}

/**
 * @brief 限制摇杆通道值范围。
 *
 * @param value 待限制的通道值。
 * @return 限幅后的通道值。
 */
uint16_t NormalizeStick(uint16_t value)
{
  return (value < 1000U) ? 1000U : ((value > 2000U) ? 2000U : value);
}

/**
 * @brief 按系统参数中的 RC 通道映射重排主控四轴通道。
 */
void ApplyChannelMap()
{
  const SysParameters &parameters = ParameterManager::Instance().Data();
  const RcMapParameters &map = parameters.rc_map;

  const uint16_t roll = NormalizeStick(ChannelFromIndex(g_input, map.roll));
  const uint16_t pitch = NormalizeStick(ChannelFromIndex(g_input, map.pitch));
  const uint16_t throttle = NormalizeStick(ChannelFromIndex(g_input, map.throttle));
  const uint16_t yaw = NormalizeStick(ChannelFromIndex(g_input, map.yaw));

  g_input.channel[0] = roll;
  g_input.channel[1] = pitch;
  g_input.channel[2] = throttle;
  g_input.channel[3] = yaw;
}

/**
 * @brief 根据 SBUS 帧刷新标准 RC 输入。
 *
 * @param now_ms 当前时间戳，单位为 ms。
 * @param frame 最新 SBUS 帧。
 */
void BuildRcInputFromSbus(uint32_t now_ms, const SbusFrame &frame)
{
  g_input.time_boot_ms = now_ms;
  g_input.protocol = RcProtocolType::kSbus;
  g_input.connected = g_rc != nullptr ? g_rc->Connected(now_ms) : true;
  g_input.count = RcInput::kChannelCount;
  g_input.rssi = 0xFFU;

  for (uint8_t channelIndex = 0U; channelIndex < SbusFrame::kAnalogChannelCount; ++channelIndex) {
    g_input.channel[channelIndex] = RawToPwm(frame.channels[channelIndex]);
  }

  g_input.channel[16] = frame.channel17 ? 2000U : 1000U;
  g_input.channel[17] = frame.channel18 ? 2000U : 1000U;
  ApplyChannelMap();
}

/**
 * @brief 根据 CRSF 通道帧刷新标准 RC 输入。
 *
 * @param now_ms 当前时间戳，单位为 ms。
 * @param channels 最新 CRSF 通道数据。
 */
void BuildRcInputFromCrsf(uint32_t now_ms, const CrsfRcChannelsPacked &channels)
{
  g_input.time_boot_ms = now_ms;
  g_input.protocol = RcProtocolType::kCrsf;
  g_input.connected = g_rc != nullptr ? g_rc->Connected(now_ms) : true;
  g_input.count = CrsfRcChannelsPacked::kChannelCount;
  g_input.rssi = 0xFFU;

  for (uint8_t channelIndex = 0U; channelIndex < CrsfRcChannelsPacked::kChannelCount; ++channelIndex) {
    g_input.channel[channelIndex] = RawToPwm(channels.channels[channelIndex]);
  }
  for (uint8_t channelIndex = CrsfRcChannelsPacked::kChannelCount; channelIndex < RcInput::kChannelCount; ++channelIndex) {
    g_input.channel[channelIndex] = 0U;
  }

  ApplyChannelMap();
}

/**
 * @brief 标记当前 RC 输入离线。
 *
 * @param now_ms 当前时间戳，单位为 ms。
 */
void MarkInputOffline(uint32_t now_ms)
{
  g_input.time_boot_ms = now_ms;
  g_input.connected = false;
  g_input.protocol = g_rc != nullptr ? g_rc->Protocol() : RcProtocolType::kUnknown;
}

} // namespace

bool RcMainInit(Rc *receiver)
{
  g_rc = receiver;
  return g_rc != nullptr;
}

void RcMain(uint32_t now_ms)
{
  if (g_rc == nullptr) {
    return;
  }

  SbusFrame sbusFrames[Rc::kDefaultFrameBatchSize] {};
  CrsfFrame crsfFrames[Rc::kDefaultFrameBatchSize] {};
  const uint32_t delivered = g_rc->Poll(now_ms,
                                        sbusFrames,
                                        Rc::kDefaultFrameBatchSize,
                                        crsfFrames,
                                        Rc::kDefaultFrameBatchSize);

  if (delivered == 0U) {
    if (!g_rc->Connected(now_ms)) {
      MarkInputOffline(now_ms);
    }
    return;
  }

  const RcReceiverState &state = g_rc->State();
  if ((g_rc->Protocol() == RcProtocolType::kSbus) && (state.lastSbusFrames > 0U)) {
    const uint32_t frameIndex = state.lastSbusFrames - 1U;
    BuildRcInputFromSbus(now_ms, sbusFrames[frameIndex]);
    return;
  }

  if ((g_rc->Protocol() == RcProtocolType::kCrsf) && (state.lastCrsfFrames > 0U)) {
    for (int32_t index = static_cast<int32_t>(state.lastCrsfFrames) - 1; index >= 0; --index) {
      CrsfRcChannelsPacked channels {};
      if (CrsfProtocol::DecodeRcChannelsPacked(crsfFrames[index], &channels)) {
        BuildRcInputFromCrsf(now_ms, channels);
        return;
      }
    }
  }
}

const RcInput &RcMainInput()
{
  return g_input;
}

} // namespace iFly
