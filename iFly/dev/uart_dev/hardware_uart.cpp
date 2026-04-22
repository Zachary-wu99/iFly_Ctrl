#include "hardware_uart.hpp"

namespace iFly {

HardwareUart::HardwareUart(UartPortId port, uint32_t rxQueueStorageSize)
    : SerialIoBase(rxQueueStorageSize), port_(port) {
}

void HardwareUart::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  (void)Device().InitPort(port_, RxQueue());
}

uint32_t HardwareUart::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(port_, data, len);
}

uint32_t HardwareUart::TxFree() const {
  return Device().TxFree(port_);
}

uint32_t HardwareUart::TxUsed() const {
  return Device().TxUsed(port_);
}

uint32_t HardwareUart::RxDropped() const {
  return Device().RxDropped(port_);
}

bool HardwareUart::IsConnected() const {
  return Device().IsReady(port_);
}

UartPortId HardwareUart::Port() const {
  return port_;
}

UartDmaService &HardwareUart::Device() {
  return UartDmaService::Instance();
}

} // namespace iFly
