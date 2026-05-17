/**
 * @file sbus_protocol.hpp
 * @brief SBUS 协议接口。
 */
#ifndef IFLY_APP_PROTO_SBUS_PROTOCOL_HPP
#define IFLY_APP_PROTO_SBUS_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 单帧 SBUS 解码结果。
 */
struct SbusFrame final {
  static constexpr uint8_t kAnalogChannelCount = 16U; /**< 模拟通道数量。 */

  uint16_t channels[kAnalogChannelCount] {}; /**< 16 路模拟通道值。 */
  bool channel17 = false; /**< 第 17 路开关通道状态。 */
  bool channel18 = false; /**< 第 18 路开关通道状态。 */
  bool frameLost = false; /**< 当前帧是否标记为丢帧。 */
  bool failsafe = false; /**< 当前帧是否处于 failsafe 状态。 */
  uint8_t footer = 0x00U; /**< 帧尾字节。 */
};

/**
 * @brief SBUS 协议处理器。
 */
class SbusProtocol final {
public:
  static constexpr uint8_t kFrameSize = 25U; /**< 标准帧长度。 */
  static constexpr uint8_t kPayloadSize = 22U; /**< 有效负载长度。 */
  static constexpr uint8_t kHeaderByte = 0x0FU; /**< 帧头字节。 */
  static constexpr uint8_t kDefaultFooterByte = 0x00U; /**< 默认帧尾字节。 */

  static constexpr uint8_t kChannel17Mask = 0x01U; /**< 通道 17 标志位。 */
  static constexpr uint8_t kChannel18Mask = 0x02U; /**< 通道 18 标志位。 */
  static constexpr uint8_t kFrameLostMask = 0x04U; /**< 丢帧标志位。 */
  static constexpr uint8_t kFailsafeMask = 0x08U; /**< failsafe 标志位。 */

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
    uint32_t resyncCount = 0U; /**< 重新同步次数。 */
  };

  SbusProtocol() = default;

  /**
   * @brief 重置协议内部状态。
   */
  void Reset();

  /**
   * @brief 解析输入字节流并输出完成的 SBUS 帧。
   *
   * @param data 输入数据首地址。
   * @param length 输入数据长度。
   * @param outFrames 输出帧缓冲区。
   * @param maxFrames 输出缓冲区最大帧数。
   * @return 实际输出的帧数。
   */
  uint32_t Parse(const uint8_t *data, uint32_t length, SbusFrame *outFrames,
                 uint32_t maxFrames);

  /**
   * @brief 将结构化帧数据编码为 SBUS 原始帧。
   *
   * @param frame 待编码的 SBUS 帧。
   * @param outFrame 输出缓冲区。
   * @param outLength 输出缓冲区长度。
   * @return 编码成功返回 `true`。
   */
  bool Encode(const SbusFrame &frame, uint8_t *outFrame,
              uint32_t outLength = kFrameSize) const;

  /**
   * @brief 获取当前解析统计信息。
   *
   * @return 统计信息只读引用。
   */
  const ParseStats &Stats() const {
    return stats_;
  }

  /**
   * @brief 尝试解码单帧 SBUS 原始数据。
   *
   * @param rawFrame 原始帧首地址。
   * @param rawLength 原始帧长度。
   * @param frame 输出帧对象。
   * @return 解码成功返回 `true`。
   */
  static bool TryDecodeFrame(const uint8_t *rawFrame, uint32_t rawLength,
                             SbusFrame *frame);

  /**
   * @brief 判断原始帧是否为合法 SBUS 帧。
   *
   * @param rawFrame 原始帧首地址。
   * @param rawLength 原始帧长度。
   * @return 合法返回 `true`。
   */
  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);

  /**
   * @brief 判断帧尾字节是否有效。
   *
   * @param footer 帧尾字节。
   * @return 有效返回 `true`。
   */
  static bool IsValidFooter(uint8_t footer);

private:
  static constexpr uint8_t kPayloadOffset = 1U; /**< 负载起始偏移。 */
  static constexpr uint8_t kFlagsOffset = 23U; /**< 标志字节偏移。 */
  static constexpr uint8_t kFooterOffset = 24U; /**< 帧尾字节偏移。 */

  /**
   * @brief 从负载中读取指定通道值。
   *
   * @param payload SBUS 负载首地址。
   * @param channelIndex 通道索引。
   * @return 解码后的通道值。
   */
  static uint16_t ReadChannel(const uint8_t *payload, uint8_t channelIndex);

  /**
   * @brief 将指定通道值写入负载。
   *
   * @param payload SBUS 负载首地址。
   * @param channelIndex 通道索引。
   * @param value 待写入的通道值。
   */
  static void WriteChannel(uint8_t *payload, uint8_t channelIndex,
                           uint16_t value);

  /**
   * @brief 丢弃缓存中的无效前导数据，直到下一个候选帧头。
   */
  void DropUntilNextCandidate();

  uint8_t buffer_[kFrameSize] {}; /**< 流式解析缓冲区。 */
  uint8_t bufferedBytes_ = 0U; /**< 当前缓存字节数。 */
  ParseStats stats_ {}; /**< 解析统计信息。 */
};

} // namespace iFly

#endif /* IFLY_APP_PROTO_SBUS_PROTOCOL_HPP */

