#ifndef IFLY_APP_PROTO_SBUS_PROTOCOL_HPP
#define IFLY_APP_PROTO_SBUS_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

struct SbusFrame final {
  static constexpr uint8_t kAnalogChannelCount = 16U;

  uint16_t channels[kAnalogChannelCount] {};
  bool channel17 = false;
  bool channel18 = false;
  bool frameLost = false;
  bool failsafe = false;
  uint8_t footer = 0x00U;
};

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

  void Reset();

  uint32_t Parse(const uint8_t *data,
                 uint32_t length,
                 SbusFrame *outFrames,
                 uint32_t maxFrames);

  bool Encode(const SbusFrame &frame,
              uint8_t *outFrame,
              uint32_t outLength = kFrameSize) const;

  const ParseStats &Stats() const {
    return stats_;
  }

  static bool TryDecodeFrame(const uint8_t *rawFrame,
                             uint32_t rawLength,
                             SbusFrame *frame);

  static bool IsValidFrame(const uint8_t *rawFrame, uint32_t rawLength);
  static bool IsValidFooter(uint8_t footer);

private:
  static constexpr uint8_t kPayloadOffset = 1U;
  static constexpr uint8_t kFlagsOffset = 23U;
  static constexpr uint8_t kFooterOffset = 24U;

  static uint16_t ReadChannel(const uint8_t *payload, uint8_t channelIndex);
  static void WriteChannel(uint8_t *payload, uint8_t channelIndex, uint16_t value);

  void DropUntilNextCandidate();

private:
  uint8_t buffer_[kFrameSize] {};
  uint8_t bufferedBytes_ = 0U;
  ParseStats stats_ {};
};

} // namespace iFly::Sbus

#endif /* IFLY_APP_PROTO_SBUS_PROTOCOL_HPP */
