#ifndef IFLY_APP_PROTO_CRSF_PROTOCOL_HPP
#define IFLY_APP_PROTO_CRSF_PROTOCOL_HPP

#include <stdint.h>

namespace iFly {

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

struct CrsfRcChannelsPacked final {
  static constexpr uint8_t kChannelCount = 16U;

  uint16_t channels[kChannelCount] {};
};

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

private:
  uint8_t buffer_[kMaxPacketSize] {};
  uint8_t bufferedBytes_ = 0U;
  uint8_t expectedPacketSize_ = 0U;
  ParseStats stats_ {};
};

} // namespace iFly::Crsf

#endif /* IFLY_APP_PROTO_CRSF_PROTOCOL_HPP */
