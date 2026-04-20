// CRSF 协议接口。
// 定义帧结构、RC 通道打包格式以及解析/编码入口。
#ifndef IFLY_APP_PROTO_CRSF_PROTOCOL_HPP
#define IFLY_APP_PROTO_CRSF_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

/** @brief 一帧 CRSF 解码结果。 */
struct CrsfFrame final {
  static constexpr uint8_t kMaxPayloadSize = 60U;

  uint8_t deviceAddress = 0xC8U;
  uint8_t type = 0U;
  bool extended = false;
  uint8_t destination = 0U;
  uint8_t source = 0U;
  uint8_t payloadLength = 0U;
  uint8_t payload[kMaxPayloadSize] {};
};

/** @brief CRSF 的 16 通道打包格式。 */
struct CrsfRcChannelsPacked final {
  static constexpr uint8_t kChannelCount = 16U;

  uint16_t channels[kChannelCount] {};
};

/**
 * @brief CRSF 协议处理器。
 *
 * @details
 * 负责维护流式解析缓冲区，支持从串行字节流中拆包 CRSF 帧，
 * 并提供单帧编码、CRC 计算以及 RC 通道打包辅助函数。
 */
class CrsfProtocol final {
public:
  static constexpr uint8_t kSyncByte = 0xC8U;
  static constexpr uint8_t kEdgeTxSyncByte = 0xEEU;
  static constexpr uint8_t kMaxPacketSize = 64U;
  static constexpr uint8_t kMinPacketSize = 4U;
  static constexpr uint8_t kMinFrameLength = 2U;
  static constexpr uint8_t kMaxFrameLength = 62U;
  static constexpr uint8_t kExtendedTypeMin = 0x28U;
  static constexpr uint8_t kRcChannelsPackedType = 0x16U;
  static constexpr uint8_t kCrcPolynomial = 0xD5U;
  static constexpr uint8_t kRcChannelsPayloadSize = 22U;

  struct ParseStats final {
    uint32_t bytesReceived = 0U;
    uint32_t framesDecoded = 0U;
    uint32_t framesDelivered = 0U;
    uint32_t framesDropped = 0U;
    uint32_t invalidFrames = 0U;
    uint32_t droppedBytes = 0U;
    uint32_t crcErrors = 0U;
    uint32_t resyncCount = 0U;
  };

  CrsfProtocol() = default;

  /** @brief 清空内部缓冲和统计状态。 */
  void Reset();

  /** @brief 从字节流中解析尽可能多的 CRSF 帧。 */
  uint32_t Parse(const uint8_t *data,
                 uint32_t length,
                 CrsfFrame *outFrames,
                 uint32_t maxFrames);

  /** @brief 把一帧 CRSF 数据编码成原始字节流。 */
  bool Encode(const CrsfFrame &frame,
              uint8_t *outFrame,
              uint32_t outCapacity,
              uint32_t *writtenLength = nullptr) const;

  /** @brief 返回累计解析统计信息。 */
  const ParseStats &Stats() const {
    return stats_;
  }

  /** @brief 尝试直接解码一段完整原始帧。 */
  static bool TryDecodeFrame(const uint8_t *rawFrame,
                             uint32_t rawLength,
                             CrsfFrame *frame);

  /** @brief 校验一段原始帧是否有效。 */
  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);
  /** @brief 判断类型是否属于扩展帧。 */
  static bool IsExtendedType(uint8_t type);
  /** @brief 计算 CRSF CRC。 */
  static uint8_t ComputeCrc(const uint8_t *data, uint32_t length);

  /** @brief 解包 RC 通道载荷。 */
  static bool DecodeRcChannelsPacked(const CrsfFrame &frame,
                                     CrsfRcChannelsPacked *channels);

  /** @brief 按 RC 通道格式编码一帧 CRSF 原始数据。 */
  static bool EncodeRcChannelsPacked(uint8_t deviceAddress,
                                     const CrsfRcChannelsPacked &channels,
                                     uint8_t *outFrame,
                                     uint32_t outCapacity,
                                     uint32_t *writtenLength = nullptr);

private:
  /** @brief 根据 CRSF length 字段推导整个包长。 */
  static uint8_t ComputeExpectedPacketSize(uint8_t frameLength);
  /** @brief 从 11bit 打包载荷中读取一个通道值。 */
  static uint16_t ReadChannel11(const uint8_t *payload, uint8_t channelIndex);
  /** @brief 向 11bit 打包载荷中写入一个通道值。 */
  static void WriteChannel11(uint8_t *payload, uint8_t channelIndex, uint16_t value);

  /** @brief 根据当前缓冲区刷新期望包长。 */
  void RefreshExpectedPacketSize();
  /** @brief 消费缓冲区头部已处理字节。 */
  void ConsumeLeadingBytes(uint8_t count);
  /** @brief 丢弃无效前导字节并计入统计。 */
  void DropLeadingBytes(uint8_t count);

private:
  uint8_t buffer_[kMaxPacketSize] {};
  uint8_t bufferedBytes_ = 0U;
  uint8_t expectedPacketSize_ = 0U;
  ParseStats stats_ {};
};

} // namespace iFly

#endif /* IFLY_APP_PROTO_CRSF_PROTOCOL_HPP */
