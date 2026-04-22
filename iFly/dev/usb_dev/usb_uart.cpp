#include "usb_uart.hpp"

namespace iFly {

UsbUart::UsbUart(uint32_t rxQueueStorageSize) : SerialIoBase(rxQueueStorageSize) {
}

void UsbUart::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  Device().AttachRxQueue(RxQueue());
  Device().Init();
}

uint32_t UsbUart::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(data, len);
}

uint32_t UsbUart::TxFree() const {
  return Device().TxFree();
}

uint32_t UsbUart::TxUsed() const {
  return Device().TxUsed();
}

uint32_t UsbUart::RxDropped() const {
  return Device().RxDropped();
}

bool UsbUart::IsConnected() const {
  return Device().IsConfigured();
}

UsbCdcAcm &UsbUart::Device() {
  return UsbCdcAcm::Instance();
}

void UsbUart::BeforeRead() {
  Device().Service();
}

} // namespace iFly
