/**
 * @file rc.hpp
 * @brief RC 接收机接收与发送接口。
 */
#ifndef IFLY_APP_FLY_SYS_RC_RC_HPP
#define IFLY_APP_FLY_SYS_RC_RC_HPP

#include <stddef.h>
#include <stdint.h>

#include "serial_io_base.hpp"
#include "sbus_protocol.hpp"
#include "crsf_protocol.hpp"

namespace iFly {

/**
 * @brief RC 协议类型。
 */
enum class RcProtocolType : uint8_t {
  kUnknown = 0U, /**< 未识别。 */
  kSbus = 1U, /**< SBUS 协议。 */
  kCrsf = 2U /**< CRSF 协议。 */
};

/**
 * @brief RC 接收机状态。
 */
struct RcReceiverState final {
  RcProtocolType protocol = RcProtocolType::kUnknown; /**< 当前识别到的协议。 */
  bool connected = false; /**< 当前是否判定为在线。 */
  uint32_t lastUpdateMs = 0U; /**< 最近一次解出有效帧的时间戳。 */
  uint32_t decodedFrames = 0U; /**< 累计解码帧数。 */
  uint32_t invalidFrames = 0U; /**< 累计非法帧数。 */
  uint32_t droppedBytes = 0U; /**< 累计丢弃字节数。 */
  uint32_t lastSbusFrames = 0U; /**< 最近一次轮询输出的 SBUS 帧数。 */
  uint32_t lastCrsfFrames = 0U; /**< 最近一次轮询输出的 CRSF 帧数。 */
};

/**
 * @brief 标准化后的 RC 输入。
 */
struct RcInput final {
  static constexpr uint8_t kChannelCount = 18U; /**< RC 通道数量。 */

  uint32_t time_boot_ms = 0U; /**< 时间戳，单位为 ms。 */
  RcProtocolType protocol = RcProtocolType::kUnknown; /**< 当前协议。 */
  bool connected = false; /**< 是否在线。 */
  uint8_t count = 0U; /**< 有效通道数量。 */
  uint16_t channel[kChannelCount] {}; /**< 归一化后的通道值，单位为 us。 */
  uint8_t rssi = 0xFFU; /**< 接收强度。 */
};

/**
 * @brief RC 接收/发送服务。
 */
class Rc final {
public:
  static constexpr uint8_t kDefaultFrameBatchSize = 8U; /**< 单次最多输出的帧数。 */
  static constexpr uint32_t kOfflineTimeoutMs = 300U; /**< 判定掉线的超时时间。 */

  /**
   * @brief 构造 RC 服务对象。
   *
   * @param io 底层串行 IO 对象。
   */
  explicit Rc(SerialIoBase *io = nullptr);

  /**
   * @brief 绑定底层串行 IO。
   *
   * @param io 底层串行 IO 对象。
   */
  void BindIo(SerialIoBase *io);

  /**
   * @brief 获取当前绑定的串行 IO。
   *
   * @return 当前串行 IO 对象地址。
   */
  SerialIoBase *BoundIo() const;

  /**
   * @brief 重置协议解析器和接收状态。
   */
  void Reset();

  /**
   * @brief 轮询接收机数据，只更新内部状态。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   * @return 本次解析出的帧数量。
   */
  uint32_t Poll(uint32_t now_ms);

  /**
   * @brief 轮询接收机数据，并输出解析到的协议帧。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   * @param sbusFrames SBUS 帧输出缓冲区。
   * @param maxSbusFrames SBUS 帧输出缓冲区容量。
   * @param crsfFrames CRSF 帧输出缓冲区。
   * @param maxCrsfFrames CRSF 帧输出缓冲区容量。
   * @return 本次解析出的帧数量。
   */
  uint32_t Poll(uint32_t now_ms, SbusFrame *sbusFrames, uint32_t maxSbusFrames,
                CrsfFrame *crsfFrames, uint32_t maxCrsfFrames);

  /**
   * @brief 直接发送一段原始 RC 串口数据。
   *
   * @param data 待发送数据首地址。
   * @param length 待发送数据长度。
   * @return 完整写入返回 `true`。
   */
  bool Send(const uint8_t *data, uint32_t length);

  /**
   * @brief 编码并发送一帧 SBUS 数据。
   *
   * @param frame 待发送的 SBUS 帧。
   * @return 发送成功返回 `true`。
   */
  bool SendSbus(const SbusFrame &frame);

  /**
   * @brief 编码并发送一帧 CRSF 数据。
   *
   * @param frame 待发送的 CRSF 帧。
   * @return 发送成功返回 `true`。
   */
  bool SendCrsf(const CrsfFrame &frame);

  /**
   * @brief 获取当前识别到的 RC 协议。
   *
   * @return 当前 RC 协议类型。
   */
  RcProtocolType Protocol() const;

  /**
   * @brief 判断接收机是否在线。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   * @return 在线返回 `true`。
   */
  bool Connected(uint32_t now_ms) const;

  /**
   * @brief 获取当前接收机状态。
   *
   * @return 接收机状态只读引用。
   */
  const RcReceiverState &State() const;

private:
  static constexpr uint32_t kRxScratchSize = 96U; /**< 单次读取缓冲区大小。 */
  static constexpr uint8_t kStableFrameCount = 2U; /**< 锁定协议所需连续有效帧数。 */

  /**
   * @brief 将 PWM 通道值限制到 1000 到 2000 us。
   *
   * @param rawValue 原始 PWM 通道值。
   * @return 限幅后的 PWM 通道值。
   */
  static uint16_t NormalizeChannelValue(uint16_t rawValue);

  /**
   * @brief 将二值通道转换为 PWM 通道值。
   *
   * @param active 通道是否激活。
   * @return 激活返回 2000 us，否则返回 1000 us。
   */
  static uint16_t NormalizeChannelValue(bool active);

  /**
   * @brief 限制 PWM 通道值范围。
   *
   * @param value 待限制的 PWM 通道值。
   * @return 限幅后的 PWM 通道值。
   */
  static uint16_t ClampChannelValue(uint16_t value);

  /**
   * @brief 根据超时时间刷新在线状态。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   */
  void UpdateState(uint32_t now_ms);

  /**
   * @brief 锁定当前输入协议为 SBUS。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   */
  void MarkSbusLocked(uint32_t now_ms);

  /**
   * @brief 锁定当前输入协议为 CRSF。
   *
   * @param now_ms 当前时间戳，单位为 ms。
   */
  void MarkCrsfLocked(uint32_t now_ms);

  /**
   * @brief 清空协议识别结果并回到未知状态。
   */
  void MarkUnknown();

  /**
   * @brief 将新收到的字节喂入 SBUS 解析器。
   *
   * @param data 输入数据首地址。
   * @param length 输入数据长度。
   * @param frames SBUS 帧输出缓冲区。
   * @param maxFrames SBUS 帧输出缓冲区容量。
   * @param delivered 已输出帧数量。
   */
  void FeedSbusParser(const uint8_t *data, uint32_t length, SbusFrame *frames,
                      uint32_t maxFrames, uint32_t *delivered);

  /**
   * @brief 将新收到的字节喂入 CRSF 解析器。
   *
   * @param data 输入数据首地址。
   * @param length 输入数据长度。
   * @param frames CRSF 帧输出缓冲区。
   * @param maxFrames CRSF 帧输出缓冲区容量。
   * @param delivered 已输出帧数量。
   */
  void FeedCrsfParser(const uint8_t *data, uint32_t length, CrsfFrame *frames,
                      uint32_t maxFrames, uint32_t *delivered);

  SerialIoBase *io_ = nullptr; /**< 底层串行 IO 对象。 */
  SbusProtocol sbus_ {}; /**< SBUS 协议解析器。 */
  CrsfProtocol crsf_ {}; /**< CRSF 协议解析器。 */
  RcReceiverState state_ {}; /**< 当前 RC 状态。 */
  uint8_t sbusHitCount_ = 0U; /**< 连续识别到 SBUS 的次数。 */
  uint8_t crsfHitCount_ = 0U; /**< 连续识别到 CRSF 的次数。 */
  uint8_t rxScratch_[kRxScratchSize] {}; /**< 读取串口的临时缓冲区。 */
};

inline Rc::Rc(SerialIoBase *io)
    : io_(io) {
}

inline void Rc::BindIo(SerialIoBase *io)
{
  io_ = io;
}

inline SerialIoBase *Rc::BoundIo() const
{
  return io_;
}

inline void Rc::Reset()
{
  sbus_.Reset();
  crsf_.Reset();
  state_ = RcReceiverState {};
  sbusHitCount_ = 0U;
  crsfHitCount_ = 0U;
}

inline RcProtocolType Rc::Protocol() const
{
  return state_.protocol;
}

inline bool Rc::Connected(uint32_t now_ms) const
{
  return state_.connected && ((now_ms - state_.lastUpdateMs) <= kOfflineTimeoutMs);
}

inline const RcReceiverState &Rc::State() const
{
  return state_;
}

inline uint16_t Rc::ClampChannelValue(uint16_t value)
{
  return (value < 1000U) ? 1000U : ((value > 2000U) ? 2000U : value);
}

inline uint16_t Rc::NormalizeChannelValue(uint16_t rawValue)
{
  const uint16_t clamped = ClampChannelValue(rawValue);
  return static_cast<uint16_t>(1000U +
                               ((static_cast<uint32_t>(clamped - 1000U) * 1000U) /
                                1000U));
}

inline uint16_t Rc::NormalizeChannelValue(bool active)
{
  return active ? 2000U : 1000U;
}

inline void Rc::MarkSbusLocked(uint32_t now_ms)
{
  state_.protocol = RcProtocolType::kSbus;
  state_.connected = true;
  state_.lastUpdateMs = now_ms;
  sbusHitCount_ = 0U;
  crsfHitCount_ = 0U;
  crsf_.Reset();
}

inline void Rc::MarkCrsfLocked(uint32_t now_ms)
{
  state_.protocol = RcProtocolType::kCrsf;
  state_.connected = true;
  state_.lastUpdateMs = now_ms;
  sbusHitCount_ = 0U;
  crsfHitCount_ = 0U;
  sbus_.Reset();
}

inline void Rc::MarkUnknown()
{
  state_.protocol = RcProtocolType::kUnknown;
  state_.connected = false;
  sbusHitCount_ = 0U;
  crsfHitCount_ = 0U;
  sbus_.Reset();
  crsf_.Reset();
}

inline void Rc::UpdateState(uint32_t now_ms)
{
  state_.connected = (state_.protocol != RcProtocolType::kUnknown) &&
                     ((now_ms - state_.lastUpdateMs) <= kOfflineTimeoutMs);
}

inline void Rc::FeedSbusParser(const uint8_t *data, uint32_t length,
                               SbusFrame *frames, uint32_t maxFrames,
                               uint32_t *delivered)
{
  if ((data == nullptr) || (length == 0U) || (delivered == nullptr)) {
    return;
  }

  const uint32_t remaining = (*delivered < maxFrames) ? (maxFrames - *delivered) : 0U;
  SbusFrame *writeFrames = (frames != nullptr) ? (frames + *delivered) : nullptr;
  const uint32_t written = sbus_.Parse(data, length, writeFrames, remaining);
  *delivered += written;
}

inline void Rc::FeedCrsfParser(const uint8_t *data, uint32_t length,
                               CrsfFrame *frames, uint32_t maxFrames,
                               uint32_t *delivered)
{
  if ((data == nullptr) || (length == 0U) || (delivered == nullptr)) {
    return;
  }

  const uint32_t remaining = (*delivered < maxFrames) ? (maxFrames - *delivered) : 0U;
  CrsfFrame *writeFrames = (frames != nullptr) ? (frames + *delivered) : nullptr;
  const uint32_t written = crsf_.Parse(data, length, writeFrames, remaining);
  *delivered += written;
}

inline uint32_t Rc::Poll(uint32_t now_ms)
{
  return Poll(now_ms, nullptr, 0U, nullptr, 0U);
}

inline uint32_t Rc::Poll(uint32_t now_ms,
                         SbusFrame *sbusFrames,
                         uint32_t maxSbusFrames,
                         CrsfFrame *crsfFrames,
                         uint32_t maxCrsfFrames)
{
  if (io_ == nullptr) {
    MarkUnknown();
    return 0U;
  }

  const uint32_t available = io_->Available();
  state_.lastSbusFrames = 0U;
  state_.lastCrsfFrames = 0U;
  if (available == 0U) {
    UpdateState(now_ms);
    return 0U;
  }

  uint32_t sbusDelivered = 0U;
  uint32_t crsfDelivered = 0U;
  while (io_->Available() > 0U) {
    const uint32_t chunkLength = (io_->Available() > kRxScratchSize) ? kRxScratchSize
                                                                     : io_->Available();
    const uint32_t readLength = io_->Read(rxScratch_, chunkLength);
    if (readLength == 0U) {
      break;
    }

    if (state_.protocol == RcProtocolType::kSbus) {
      const uint32_t decodedBefore = sbus_.Stats().framesDecoded;
      const uint32_t invalidBefore = sbus_.Stats().invalidFrames;
      const uint32_t droppedBefore = sbus_.Stats().droppedBytes;
      FeedSbusParser(rxScratch_, readLength, sbusFrames, maxSbusFrames, &sbusDelivered);
      const uint32_t decodedDelta = sbus_.Stats().framesDecoded - decodedBefore;
      state_.lastSbusFrames += decodedDelta;
      if (decodedDelta > 0U) {
        state_.lastUpdateMs = now_ms;
        state_.connected = true;
        state_.decodedFrames += decodedDelta;
        state_.invalidFrames += sbus_.Stats().invalidFrames - invalidBefore;
        state_.droppedBytes += sbus_.Stats().droppedBytes - droppedBefore;
      }
      continue;
    }

    if (state_.protocol == RcProtocolType::kCrsf) {
      const uint32_t decodedBefore = crsf_.Stats().framesDecoded;
      const uint32_t invalidBefore = crsf_.Stats().invalidFrames;
      const uint32_t droppedBefore = crsf_.Stats().droppedBytes;
      FeedCrsfParser(rxScratch_, readLength, crsfFrames, maxCrsfFrames, &crsfDelivered);
      const uint32_t decodedDelta = crsf_.Stats().framesDecoded - decodedBefore;
      state_.lastCrsfFrames += decodedDelta;
      if (decodedDelta > 0U) {
        state_.lastUpdateMs = now_ms;
        state_.connected = true;
        state_.decodedFrames += decodedDelta;
        state_.invalidFrames += crsf_.Stats().invalidFrames - invalidBefore;
        state_.droppedBytes += crsf_.Stats().droppedBytes - droppedBefore;
      }
      continue;
    }

    const uint32_t sbusDecodedBefore = sbus_.Stats().framesDecoded;
    const uint32_t sbusInvalidBefore = sbus_.Stats().invalidFrames;
    const uint32_t sbusDroppedBefore = sbus_.Stats().droppedBytes;
    const uint32_t crsfDecodedBefore = crsf_.Stats().framesDecoded;
    const uint32_t crsfInvalidBefore = crsf_.Stats().invalidFrames;
    const uint32_t crsfDroppedBefore = crsf_.Stats().droppedBytes;

    FeedSbusParser(rxScratch_, readLength, sbusFrames, maxSbusFrames, &sbusDelivered);
    FeedCrsfParser(rxScratch_, readLength, crsfFrames, maxCrsfFrames, &crsfDelivered);

    const uint32_t sbusDecodedDelta = sbus_.Stats().framesDecoded - sbusDecodedBefore;
    const uint32_t crsfDecodedDelta = crsf_.Stats().framesDecoded - crsfDecodedBefore;

    if (sbusDecodedDelta > 0U) {
      ++sbusHitCount_;
      crsfHitCount_ = 0U;
      state_.lastSbusFrames += sbusDecodedDelta;
      state_.decodedFrames += sbusDecodedDelta;
      state_.invalidFrames += sbus_.Stats().invalidFrames - sbusInvalidBefore;
      state_.droppedBytes += sbus_.Stats().droppedBytes - sbusDroppedBefore;
      if (sbusHitCount_ >= kStableFrameCount) {
        MarkSbusLocked(now_ms);
      }
    } else if (crsfDecodedDelta > 0U) {
      ++crsfHitCount_;
      sbusHitCount_ = 0U;
      state_.lastCrsfFrames += crsfDecodedDelta;
      state_.decodedFrames += crsfDecodedDelta;
      state_.invalidFrames += crsf_.Stats().invalidFrames - crsfInvalidBefore;
      state_.droppedBytes += crsf_.Stats().droppedBytes - crsfDroppedBefore;
      if (crsfHitCount_ >= kStableFrameCount) {
        MarkCrsfLocked(now_ms);
      }
    }
  }

  UpdateState(now_ms);
  return sbusDelivered + crsfDelivered;
}

inline bool Rc::Send(const uint8_t *data, uint32_t length)
{
  if ((io_ == nullptr) || (data == nullptr) || (length == 0U)) {
    return false;
  }

  return io_->Write(data, length) == length;
}

inline bool Rc::SendSbus(const SbusFrame &frame)
{
  uint8_t rawFrame[SbusProtocol::kFrameSize] {};
  if (!sbus_.Encode(frame, rawFrame, sizeof(rawFrame))) {
    return false;
  }

  return Send(rawFrame, sizeof(rawFrame));
}

inline bool Rc::SendCrsf(const CrsfFrame &frame)
{
  uint8_t rawFrame[CrsfProtocol::kMaxPacketSize] {};
  uint32_t writtenLength = 0U;
  if (!crsf_.Encode(frame, rawFrame, sizeof(rawFrame), &writtenLength)) {
    return false;
  }

  return Send(rawFrame, writtenLength);
}

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_RC_RC_HPP */
