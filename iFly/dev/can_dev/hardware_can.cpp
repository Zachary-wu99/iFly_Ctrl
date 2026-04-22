#include "hardware_can.hpp"

namespace iFly {

HardwareCan::HardwareCan(CanPortId port, uint32_t rxQueueStorageSize)
    : SerialIoBase(rxQueueStorageSize), port_(port) {
}

void HardwareCan::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  (void)Device().InitPort(port_, RxQueue());
}

uint32_t HardwareCan::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(port_, data, len);
}

uint32_t HardwareCan::TxFree() const {
  return Device().TxFree(port_);
}

uint32_t HardwareCan::TxUsed() const {
  return Device().TxUsed(port_);
}

uint32_t HardwareCan::RxDropped() const {
  return Device().RxDropped(port_);
}

bool HardwareCan::IsConnected() const {
  return Device().IsReady(port_);
}

CanPortId HardwareCan::Port() const {
  return port_;
}

bool HardwareCan::WriteFrame(const CanFramePacket &frame) {
  return Device().WriteFrame(port_, frame);
}

bool HardwareCan::ReadFrame(CanFramePacket *frame) {
  if (frame == nullptr) {
    return false;
  }

  BeforeRead();
  if (SerialIoBase::Available() < sizeof(CanFramePacket)) {
    return false;
  }

  return Dequeue(reinterpret_cast<uint8_t *>(frame), sizeof(CanFramePacket)) == sizeof(CanFramePacket);
}

uint32_t HardwareCan::Available() {
  BeforeRead();
  return SerialIoBase::Available();
}

uint32_t HardwareCan::RxFree() {
  BeforeRead();
  return SerialIoBase::RxFree();
}

uint32_t HardwareCan::RxUsed() {
  BeforeRead();
  return SerialIoBase::RxUsed();
}

CanService &HardwareCan::Device() {
  return CanService::Instance();
}

void HardwareCan::BeforeRead() {
  (void)port_;
}

} // namespace iFly
