/**
 * @file crsf_protocol.hpp
 * @brief CRSF 协议接口。
 */
#ifndef IFLY_APP_PROTO_CRSF_PROTOCOL_HPP
#define IFLY_APP_PROTO_CRSF_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 一帧 CRSF 解码结果。
 */
struct CrsfFrame final {
  static constexpr uint8_t kMaxPayloadSize = 60U; /**< 最大负载长度。 */

  uint8_t deviceAddress = 0xC8U; /**< 设备地址。 */
  uint8_t type = 0U; /**< 帧类型。 */
  bool extended = false; /**< 是否为扩展帧。 */
  uint8_t destination = 0U; /**< 扩展帧目标地址。 */
  uint8_t source = 0U; /**< 扩展帧源地址。 */
  uint8_t payloadLength = 0U; /**< 负载长度。 */
  uint8_t payload[kMaxPayloadSize] {}; /**< 帧负载内容。 */
};

/**
 * @brief CRSF 的 16 通道打包格式。
 */
struct CrsfRcChannelsPacked final {
  static constexpr uint8_t kChannelCount = 16U; /**< 通道数量。 */

  uint16_t channels[kChannelCount] {}; /**< 16 路 RC 通道值。 */
};

/**
 * @brief CRSF 协议处理器。
 */
class CrsfProtocol final {
public:
  static constexpr uint8_t kSyncByte = 0xC8U; /**< 标准同步字节。 */
  static constexpr uint8_t kEdgeTxSyncByte = 0xEEU; /**< EdgeTX 常见同步字节。 */
  static constexpr uint8_t kMaxPacketSize = 64U; /**< 最大包长。 */
  static constexpr uint8_t kMinPacketSize = 4U; /**< 最小包长。 */
  static constexpr uint8_t kMinFrameLength = 2U; /**< 最小帧长度字段值。 */
  static constexpr uint8_t kMaxFrameLength = 62U; /**< 最大帧长度字段值。 */
  static constexpr uint8_t kExtendedTypeMin = 0x28U; /**< 扩展帧类型起始值。 */
  static constexpr uint8_t kRcChannelsPackedType = 0x16U; /**< RC 通道帧类型。 */
  static constexpr uint8_t kCrcPolynomial = 0xD5U; /**< CRC 多项式。 */
  static constexpr uint8_t kRcChannelsPayloadSize = 22U; /**< RC 通道负载长度。 */

  /**
   * @brief 解析统计信息。
   */
  struct ParseStats final {
    uint32_t bytesReceived = 0U; /**< 累计接收字节数。 */
    uint32_t framesDecoded = 0U; /**< 成功解码帧数。 */
    uint32_t framesDelivered = 0U; /**< 成功输出帧数。 */
    uint32_t framesDropped = 0U; /**< 因输出空间不足丢弃的帧数。 */
    uint32_t invalidFrames = 0U; /**< 非法帧数量。 */
    uint32_t droppedBytes = 0U; /**< 为重同步丢弃的字节数。 */
    uint32_t crcErrors = 0U; /**< CRC 校验失败次数。 */
    uint32_t resyncCount = 0U; /**< 重同步次数。 */
  };

  CrsfProtocol() = default;

  void Reset();
  uint32_t Parse(const uint8_t *data,
                 uint32_t length,
                 CrsfFrame *outFrames,
                 uint32_t maxFrames);
  bool Encode(const CrsfFrame &frame,
              uint8_t *outFrame,
              uint32_t outCapacity,
              uint32_t *writtenLength = nullptr) const;

  const ParseStats &Stats() const {
    return stats_;
  }

  static bool TryDecodeFrame(const uint8_t *rawFrame,
                             uint32_t rawLength,
                             CrsfFrame *frame);
  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);
  static bool IsExtendedType(uint8_t type);
  static uint8_t ComputeCrc(const uint8_t *data, uint32_t length);
  static bool DecodeRcChannelsPacked(const CrsfFrame &frame,
                                     CrsfRcChannelsPacked *channels);
  static bool EncodeRcChannelsPacked(uint8_t deviceAddress,
                                     const CrsfRcChannelsPacked &channels,
                                     uint8_t *outFrame,
                                     uint32_t outCapacity,
                                     uint32_t *writtenLength = nullptr);

private:
  static uint8_t ComputeExpectedPacketSize(uint8_t frameLength);
  static uint16_t ReadChannel11(const uint8_t *payload, uint8_t channelIndex);
  static void WriteChannel11(uint8_t *payload, uint8_t channelIndex, uint16_t value);
  void RefreshExpectedPacketSize();
  void ConsumeLeadingBytes(uint8_t count);
  void DropLeadingBytes(uint8_t count);

  uint8_t buffer_[kMaxPacketSize] {}; /**< 流式解析缓冲区。 */
  uint8_t bufferedBytes_ = 0U; /**< 当前缓存字节数。 */
  uint8_t expectedPacketSize_ = 0U; /**< 当前期望包长。 */
  ParseStats stats_ {}; /**< 解析统计信息。 */
};

} // namespace iFly

#endif /* IFLY_APP_PROTO_CRSF_PROTOCOL_HPP */
