// CRSF 协议实现。
// 负责字节流同步、CRC 校验、帧拆包和 RC 通道编解码。
#include "crsf_protocol.hpp"

#include <string.h>

namespace iFly {

namespace {

constexpr uint16_t k11BitMask = 0x07FFU;

} // namespace

// 重置内部运行状态。
void CrsfProtocol::Reset()
{
  (void)memset(buffer_, 0, sizeof(buffer_));
  bufferedBytes_ = 0U;
  expectedPacketSize_ = 0U;
  stats_ = ParseStats {};
}

// 解析输入字节流并输出已完成的帧。
uint32_t CrsfProtocol::Parse(const uint8_t *data,
                             uint32_t length,
                             CrsfFrame *outFrames,
                             uint32_t maxFrames)
{
  if ((data == nullptr) || (length == 0U)) {
    return 0U;
  }

  stats_.bytesReceived += length;
  uint32_t delivered = 0U;

  for (uint32_t index = 0U; index < length; ++index) {
    if (bufferedBytes_ >= kMaxPacketSize) {
      DropLeadingBytes(1U);
    }

    buffer_[bufferedBytes_++] = data[index];
    RefreshExpectedPacketSize();

    while ((expectedPacketSize_ != 0U) && (bufferedBytes_ >= expectedPacketSize_)) {
      CrsfFrame frame {};
      if (TryDecodeFrame(buffer_, expectedPacketSize_, &frame)) {
        ++stats_.framesDecoded;
        if ((outFrames != nullptr) && (delivered < maxFrames)) {
          outFrames[delivered++] = frame;
          ++stats_.framesDelivered;
        } else {
          ++stats_.framesDropped;
        }

        ConsumeLeadingBytes(expectedPacketSize_);
        continue;
      }

      ++stats_.invalidFrames;
      ++stats_.crcErrors;
      DropLeadingBytes(1U);
    }
  }

  return delivered;
}

// 把结构化数据编码为协议帧。
bool CrsfProtocol::Encode(const CrsfFrame &frame,
                          uint8_t *outFrame,
                          uint32_t outCapacity,
                          uint32_t *writtenLength) const
{
  if (outFrame == nullptr) {
    return false;
  }

  const bool extended = frame.extended || IsExtendedType(frame.type);
  const uint8_t maxPayloadLength = extended ? 58U : CrsfFrame::kMaxPayloadSize;
  if (frame.payloadLength > maxPayloadLength) {
    return false;
  }

  const uint8_t frameLength =
      static_cast<uint8_t>(frame.payloadLength + (extended ? 4U : 2U));
  const uint8_t packetSize = ComputeExpectedPacketSize(frameLength);
  if ((packetSize == 0U) || (outCapacity < packetSize)) {
    return false;
  }

  (void)memset(outFrame, 0, packetSize);
  outFrame[0] = frame.deviceAddress;
  outFrame[1] = frameLength;
  outFrame[2] = frame.type;

  uint8_t payloadOffset = 3U;
  if (extended) {
    outFrame[payloadOffset++] = frame.destination;
    outFrame[payloadOffset++] = frame.source;
  }

  if (frame.payloadLength > 0U) {
    (void)memcpy(outFrame + payloadOffset, frame.payload, frame.payloadLength);
  }

  outFrame[packetSize - 1U] = ComputeCrc(outFrame + 2U, static_cast<uint32_t>(frameLength - 1U));
  if (writtenLength != nullptr) {
    *writtenLength = packetSize;
  }
  return true;
}

// 尝试解析一帧完整报文。
bool CrsfProtocol::TryDecodeFrame(const uint8_t *rawFrame,
                                  uint32_t rawLength,
                                  CrsfFrame *frame)
{
  if ((frame == nullptr) || !IsValidFrame(rawFrame, rawLength)) {
    return false;
  }

  frame->deviceAddress = rawFrame[0];
  frame->type = rawFrame[2];
  frame->extended = IsExtendedType(frame->type);

  uint8_t payloadOffset = 3U;
  uint8_t payloadLength = static_cast<uint8_t>(rawFrame[1] - 2U);

  if (frame->extended) {
    frame->destination = rawFrame[payloadOffset++];
    frame->source = rawFrame[payloadOffset++];
    payloadLength = static_cast<uint8_t>(payloadLength - 2U);
  } else {
    frame->destination = 0U;
    frame->source = 0U;
  }

  frame->payloadLength = payloadLength;
  if (payloadLength > 0U) {
    (void)memcpy(frame->payload, rawFrame + payloadOffset, payloadLength);
  }
  if (payloadLength < CrsfFrame::kMaxPayloadSize) {
    (void)memset(frame->payload + payloadLength, 0, CrsfFrame::kMaxPayloadSize - payloadLength);
  }

  return true;
}

// 校验当前报文格式是否合法。
bool CrsfProtocol::IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength)
{
  if ((rawFrame == nullptr) || (rawLength < kMinPacketSize) || (rawLength > kMaxPacketSize)) {
    return false;
  }

  const uint8_t frameLength = rawFrame[1];
  const uint8_t packetSize = ComputeExpectedPacketSize(frameLength);
  if ((packetSize == 0U) || (rawLength != packetSize)) {
    return false;
  }

  const uint8_t type = rawFrame[2];
  if (!IsExtendedType(type) && (frameLength < 2U)) {
    return false;
  }
  if (IsExtendedType(type) && (frameLength < 4U)) {
    return false;
  }

  const uint8_t actualCrc = rawFrame[packetSize - 1U];
  const uint8_t expectedCrc = ComputeCrc(rawFrame + 2U, static_cast<uint32_t>(frameLength - 1U));
  return actualCrc == expectedCrc;
}

// 判断当前类型是否为扩展帧。
bool CrsfProtocol::IsExtendedType(uint8_t type)
{
  return type >= kExtendedTypeMin;
}

// 计算报文校验值。
uint8_t CrsfProtocol::ComputeCrc(const uint8_t *data, uint32_t length)
{
  if ((data == nullptr) || (length == 0U)) {
    return 0U;
  }

  uint8_t crc = 0U;
  for (uint32_t index = 0U; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x80U) != 0U) {
        crc = static_cast<uint8_t>((crc << 1U) ^ kCrcPolynomial);
      } else {
        crc = static_cast<uint8_t>(crc << 1U);
      }
    }
  }

  return crc;
}

// 解码打包的 RC 通道数据。
bool CrsfProtocol::DecodeRcChannelsPacked(const CrsfFrame &frame,
                                          CrsfRcChannelsPacked *channels)
{
  if ((channels == nullptr) || (frame.type != kRcChannelsPackedType) ||
      frame.extended || (frame.payloadLength != kRcChannelsPayloadSize)) {
    return false;
  }

  for (uint8_t channelIndex = 0U; channelIndex < CrsfRcChannelsPacked::kChannelCount; ++channelIndex) {
    channels->channels[channelIndex] = ReadChannel11(frame.payload, channelIndex);
  }

  return true;
}

// 编码打包的 RC 通道数据。
bool CrsfProtocol::EncodeRcChannelsPacked(uint8_t deviceAddress,
                                          const CrsfRcChannelsPacked &channels,
                                          uint8_t *outFrame,
                                          uint32_t outCapacity,
                                          uint32_t *writtenLength)
{
  CrsfFrame frame {};
  frame.deviceAddress = deviceAddress;
  frame.type = kRcChannelsPackedType;
  frame.extended = false;
  frame.payloadLength = kRcChannelsPayloadSize;
  (void)memset(frame.payload, 0, sizeof(frame.payload));

  for (uint8_t channelIndex = 0U; channelIndex < CrsfRcChannelsPacked::kChannelCount; ++channelIndex) {
    WriteChannel11(frame.payload, channelIndex, channels.channels[channelIndex]);
  }

  return CrsfProtocol {}.Encode(frame, outFrame, outCapacity, writtenLength);
}

// 根据长度字段推算期望报文长度。
uint8_t CrsfProtocol::ComputeExpectedPacketSize(uint8_t frameLength)
{
  if ((frameLength < kMinFrameLength) || (frameLength > kMaxFrameLength)) {
    return 0U;
  }

  return static_cast<uint8_t>(frameLength + 2U);
}

// 从打包负载中读取一个 11 位通道值。
uint16_t CrsfProtocol::ReadChannel11(const uint8_t *payload, uint8_t channelIndex)
{
  if (payload == nullptr) {
    return 0U;
  }

  const uint16_t bitOffset = static_cast<uint16_t>(channelIndex) * 11U;
  uint16_t value = 0U;
  for (uint8_t bit = 0U; bit < 11U; ++bit) {
    const uint16_t bitIndex = static_cast<uint16_t>(bitOffset + bit);
    const uint8_t byteIndex = static_cast<uint8_t>(bitIndex >> 3U);
    const uint8_t bitMask = static_cast<uint8_t>(1U << (bitIndex & 0x07U));
    if ((payload[byteIndex] & bitMask) != 0U) {
      value |= static_cast<uint16_t>(1U << bit);
    }
  }

  return value;
}

// 把一个 11 位通道值写入打包负载。
void CrsfProtocol::WriteChannel11(uint8_t *payload, uint8_t channelIndex, uint16_t value)
{
  if (payload == nullptr) {
    return;
  }

  value &= k11BitMask;
  const uint16_t bitOffset = static_cast<uint16_t>(channelIndex) * 11U;
  for (uint8_t bit = 0U; bit < 11U; ++bit) {
    const uint16_t bitIndex = static_cast<uint16_t>(bitOffset + bit);
    const uint8_t byteIndex = static_cast<uint8_t>(bitIndex >> 3U);
    const uint8_t bitMask = static_cast<uint8_t>(1U << (bitIndex & 0x07U));

    if ((value & static_cast<uint16_t>(1U << bit)) != 0U) {
      payload[byteIndex] = static_cast<uint8_t>(payload[byteIndex] | bitMask);
    } else {
      payload[byteIndex] = static_cast<uint8_t>(payload[byteIndex] & static_cast<uint8_t>(~bitMask));
    }
  }
}

// 根据缓存头部刷新期望报文长度。
void CrsfProtocol::RefreshExpectedPacketSize()
{
  expectedPacketSize_ = 0U;
  while (bufferedBytes_ >= 2U) {
    expectedPacketSize_ = ComputeExpectedPacketSize(buffer_[1]);
    if (expectedPacketSize_ != 0U) {
      return;
    }

    DropLeadingBytes(1U);
  }
}

// 丢弃缓存头部若干字节。
void CrsfProtocol::DropLeadingBytes(uint8_t count)
{
  if ((count == 0U) || (bufferedBytes_ == 0U)) {
    return;
  }

  const uint8_t dropCount = (count < bufferedBytes_) ? count : bufferedBytes_;
  const uint8_t remaining = static_cast<uint8_t>(bufferedBytes_ - dropCount);
  if (remaining > 0U) {
    (void)memmove(buffer_, buffer_ + dropCount, remaining);
  }

  bufferedBytes_ = remaining;
  expectedPacketSize_ = 0U;
  stats_.droppedBytes += dropCount;
  ++stats_.resyncCount;
}

// 消费缓存头部若干字节。
void CrsfProtocol::ConsumeLeadingBytes(uint8_t count)
{
  if ((count == 0U) || (bufferedBytes_ == 0U)) {
    return;
  }

  const uint8_t consumeCount = (count < bufferedBytes_) ? count : bufferedBytes_;
  const uint8_t remaining = static_cast<uint8_t>(bufferedBytes_ - consumeCount);
  if (remaining > 0U) {
    (void)memmove(buffer_, buffer_ + consumeCount, remaining);
  }

  bufferedBytes_ = remaining;
  expectedPacketSize_ = 0U;
}

} // namespace iFly::Crsf
