// SBUS 协议接口。
// 定义标准 25 字节帧结构以及解析和编码入口。
#ifndef IFLY_APP_PROTO_SBUS_PROTOCOL_HPP
#define IFLY_APP_PROTO_SBUS_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

/** @brief 一帧 SBUS 解码结果。 */
struct SbusFrame final {
  static constexpr uint8_t kAnalogChannelCount = 16U;

  uint16_t channels[kAnalogChannelCount] {};
  bool channel17 = false;
  bool channel18 = false;
  bool frameLost = false;
  bool failsafe = false;
  uint8_t footer = 0x00U;
};

/**
 * @brief SBUS 协议处理器。
 *
 * @details
 * 负责从串口字节流中同步 SBUS 帧边界，完成 16 路 11bit 通道
 * 的解包与回写，并输出基础解析统计信息。
 */
class SbusProtocol final {
public:
  static constexpr uint8_t kFrameSize = 25U;
  static constexpr uint8_t kPayloadSize = 22U;
  static constexpr uint8_t kHeaderByte = 0x0FU;
  static constexpr uint8_t kDefaultFooterByte = 0x00U;

  static constexpr uint8_t kChannel17Mask = 0x01U;
  static constexpr uint8_t kChannel18Mask = 0x02U;
  static constexpr uint8_t kFrameLostMask = 0x04U;
  static constexpr uint8_t kFailsafeMask = 0x08U;

  struct ParseStats final {
    uint32_t bytesReceived = 0U;
    uint32_t framesDecoded = 0U;
    uint32_t framesDelivered = 0U;
    uint32_t framesDropped = 0U;
    uint32_t invalidFrames = 0U;
    uint32_t droppedBytes = 0U;
    uint32_t resyncCount = 0U;
  };

  SbusProtocol() = default;

  /** @brief 清空内部缓冲与统计状态。 */
  void Reset();

  /** @brief 从字节流中解析尽可能多的 SBUS 帧。 */
  uint32_t Parse(const uint8_t *data,
                 uint32_t length,
                 SbusFrame *outFrames,
                 uint32_t maxFrames);

  /** @brief 把一帧 SBUS 数据编码成标准 25 字节原始帧。 */
  bool Encode(const SbusFrame &frame,
              uint8_t *outFrame,
              uint32_t outLength = kFrameSize) const;

  /** @brief 返回累计解析统计信息。 */
  const ParseStats &Stats() const {
    return stats_;
  }

  /** @brief 尝试直接解码一段完整 SBUS 原始帧。 */
  static bool TryDecodeFrame(const uint8_t *rawFrame,
                             uint32_t rawLength,
                             SbusFrame *frame);

  /** @brief 校验原始帧是否合法。 */
  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);
  /** @brief 校验尾字节是否符合 SBUS 约定。 */
  static bool IsValidFooter(uint8_t footer);

private:
  static constexpr uint8_t kPayloadOffset = 1U;
  static constexpr uint8_t kFlagsOffset = 23U;
  static constexpr uint8_t kFooterOffset = 24U;

  /** @brief 从 11bit 打包载荷中读取一个通道值。 */
  static uint16_t ReadChannel(const uint8_t *payload, uint8_t channelIndex);
  /** @brief 向 11bit 打包载荷中写入一个通道值。 */
  static void WriteChannel(uint8_t *payload, uint8_t channelIndex, uint16_t value);

  /** @brief 丢弃头部无效字节，直到下一个候选帧头。 */
  void DropUntilNextCandidate();

private:
  uint8_t buffer_[kFrameSize] {};
  uint8_t bufferedBytes_ = 0U;
  ParseStats stats_ {};
};

} // namespace iFly

#endif /* IFLY_APP_PROTO_SBUS_PROTOCOL_HPP */
