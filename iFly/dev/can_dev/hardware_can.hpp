#ifndef IFLY_HARDWARE_CAN_HPP
#define IFLY_HARDWARE_CAN_HPP

#include <stdint.h>

#include "can.hpp"
#include "serial_io_base.hpp"

namespace iFly {

class HardwareCan final : public SerialIoBase {
public:
  explicit HardwareCan(CanPortId port,
                       uint32_t rxQueueStorageSize = kDefaultRxQueueStorageSize) noexcept;

  void Init() override;
  uint32_t Write(const uint8_t *data, uint32_t len) override;
  uint32_t TxFree() const override;
  uint32_t TxUsed() const override;
  uint32_t RxDropped() const override;
  bool IsConnected() const override;

  CanPortId Port() const noexcept;

  bool WriteFrame(const CanFramePacket &frame);
  bool ReadFrame(CanFramePacket *frame);

  uint32_t Available();
  uint32_t RxFree();
  uint32_t RxUsed();

private:
  static CanService &Device() noexcept;
  void BeforeRead() override;

private:
  CanPortId port_;
};

using can_port = HardwareCan;

} // namespace iFly

#endif /* IFLY_HARDWARE_CAN_HPP */
