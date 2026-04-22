/**
 * @file crsf_protocol.hpp
 * @brief CRSF 协议接口。
 */
#ifndef IFLY_APP_PROTO_CRSF_PROTOCOL_HPP
#define IFLY_APP_PROTO_CRSF_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 单帧 CRSF 解码结果。
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
    uint32_t droppedBytes = 0U; /**< 为重新同步丢弃的字节数。 */
    uint32_t crcErrors = 0U; /**< CRC 校验失败次数。 */
    uint32_t resyncCount = 0U; /**< 重新同步次数。 */
  };

  CrsfProtocol() = default;

  /**
   * @brief 重置协议内部状态。
   */
  void Reset();

  /**
   * @brief 解析输入字节流并输出完成的 CRSF 帧。
   *
   * @param data 输入数据首地址。
   * @param length 输入数据长度。
   * @param outFrames 输出帧缓冲区。
   * @param maxFrames 输出缓冲区最大帧数。
   * @return 实际输出的帧数。
   */
  uint32_t Parse(const uint8_t *data, uint32_t length, CrsfFrame *outFrames,
                 uint32_t maxFrames);

  /**
   * @brief 将结构化帧数据编码为 CRSF 原始帧。
   *
   * @param frame 待编码的 CRSF 帧。
   * @param outFrame 输出缓冲区。
   * @param outCapacity 输出缓冲区容量。
   * @param writtenLength 输出编码后的实际长度。
   * @return 编码成功返回 `true`。
   */
  bool Encode(const CrsfFrame &frame, uint8_t *outFrame, uint32_t outCapacity,
              uint32_t *writtenLength = nullptr) const;

  /**
   * @brief 获取当前解析统计信息。
   *
   * @return 统计信息只读引用。
   */
  const ParseStats &Stats() const {
    return stats_;
  }

  /**
   * @brief 尝试解码单帧 CRSF 原始数据。
   *
   * @param rawFrame 原始帧首地址。
   * @param rawLength 原始帧长度。
   * @param frame 输出帧对象。
   * @return 解码成功返回 `true`。
   */
  static bool TryDecodeFrame(const uint8_t *rawFrame, uint32_t rawLength,
                             CrsfFrame *frame);

  /**
   * @brief 判断原始帧是否为合法 CRSF 帧。
   *
   * @param rawFrame 原始帧首地址。
   * @param rawLength 原始帧长度。
   * @return 合法返回 `true`。
   */
  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);

  /**
   * @brief 判断帧类型是否属于扩展帧。
   *
   * @param type 帧类型字节。
   * @return 扩展帧返回 `true`。
   */
  static bool IsExtendedType(uint8_t type);

  /**
   * @brief 计算指定数据段的 CRC。
   *
   * @param data 数据首地址。
   * @param length 数据长度。
   * @return 计算得到的 CRC 值。
   */
  static uint8_t ComputeCrc(const uint8_t *data, uint32_t length);

  /**
   * @brief 解码 RC 通道打包负载。
   *
   * @param frame 待解码的 CRSF 帧。
   * @param channels 输出通道对象。
   * @return 解码成功返回 `true`。
   */
  static bool DecodeRcChannelsPacked(const CrsfFrame &frame,
                                     CrsfRcChannelsPacked *channels);

  /**
   * @brief 编码 RC 通道打包负载。
   *
   * @param deviceAddress 设备地址。
   * @param channels 待编码通道数据。
   * @param outFrame 输出缓冲区。
   * @param outCapacity 输出缓冲区容量。
   * @param writtenLength 输出编码后的实际长度。
   * @return 编码成功返回 `true`。
   */
  static bool EncodeRcChannelsPacked(uint8_t deviceAddress,
                                     const CrsfRcChannelsPacked &channels,
                                     uint8_t *outFrame, uint32_t outCapacity,
                                     uint32_t *writtenLength = nullptr);

private:
  /**
   * @brief 根据帧长度字段计算预期包长。
   *
   * @param frameLength 帧长度字段值。
   * @return 预期包长。
   */
  static uint8_t ComputeExpectedPacketSize(uint8_t frameLength);

  /**
   * @brief 从打包负载中读取一个 11 位通道值。
   *
   * @param payload 负载首地址。
   * @param channelIndex 通道索引。
   * @return 解码后的通道值。
   */
  static uint16_t ReadChannel11(const uint8_t *payload, uint8_t channelIndex);

  /**
   * @brief 将一个 11 位通道值写入打包负载。
   *
   * @param payload 负载首地址。
   * @param channelIndex 通道索引。
   * @param value 待写入的通道值。
   */
  static void WriteChannel11(uint8_t *payload, uint8_t channelIndex,
                             uint16_t value);

  /**
   * @brief 根据当前缓存头部刷新预期包长。
   */
  void RefreshExpectedPacketSize();

  /**
   * @brief 消费缓存前部若干字节。
   *
   * @param count 需要消费的字节数。
   */
  void ConsumeLeadingBytes(uint8_t count);

  /**
   * @brief 丢弃缓存前部若干字节。
   *
   * @param count 需要丢弃的字节数。
   */
  void DropLeadingBytes(uint8_t count);

  uint8_t buffer_[kMaxPacketSize] {}; /**< 流式解析缓冲区。 */
  uint8_t bufferedBytes_ = 0U; /**< 当前缓存字节数。 */
  uint8_t expectedPacketSize_ = 0U; /**< 当前预期包长。 */
  ParseStats stats_ {}; /**< 解析统计信息。 */
};

} // namespace iFly

#endif /* IFLY_APP_PROTO_CRSF_PROTOCOL_HPP */
