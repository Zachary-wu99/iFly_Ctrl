#include "sbus_protocol.hpp"

#include <string.h>

namespace iFly {

namespace {

constexpr uint16_t kAnalogChannelMask = 0x07FFU;

} // namespace

void SbusProtocol::Reset()
{
  (void)memset(buffer_, 0, sizeof(buffer_));
  bufferedBytes_ = 0U;
  stats_ = ParseStats {};
}

uint32_t SbusProtocol::Parse(const uint8_t *data,
                             uint32_t length,
                             SbusFrame *outFrames,
                             uint32_t maxFrames)
{
  if ((data == nullptr) || (length == 0U)) {
    return 0U;
  }

  stats_.bytesReceived += length;
  uint32_t delivered = 0U;

  for (uint32_t index = 0U; index < length; ++index) {
    const uint8_t byte = data[index];

    if ((bufferedBytes_ == 0U) && (byte != kHeaderByte)) {
      ++stats_.droppedBytes;
      continue;
    }

    buffer_[bufferedBytes_++] = byte;
    if (bufferedBytes_ < kFrameSize) {
      continue;
    }

    while (bufferedBytes_ == kFrameSize) {
      SbusFrame frame {};
      if (TryDecodeFrame(buffer_, kFrameSize, &frame)) {
        ++stats_.framesDecoded;
        if ((outFrames != nullptr) && (delivered < maxFrames)) {
          outFrames[delivered++] = frame;
          ++stats_.framesDelivered;
        } else {
          ++stats_.framesDropped;
        }

        bufferedBytes_ = 0U;
        break;
      }

      ++stats_.invalidFrames;
      DropUntilNextCandidate();
    }
  }

  return delivered;
}

bool SbusProtocol::Encode(const SbusFrame &frame,
                          uint8_t *outFrame,
                          uint32_t outLength) const
{
  if ((outFrame == nullptr) || (outLength < kFrameSize)) {
    return false;
  }

  (void)memset(outFrame, 0, kFrameSize);
  outFrame[0] = kHeaderByte;

  uint8_t *payload = outFrame + kPayloadOffset;
  for (uint8_t channelIndex = 0U; channelIndex < SbusFrame::kAnalogChannelCount; ++channelIndex) {
    WriteChannel(payload, channelIndex, frame.channels[channelIndex]);
  }

  uint8_t flags = 0U;
  if (frame.channel17) {
    flags |= kChannel17Mask;
  }
  if (frame.channel18) {
    flags |= kChannel18Mask;
  }
  if (frame.frameLost) {
    flags |= kFrameLostMask;
  }
  if (frame.failsafe) {
    flags |= kFailsafeMask;
  }

  outFrame[kFlagsOffset] = flags;
  outFrame[kFooterOffset] = IsValidFooter(frame.footer) ? frame.footer : kDefaultFooterByte;
  return true;
}

bool SbusProtocol::TryDecodeFrame(const uint8_t *rawFrame,
                                  uint32_t rawLength,
                                  SbusFrame *frame)
{
  if ((frame == nullptr) || !IsValidFrame(rawFrame, rawLength)) {
    return false;
  }

  const uint8_t *payload = rawFrame + kPayloadOffset;
  for (uint8_t channelIndex = 0U; channelIndex < SbusFrame::kAnalogChannelCount; ++channelIndex) {
    frame->channels[channelIndex] = ReadChannel(payload, channelIndex);
  }

  const uint8_t flags = rawFrame[kFlagsOffset];
  frame->channel17 = (flags & kChannel17Mask) != 0U;
  frame->channel18 = (flags & kChannel18Mask) != 0U;
  frame->frameLost = (flags & kFrameLostMask) != 0U;
  frame->failsafe = (flags & kFailsafeMask) != 0U;
  frame->footer = rawFrame[kFooterOffset];
  return true;
}

bool SbusProtocol::IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength)
{
  if ((rawFrame == nullptr) || (rawLength < kFrameSize)) {
    return false;
  }

  if (rawFrame[0] != kHeaderByte) {
    return false;
  }

  if ((rawFrame[kFlagsOffset] & 0xF0U) != 0U) {
    return false;
  }

  return IsValidFooter(rawFrame[kFooterOffset]);
}

bool SbusProtocol::IsValidFooter(uint8_t footer)
{
  switch (footer) {
    case 0x00U:
    case 0x04U:
    case 0x14U:
    case 0x24U:
    case 0x34U:
      return true;
    default:
      return false;
  }
}

uint16_t SbusProtocol::ReadChannel(const uint8_t *payload, uint8_t channelIndex)
{
  if (payload == nullptr) {
    return 0U;
  }

  const uint16_t bitOffset = static_cast<uint16_t>(channelIndex) * 11U;
  uint16_t value = 0U;
  for (uint8_t bit = 0U; bit < 11U; ++bit) {
    const uint16_t bitIndex = bitOffset + bit;
    const uint8_t byteIndex = static_cast<uint8_t>(bitIndex >> 3U);
    const uint8_t bitMask = static_cast<uint8_t>(1U << (bitIndex & 0x07U));
    if ((payload[byteIndex] & bitMask) != 0U) {
      value |= static_cast<uint16_t>(1U << bit);
    }
  }

  return value;
}

void SbusProtocol::WriteChannel(uint8_t *payload, uint8_t channelIndex, uint16_t value)
{
  if (payload == nullptr) {
    return;
  }

  value &= kAnalogChannelMask;
  const uint16_t bitOffset = static_cast<uint16_t>(channelIndex) * 11U;
  for (uint8_t bit = 0U; bit < 11U; ++bit) {
    const uint16_t bitIndex = bitOffset + bit;
    const uint8_t byteIndex = static_cast<uint8_t>(bitIndex >> 3U);
    const uint8_t bitMask = static_cast<uint8_t>(1U << (bitIndex & 0x07U));

    if ((value & static_cast<uint16_t>(1U << bit)) != 0U) {
      payload[byteIndex] = static_cast<uint8_t>(payload[byteIndex] | bitMask);
    } else {
      payload[byteIndex] = static_cast<uint8_t>(payload[byteIndex] & static_cast<uint8_t>(~bitMask));
    }
  }
}

void SbusProtocol::DropUntilNextCandidate()
{
  uint8_t shift = bufferedBytes_;
  for (uint8_t index = 1U; index < bufferedBytes_; ++index) {
    if (buffer_[index] == kHeaderByte) {
      shift = index;
      break;
    }
  }

  if (shift >= bufferedBytes_) {
    stats_.droppedBytes += bufferedBytes_;
    bufferedBytes_ = 0U;
    ++stats_.resyncCount;
    return;
  }

  const uint8_t remaining = static_cast<uint8_t>(bufferedBytes_ - shift);
  (void)memmove(buffer_, buffer_ + shift, remaining);
  bufferedBytes_ = remaining;
  stats_.droppedBytes += shift;
  ++stats_.resyncCount;
}

} // namespace iFly::Sbus
