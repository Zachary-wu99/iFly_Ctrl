#include "usb_cdc.hpp"

#include <string.h>

#include "main.h"
#include "platform_handle.hpp"
#include "usermath.hpp"
#include "usb_otg.h"

namespace {

constexpr uint8_t kReqTypeMask = 0x60U;
constexpr uint8_t kReqTypeStandard = 0x00U;
constexpr uint8_t kReqTypeClass = 0x20U;

constexpr uint8_t kRecipientMask = 0x1FU;
constexpr uint8_t kRecipientInterface = 0x01U;
constexpr uint8_t kRecipientEndpoint = 0x02U;

constexpr uint8_t kReqGetStatus = 0x00U;
constexpr uint8_t kReqClearFeature = 0x01U;
constexpr uint8_t kReqSetFeature = 0x03U;
constexpr uint8_t kReqSetAddress = 0x05U;
constexpr uint8_t kReqGetDescriptor = 0x06U;
constexpr uint8_t kReqGetConfiguration = 0x08U;
constexpr uint8_t kReqSetConfiguration = 0x09U;
constexpr uint8_t kReqGetInterface = 0x0AU;
constexpr uint8_t kReqSetInterface = 0x0BU;

constexpr uint8_t kCdcReqSetLineCoding = 0x20U;
constexpr uint8_t kCdcReqGetLineCoding = 0x21U;
constexpr uint8_t kCdcReqSetControlLineState = 0x22U;

constexpr uint8_t kDescTypeDevice = 0x01U;
constexpr uint8_t kDescTypeConfiguration = 0x02U;
constexpr uint8_t kDescTypeString = 0x03U;

constexpr uint8_t kFeatureEndpointHalt = 0x00U;
constexpr uint8_t kEndpoint0Out = 0x00U;
constexpr uint8_t kEndpoint0In = 0x80U;
constexpr uint16_t kCdcConfigValue = 0x0001U;

class Stm32FsPcdAdapter final {
public:
  void *Handle() const {
    return static_cast<void *>(&hpcd_USB_OTG_FS);
  }

  bool Matches(void *hpcd) const {
    return hpcd == Handle();
  }

  void ConfigureFifos() const {
    PCD_HandleTypeDef *hpcd = iFly::platform::AsPcdHandle(Handle());
    (void)HAL_PCDEx_SetRxFiFo(hpcd, 128U);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 0U, 64U);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 1U, 64U);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 2U, 16U);
  }

  HAL_StatusTypeDef Start() const {
    return HAL_PCD_Start(iFly::platform::AsPcdHandle(Handle()));
  }

  HAL_StatusTypeDef OpenEndpoint(uint8_t epAddr, uint16_t mps, uint8_t epType) const {
    return HAL_PCD_EP_Open(iFly::platform::AsPcdHandle(Handle()), epAddr, mps, epType);
  }

  HAL_StatusTypeDef CloseEndpoint(uint8_t epAddr) const {
    return HAL_PCD_EP_Close(iFly::platform::AsPcdHandle(Handle()), epAddr);
  }

  HAL_StatusTypeDef Receive(uint8_t epAddr, uint8_t *buffer, uint32_t length) const {
    return HAL_PCD_EP_Receive(iFly::platform::AsPcdHandle(Handle()), epAddr, buffer, length);
  }

  HAL_StatusTypeDef Transmit(uint8_t epAddr, uint8_t *buffer, uint32_t length) const {
    return HAL_PCD_EP_Transmit(iFly::platform::AsPcdHandle(Handle()), epAddr, buffer, length);
  }

  uint32_t GetRxCount(uint8_t epAddr) const {
    return HAL_PCD_EP_GetRxCount(iFly::platform::AsPcdHandle(Handle()), epAddr);
  }

  void SetAddress(uint8_t address) const {
    (void)HAL_PCD_SetAddress(iFly::platform::AsPcdHandle(Handle()), address);
  }

  void SetStall(uint8_t epAddr) const {
    (void)HAL_PCD_EP_SetStall(iFly::platform::AsPcdHandle(Handle()), epAddr);
  }

  void ClearStall(uint8_t epAddr) const {
    (void)HAL_PCD_EP_ClrStall(iFly::platform::AsPcdHandle(Handle()), epAddr);
  }

  const uint8_t *SetupBuffer() const {
    return reinterpret_cast<const uint8_t *>(iFly::platform::AsPcdHandle(Handle())->Setup);
  }
};

Stm32FsPcdAdapter &UsbPcd() {
  static Stm32FsPcdAdapter adapter;
  return adapter;
}

const uint8_t kDeviceDescriptor[] = {
  0x12U,
  0x01U,
  0x00U, 0x02U,
  0x02U,
  0x02U,
  0x00U,
  0x40U,
  0x83U, 0x04U,
  0x40U, 0x57U,
  0x00U, 0x02U,
  0x01U,
  0x02U,
  0x03U,
  0x01U
};

const uint8_t kConfigurationDescriptor[] = {
  0x09U, 0x02U, 0x43U, 0x00U, 0x02U, 0x01U, 0x00U, 0x80U, 0x32U,
  0x09U, 0x04U, 0x00U, 0x00U, 0x01U, 0x02U, 0x02U, 0x01U, 0x00U,
  0x05U, 0x24U, 0x00U, 0x10U, 0x01U,
  0x05U, 0x24U, 0x01U, 0x00U, 0x01U,
  0x04U, 0x24U, 0x02U, 0x02U,
  0x05U, 0x24U, 0x06U, 0x00U, 0x01U,
  0x07U, 0x05U, 0x82U, 0x03U, 0x08U, 0x00U, 0x10U,
  0x09U, 0x04U, 0x01U, 0x00U, 0x02U, 0x0AU, 0x00U, 0x00U, 0x00U,
  0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U, 0x00U,
  0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U
};

const uint8_t kLangIdStringDescriptor[] = {
  0x04U, 0x03U, 0x09U, 0x04U
};

const uint8_t kManufacturerStringDescriptor[] = {
  0x0AU, 0x03U, 'i', 0x00U, 'F', 0x00U, 'l', 0x00U, 'y', 0x00U
};

const uint8_t kProductStringDescriptor[] = {
  0x22U, 0x03U,
  'i', 0x00U, 'F', 0x00U, 'l', 0x00U, 'y', 0x00U, ' ', 0x00U,
  'U', 0x00U, 'S', 0x00U, 'B', 0x00U, ' ', 0x00U,
  'C', 0x00U, 'D', 0x00U, 'C', 0x00U, ' ', 0x00U,
  'A', 0x00U, 'C', 0x00U, 'M', 0x00U
};

const uint8_t kSerialStringDescriptor[] = {
  0x1AU, 0x03U,
  'I', 0x00U, 'F', 0x00U, 'L', 0x00U, 'Y', 0x00U, '0', 0x00U,
  '0', 0x00U, '0', 0x00U, '1', 0x00U, '2', 0x00U, '0', 0x00U,
  '2', 0x00U, '6', 0x00U
};

} // namespace

namespace iFly {

bool UsbEndpointDoubleBufferBase::Create(uint8_t *buffer0,
                                         uint8_t *buffer1,
                                         uint16_t bufferSize,
                                         uint16_t packetSize) {
  if ((buffer0 == nullptr) || (buffer1 == nullptr) ||
      (bufferSize == 0U) || (packetSize == 0U) || (packetSize > bufferSize)) {
    Delete();
    return false;
  }

  buffers_[0] = buffer0;
  buffers_[1] = buffer1;
  bufferSize_ = bufferSize;
  packetSize_ = packetSize;
  ResetLengths();
  return true;
}

void UsbEndpointDoubleBufferBase::Delete() {
  buffers_[0] = nullptr;
  buffers_[1] = nullptr;
  bufferSize_ = 0U;
  packetSize_ = 0U;
  ResetLengths();
}

void UsbEndpointDoubleBufferBase::Clear() {
  ResetLengths();
}

bool UsbEndpointDoubleBufferBase::IsCreated() const {
  return (buffers_[0] != nullptr) && (buffers_[1] != nullptr) && (packetSize_ > 0U);
}

uint16_t UsbEndpointDoubleBufferBase::PacketSize() const {
  return packetSize_;
}

uint8_t *UsbEndpointDoubleBufferBase::ActiveBuffer() {
  return IsCreated() ? buffers_[activeSlot_] : nullptr;
}

void UsbEndpointDoubleBufferBase::SetActiveLength(uint16_t length) {
  lengths_[activeSlot_] = length;
}

void UsbEndpointDoubleBufferBase::ClearActive() {
  lengths_[activeSlot_] = 0U;
}

uint8_t *UsbEndpointDoubleBufferBase::InactiveBuffer() {
  return IsCreated() ? buffers_[InactiveSlotIndex()] : nullptr;
}

uint16_t UsbEndpointDoubleBufferBase::InactiveLength() const {
  return lengths_[InactiveSlotIndex()];
}

void UsbEndpointDoubleBufferBase::SetInactiveLength(uint16_t length) {
  lengths_[InactiveSlotIndex()] = length;
}

bool UsbEndpointDoubleBufferBase::HasInactiveData() const {
  return InactiveLength() != 0U;
}

void UsbEndpointDoubleBufferBase::SwapBuffers() {
  activeSlot_ ^= 0x01U;
}

void UsbEndpointDoubleBufferBase::ResetLengths() {
  lengths_[0] = 0U;
  lengths_[1] = 0U;
  activeSlot_ = 0U;
}

uint8_t UsbEndpointDoubleBufferBase::InactiveSlotIndex() const {
  return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
}

UsbCdcAcm &UsbCdcAcm::Instance() {
  static UsbCdcAcm instance;
  return instance;
}

UsbCdcAcm::UsbCdcAcm() {
}

void UsbCdcAcm::Init() {
  if ((txQueue_ == nullptr) || (rxEndpointBuffer_ == nullptr) || (txEndpointBuffer_ == nullptr) ||
      (!txQueue_->IsCreated()) || (!rxEndpointBuffer_->IsCreated()) || (!txEndpointBuffer_->IsCreated())) {
    return;
  }

  if (initialized_.load(std::memory_order_acquire)) {
    ServiceTxPath();
    return;
  }

  ResetRuntimeState();
  UsbPcd().ConfigureFifos();
  (void)UsbPcd().Start();
  initialized_.store(true, std::memory_order_release);
}

void UsbCdcAcm::AttachRxQueue(LockFreeQueueBase *queue) {
  appRxQueue_ = queue;

  if ((appRxQueue_ != nullptr) && appRxQueue_->IsCreated()) {
    appRxQueue_->Clear();
  }
}

void UsbCdcAcm::AttachStorage(LockFreeQueueBase *txQueue,
                              UsbEndpointDoubleBufferBase *rxEndpointBuffer,
                              UsbEndpointDoubleBufferBase *txEndpointBuffer) {
  txQueue_ = txQueue;
  rxEndpointBuffer_ = rxEndpointBuffer;
  txEndpointBuffer_ = txEndpointBuffer;
}

void UsbCdcAcm::Service() {
  ServiceTxPath();
}

uint32_t UsbCdcAcm::Write(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U) || (txQueue_ == nullptr) || (!txQueue_->IsCreated())) {
    return 0U;
  }

  const uint32_t accepted = txQueue_->Enqueue(data, len);
  ServiceTxPath();
  return accepted;
}

uint32_t UsbCdcAcm::Read(uint8_t *data, uint32_t len) {
  if ((appRxQueue_ == nullptr) || (!appRxQueue_->IsCreated())) {
    return 0U;
  }

  return appRxQueue_->Dequeue(data, len);
}

uint32_t UsbCdcAcm::Available() const {
  return UpperRxUsed();
}

uint32_t UsbCdcAcm::TxUsed() const {
  return ((txQueue_ != nullptr) && txQueue_->IsCreated()) ? txQueue_->UsedSize() : 0U;
}

uint32_t UsbCdcAcm::TxFree() const {
  return ((txQueue_ != nullptr) && txQueue_->IsCreated()) ? txQueue_->FreeSize() : 0U;
}

uint32_t UsbCdcAcm::RxUsed() const {
  return UpperRxUsed();
}

uint32_t UsbCdcAcm::RxFree() const {
  return UpperRxFree();
}

uint32_t UsbCdcAcm::RxDropped() const {
  return rxDropped_.load(std::memory_order_acquire);
}

bool UsbCdcAcm::IsConfigured() const {
  return initialized_.load(std::memory_order_acquire) &&
         configured_.load(std::memory_order_acquire) &&
         !suspended_.load(std::memory_order_acquire);
}

void UsbCdcAcm::ResetRuntimeState() {
  configured_.store(false, std::memory_order_release);
  suspended_.store(false, std::memory_order_release);
  currentConfig_ = 0U;
  currentInterface_ = 0U;
  controlLineState_ = 0U;
  lineCoding_ = {115200U, 0U, 0U, 8U};

  ep0OutState_ = Ep0OutState::kIdle;
  ep0OutExpectedLen_ = 0U;
  ep0InPtr_ = nullptr;
  ep0InRemaining_ = 0U;
  ep0InRequestLen_ = 0U;
  ep0ZlpDummy_ = 0U;
  (void)memset(lineCodingBuffer_, 0, sizeof(lineCodingBuffer_));

  if (rxEndpointBuffer_ != nullptr) {
    rxEndpointBuffer_->Clear();
  }
  if (txQueue_ != nullptr) {
    txQueue_->Clear();
  }
  if (txEndpointBuffer_ != nullptr) {
    txEndpointBuffer_->Clear();
  }

  if ((appRxQueue_ != nullptr) && appRxQueue_->IsCreated()) {
    appRxQueue_->Clear();
  }

  txBusy_.store(false, std::memory_order_release);
  txServiceRequests_.store(0U, std::memory_order_release);
  txServiceRunning_.store(false, std::memory_order_release);
  rxDropped_.store(0U, std::memory_order_release);
}

void UsbCdcAcm::OpenControlEndpoints() {
  (void)UsbPcd().OpenEndpoint(kEndpoint0Out, kEp0Mps, EP_TYPE_CTRL);
  (void)UsbPcd().OpenEndpoint(kEndpoint0In, kEp0Mps, EP_TYPE_CTRL);
}

void UsbCdcAcm::OpenDataEndpoints() {
  (void)UsbPcd().OpenEndpoint(kEpCdcCmdIn, kEpCmdMps, EP_TYPE_INTR);
  (void)UsbPcd().OpenEndpoint(kEpCdcDataOut, kEpDataMps, EP_TYPE_BULK);
  (void)UsbPcd().OpenEndpoint(kEpCdcDataIn, kEpDataMps, EP_TYPE_BULK);
  PrimeOutEndpoint();
}

void UsbCdcAcm::CloseDataEndpoints() {
  (void)UsbPcd().CloseEndpoint(kEpCdcCmdIn);
  (void)UsbPcd().CloseEndpoint(kEpCdcDataOut);
  (void)UsbPcd().CloseEndpoint(kEpCdcDataIn);
  txBusy_.store(false, std::memory_order_release);
  if (rxEndpointBuffer_ != nullptr) {
    rxEndpointBuffer_->Clear();
  }
  if (txEndpointBuffer_ != nullptr) {
    txEndpointBuffer_->Clear();
  }
}

void UsbCdcAcm::PrimeOutEndpoint() {
  if (rxEndpointBuffer_ == nullptr) {
    return;
  }

  uint8_t *buffer = rxEndpointBuffer_->ActiveBuffer();
  if (buffer != nullptr) {
    (void)UsbPcd().Receive(kEpCdcDataOut, buffer, kEpDataMps);
  }
}

void UsbCdcAcm::OnReset() {
  ResetRuntimeState();
  UsbPcd().SetAddress(0U);
  OpenControlEndpoints();
}

void UsbCdcAcm::OnSetupStage() {
  const uint8_t *setupBuffer = UsbPcd().SetupBuffer();
  if (setupBuffer == nullptr) {
    return;
  }

  const SetupPacket setup {
    setupBuffer[0],
    setupBuffer[1],
    static_cast<uint16_t>(setupBuffer[2] | (static_cast<uint16_t>(setupBuffer[3]) << 8U)),
    static_cast<uint16_t>(setupBuffer[4] | (static_cast<uint16_t>(setupBuffer[5]) << 8U)),
    static_cast<uint16_t>(setupBuffer[6] | (static_cast<uint16_t>(setupBuffer[7]) << 8U))
  };

  const uint8_t requestType = setup.bmRequestType & kReqTypeMask;
  if (requestType == kReqTypeStandard) {
    HandleStandardRequest(setup);
    return;
  }

  if (requestType == kReqTypeClass) {
    HandleClassRequest(setup);
    return;
  }

  StallControlEndpoint();
}

void UsbCdcAcm::OnDataInStage(uint8_t epnum) {
  if (epnum == 0U) {
    ContinueControlInTransfer();
    return;
  }

  if (epnum == (kEpCdcDataIn & 0x7FU)) {
    txBusy_.store(false, std::memory_order_release);
    if (txEndpointBuffer_ != nullptr) {
      txEndpointBuffer_->ClearActive();
    }
    ServiceTxPath();
  }
}

void UsbCdcAcm::OnDataOutStage(uint8_t epnum) {
  if (epnum == 0U) {
    if (ep0OutState_ == Ep0OutState::kSetLineCoding) {
      const uint32_t rxLen = UsbPcd().GetRxCount(kEndpoint0Out);
      ep0OutState_ = Ep0OutState::kIdle;
      ep0OutExpectedLen_ = 0U;

      if (rxLen >= 7U) {
        lineCoding_.baudrate = static_cast<uint32_t>(ep0OutBuffer_[0]) |
                               (static_cast<uint32_t>(ep0OutBuffer_[1]) << 8U) |
                               (static_cast<uint32_t>(ep0OutBuffer_[2]) << 16U) |
                               (static_cast<uint32_t>(ep0OutBuffer_[3]) << 24U);
        lineCoding_.stopBits = ep0OutBuffer_[4];
        lineCoding_.parityType = ep0OutBuffer_[5];
        lineCoding_.dataBits = ep0OutBuffer_[6];
        SendControlStatus();
      } else {
        StallControlEndpoint();
      }
    }
    return;
  }

  if (epnum == (kEpCdcDataOut & 0x7FU)) {
    if (rxEndpointBuffer_ == nullptr) {
      return;
    }

    const uint8_t *completedBuffer = rxEndpointBuffer_->ActiveBuffer();
    const uint32_t rxLen = iFly::usermath::Min<uint32_t>(UsbPcd().GetRxCount(kEpCdcDataOut),
                                                         kEpDataMps);
    rxEndpointBuffer_->SetActiveLength(static_cast<uint16_t>(rxLen));
    rxEndpointBuffer_->SwapBuffers();
    rxEndpointBuffer_->ClearActive();
    PrimeOutEndpoint();
    PushReceivedPacket(completedBuffer, rxLen);
  }
}

void UsbCdcAcm::OnSuspend() {
  suspended_.store(true, std::memory_order_release);
}

void UsbCdcAcm::OnResume() {
  suspended_.store(false, std::memory_order_release);
  ServiceTxPath();
}

void UsbCdcAcm::HandleStandardRequest(const SetupPacket &setup) {
  switch (setup.bRequest) {
    case kReqGetDescriptor:
      HandleGetDescriptor(setup);
      break;

    case kReqSetAddress:
      UsbPcd().SetAddress(static_cast<uint8_t>(setup.wValue & 0x7FU));
      SendControlStatus();
      break;

    case kReqGetConfiguration: {
      const uint8_t config = currentConfig_;
      StartControlInTransfer(&config, 1U, setup.wLength);
      break;
    }

    case kReqSetConfiguration: {
      const uint16_t configValue = setup.wValue & 0x00FFU;
      if ((configValue != 0U) && (configValue != kCdcConfigValue)) {
        StallControlEndpoint();
        break;
      }

      CloseDataEndpoints();
      configured_.store(false, std::memory_order_release);
      currentConfig_ = static_cast<uint8_t>(configValue);

      if (configValue == kCdcConfigValue) {
        configured_.store(true, std::memory_order_release);
        OpenDataEndpoints();
        ServiceTxPath();
      }

      SendControlStatus();
      break;
    }

    case kReqGetInterface: {
      const uint8_t interfaceNumber = currentInterface_;
      StartControlInTransfer(&interfaceNumber, 1U, setup.wLength);
      break;
    }

    case kReqSetInterface:
      currentInterface_ = static_cast<uint8_t>(setup.wValue & 0x00FFU);
      SendControlStatus();
      break;

    case kReqGetStatus: {
      const uint8_t status[2] = {0U, 0U};
      StartControlInTransfer(status, sizeof(status), setup.wLength);
      break;
    }

    case kReqClearFeature:
    case kReqSetFeature: {
      const uint8_t recipient = setup.bmRequestType & kRecipientMask;
      if ((recipient != kRecipientEndpoint) || (setup.wValue != kFeatureEndpointHalt)) {
        StallControlEndpoint();
        break;
      }

      const uint8_t epAddr = static_cast<uint8_t>(setup.wIndex & 0x00FFU);
      if (setup.bRequest == kReqSetFeature) {
        UsbPcd().SetStall(epAddr);
      } else {
        UsbPcd().ClearStall(epAddr);
      }

      SendControlStatus();
      break;
    }

    default:
      StallControlEndpoint();
      break;
  }
}

void UsbCdcAcm::HandleClassRequest(const SetupPacket &setup) {
  const uint8_t recipient = setup.bmRequestType & kRecipientMask;
  if (recipient != kRecipientInterface) {
    StallControlEndpoint();
    return;
  }

  switch (setup.bRequest) {
    case kCdcReqSetLineCoding:
      if (setup.wLength != 7U) {
        StallControlEndpoint();
        return;
      }

      ep0OutState_ = Ep0OutState::kSetLineCoding;
      ep0OutExpectedLen_ = 7U;
      (void)UsbPcd().Receive(kEndpoint0Out, ep0OutBuffer_, ep0OutExpectedLen_);
      break;

    case kCdcReqGetLineCoding:
      lineCodingBuffer_[0] = static_cast<uint8_t>(lineCoding_.baudrate & 0xFFU);
      lineCodingBuffer_[1] = static_cast<uint8_t>((lineCoding_.baudrate >> 8U) & 0xFFU);
      lineCodingBuffer_[2] = static_cast<uint8_t>((lineCoding_.baudrate >> 16U) & 0xFFU);
      lineCodingBuffer_[3] = static_cast<uint8_t>((lineCoding_.baudrate >> 24U) & 0xFFU);
      lineCodingBuffer_[4] = lineCoding_.stopBits;
      lineCodingBuffer_[5] = lineCoding_.parityType;
      lineCodingBuffer_[6] = lineCoding_.dataBits;
      StartControlInTransfer(lineCodingBuffer_, sizeof(lineCodingBuffer_), setup.wLength);
      break;

    case kCdcReqSetControlLineState:
      controlLineState_ = static_cast<uint8_t>(setup.wValue & 0x00FFU);
      SendControlStatus();
      break;

    default:
      StallControlEndpoint();
      break;
  }
}

void UsbCdcAcm::HandleGetDescriptor(const SetupPacket &setup) {
  const uint8_t descriptorType = static_cast<uint8_t>((setup.wValue >> 8U) & 0x00FFU);
  const uint8_t descriptorIndex = static_cast<uint8_t>(setup.wValue & 0x00FFU);

  const uint8_t *descriptor = nullptr;
  uint16_t descriptorLength = 0U;

  switch (descriptorType) {
    case kDescTypeDevice:
      descriptor = kDeviceDescriptor;
      descriptorLength = sizeof(kDeviceDescriptor);
      break;

    case kDescTypeConfiguration:
      descriptor = kConfigurationDescriptor;
      descriptorLength = sizeof(kConfigurationDescriptor);
      break;

    case kDescTypeString:
      switch (descriptorIndex) {
        case 0U:
          descriptor = kLangIdStringDescriptor;
          descriptorLength = sizeof(kLangIdStringDescriptor);
          break;

        case 1U:
          descriptor = kManufacturerStringDescriptor;
          descriptorLength = sizeof(kManufacturerStringDescriptor);
          break;

        case 2U:
          descriptor = kProductStringDescriptor;
          descriptorLength = sizeof(kProductStringDescriptor);
          break;

        case 3U:
          descriptor = kSerialStringDescriptor;
          descriptorLength = sizeof(kSerialStringDescriptor);
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }

  if ((descriptor == nullptr) || (descriptorLength == 0U)) {
    StallControlEndpoint();
    return;
  }

  StartControlInTransfer(descriptor, descriptorLength, setup.wLength);
}

void UsbCdcAcm::StartControlInTransfer(const uint8_t *data, uint16_t len, uint16_t requestLen) {
  const uint16_t actualLen = iFly::usermath::Min<uint16_t>(len, requestLen);
  ep0InPtr_ = data;
  ep0InRemaining_ = actualLen;
  ep0InRequestLen_ = requestLen;

  if (actualLen == 0U) {
    (void)UsbPcd().Transmit(kEndpoint0In, &ep0ZlpDummy_, 0U);
    return;
  }

  const uint16_t packetLen = iFly::usermath::Min<uint16_t>(ep0InRemaining_, kEp0Mps);
  (void)UsbPcd().Transmit(kEndpoint0In, const_cast<uint8_t *>(ep0InPtr_), packetLen);
  ep0InPtr_ += packetLen;
  ep0InRemaining_ = static_cast<uint16_t>(ep0InRemaining_ - packetLen);
}

void UsbCdcAcm::ContinueControlInTransfer() {
  if (ep0InRemaining_ == 0U) {
    ep0InPtr_ = nullptr;
    ep0InRequestLen_ = 0U;
    (void)UsbPcd().Receive(kEndpoint0Out, ep0OutBuffer_, 0U);
    return;
  }

  const uint16_t packetLen = iFly::usermath::Min<uint16_t>(ep0InRemaining_, kEp0Mps);
  (void)UsbPcd().Transmit(kEndpoint0In, const_cast<uint8_t *>(ep0InPtr_), packetLen);
  ep0InPtr_ += packetLen;
  ep0InRemaining_ = static_cast<uint16_t>(ep0InRemaining_ - packetLen);
}

void UsbCdcAcm::SendControlStatus() {
  (void)UsbPcd().Transmit(kEndpoint0In, &ep0ZlpDummy_, 0U);
}

void UsbCdcAcm::StallControlEndpoint() {
  UsbPcd().SetStall(kEndpoint0In);
  UsbPcd().SetStall(kEndpoint0Out);
}

void UsbCdcAcm::PushReceivedPacket(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return;
  }

  uint32_t pushed = 0U;
  if ((appRxQueue_ != nullptr) && appRxQueue_->IsCreated()) {
    pushed = appRxQueue_->Enqueue(data, len);
  }

  if (pushed < len) {
    (void)rxDropped_.fetch_add(len - pushed, std::memory_order_relaxed);
  }
}

void UsbCdcAcm::ServiceTxPath() {
  (void)txServiceRequests_.fetch_add(1U, std::memory_order_acq_rel);
  if (txServiceRunning_.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  for (;;) {
    while (txServiceRequests_.exchange(0U, std::memory_order_acq_rel) != 0U) {
      if ((!initialized_.load(std::memory_order_acquire)) ||
          (!configured_.load(std::memory_order_acquire)) ||
          suspended_.load(std::memory_order_acquire) ||
          (txQueue_ == nullptr) || (txEndpointBuffer_ == nullptr) ||
          (!txQueue_->IsCreated()) || (!txEndpointBuffer_->IsCreated())) {
        continue;
      }

      (void)LoadTxPacketToInactiveBuffer();
      if (txBusy_.load(std::memory_order_acquire)) {
        continue;
      }

      if (!txEndpointBuffer_->HasInactiveData()) {
        continue;
      }

      uint8_t *data = txEndpointBuffer_->InactiveBuffer();
      const uint16_t length = txEndpointBuffer_->InactiveLength();
      if ((data == nullptr) || (length == 0U)) {
        continue;
      }

      bool expectedBusy = false;
      if (!txBusy_.compare_exchange_strong(expectedBusy, true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
        continue;
      }

      if (UsbPcd().Transmit(kEpCdcDataIn, data, length) == HAL_OK) {
        txEndpointBuffer_->SwapBuffers();
        (void)LoadTxPacketToInactiveBuffer();
        continue;
      }

      txBusy_.store(false, std::memory_order_release);
    }

    txServiceRunning_.store(false, std::memory_order_release);
    if (txServiceRequests_.load(std::memory_order_acquire) == 0U) {
      return;
    }

    if (txServiceRunning_.exchange(true, std::memory_order_acq_rel)) {
      return;
    }
  }
}

uint32_t UsbCdcAcm::LoadTxPacketToInactiveBuffer() {
  if ((txQueue_ == nullptr) || (txEndpointBuffer_ == nullptr) ||
      (!txQueue_->IsCreated()) || (!txEndpointBuffer_->IsCreated()) ||
      txEndpointBuffer_->HasInactiveData()) {
    return 0U;
  }

  uint8_t *buffer = txEndpointBuffer_->InactiveBuffer();
  if (buffer == nullptr) {
    return 0U;
  }

  const uint32_t pulled = txQueue_->Dequeue(buffer, txEndpointBuffer_->PacketSize());
  if (pulled > 0U) {
    txEndpointBuffer_->SetInactiveLength(static_cast<uint16_t>(pulled));
  }
  return pulled;
}

uint32_t UsbCdcAcm::UpperRxUsed() const {
  if ((appRxQueue_ == nullptr) || (!appRxQueue_->IsCreated())) {
    return 0U;
  }

  return appRxQueue_->UsedSize();
}

uint32_t UsbCdcAcm::UpperRxFree() const {
  if ((appRxQueue_ == nullptr) || (!appRxQueue_->IsCreated())) {
    return 0U;
  }

  return appRxQueue_->FreeSize();
}

} // namespace iFly

extern "C" {

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnReset();
  }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnSetupStage();
  }
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnDataInStage(epnum);
  }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnDataOutStage(epnum);
  }
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnSuspend();
  }
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {
  if (UsbPcd().Matches(static_cast<void *>(hpcd))) {
    iFly::UsbCdcAcm::Instance().OnResume();
  }
}

} // extern "C"
