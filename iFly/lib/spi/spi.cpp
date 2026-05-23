#include "spi.hpp"

#include "stm32f4xx_hal.h"

namespace {

constexpr iFly::GpioPinState OppositeState(iFly::GpioPinState state) {
  return (state == iFly::GpioPinState::kSet) ? iFly::GpioPinState::kReset
                                             : iFly::GpioPinState::kSet;
}

constexpr uint16_t ChunkSize(uint32_t left) {
  return (left > 0xFFFFU) ? 0xFFFFU : static_cast<uint16_t>(left);
}

#if defined(HAL_SPI_MODULE_ENABLED)
extern "C" {
extern SPI_HandleTypeDef hspi1 __attribute__((weak));
extern SPI_HandleTypeDef hspi2 __attribute__((weak));
extern SPI_HandleTypeDef hspi3 __attribute__((weak));
}

SPI_HandleTypeDef *DefaultHandleForPort(iFly::SpiPortId port) {
  switch (port) {
    case iFly::SpiPortId::kSpi1:
      return &hspi1;
    case iFly::SpiPortId::kSpi2:
      return &hspi2;
    case iFly::SpiPortId::kSpi3:
      return &hspi3;
    case iFly::SpiPortId::kCount:
    default:
      return nullptr;
  }
}

iFly::SpiStatus FromHalStatus(HAL_StatusTypeDef status) {
  switch (status) {
    case HAL_OK:
      return iFly::SpiStatus::kOk;
    case HAL_BUSY:
      return iFly::SpiStatus::kBusy;
    case HAL_TIMEOUT:
      return iFly::SpiStatus::kTimeout;
    case HAL_ERROR:
    default:
      return iFly::SpiStatus::kError;
  }
}
#endif

} // namespace

namespace iFly {

const char *ToString(SpiPortId port) {
  switch (port) {
    case SpiPortId::kSpi1:
      return "SPI1";
    case SpiPortId::kSpi2:
      return "SPI2";
    case SpiPortId::kSpi3:
      return "SPI3";
    case SpiPortId::kCount:
    default:
      return "SPI";
  }
}

SpiMaster::SpiMaster(SpiPortId port, uint32_t timeout_ms) {
  (void)Init(port, timeout_ms);
}

SpiMaster::SpiMaster(const Config &config) {
  (void)Init(config);
}

bool SpiMaster::Init(SpiPortId port, uint32_t timeout_ms) {
  port_ = port;
  timeout_ms_ = timeout_ms;
  last_status_ = SpiStatus::kOk;
  return IsReady();
}

bool SpiMaster::Init(const Config &config) {
  return Init(config.port, config.timeout_ms);
}

void SpiMaster::Deinit() {
  port_ = SpiPortId::kCount;
  timeout_ms_ = kDefaultTimeoutMs;
  last_status_ = SpiStatus::kOk;
}

void SpiMaster::AttachPort(SpiPortId port) {
  port_ = port;
  last_status_ = SpiStatus::kOk;
}

uint32_t SpiMaster::Write(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  if ((hspi == nullptr) || (hspi->Instance == nullptr)) {
    SetLastStatus(SpiStatus::kNotReady);
    return 0U;
  }

  uint32_t transferred = 0U;
  while (transferred < len) {
    const uint16_t chunk = ChunkSize(len - transferred);
    const HAL_StatusTypeDef status =
        HAL_SPI_Transmit(hspi,
                         const_cast<uint8_t *>(data + transferred),
                         chunk,
                         timeout_ms_);
    SetLastStatus(FromHalStatus(status));
    if (status != HAL_OK) {
      break;
    }

    transferred += chunk;
  }

  return transferred;
#else
  SetLastStatus(SpiStatus::kUnavailable);
  return 0U;
#endif
}

uint32_t SpiMaster::Read(uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  if ((hspi == nullptr) || (hspi->Instance == nullptr)) {
    SetLastStatus(SpiStatus::kNotReady);
    return 0U;
  }

  uint32_t received = 0U;
  while (received < len) {
    const uint16_t chunk = ChunkSize(len - received);
    const HAL_StatusTypeDef status =
        HAL_SPI_Receive(hspi, data + received, chunk, timeout_ms_);
    SetLastStatus(FromHalStatus(status));
    if (status != HAL_OK) {
      break;
    }

    received += chunk;
  }

  return received;
#else
  SetLastStatus(SpiStatus::kUnavailable);
  return 0U;
#endif
}

uint32_t SpiMaster::Transfer(const uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
  if (len == 0U) {
    return 0U;
  }

  if ((tx_data == nullptr) && (rx_data == nullptr)) {
    SetLastStatus(SpiStatus::kError);
    return 0U;
  }

  if (tx_data == nullptr) {
    return Read(rx_data, len);
  }

  if (rx_data == nullptr) {
    return Write(tx_data, len);
  }

#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  if ((hspi == nullptr) || (hspi->Instance == nullptr)) {
    SetLastStatus(SpiStatus::kNotReady);
    return 0U;
  }

  uint32_t transferred = 0U;
  while (transferred < len) {
    const uint16_t chunk = ChunkSize(len - transferred);
    const HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(hspi,
                                const_cast<uint8_t *>(tx_data + transferred),
                                rx_data + transferred,
                                chunk,
                                timeout_ms_);
    SetLastStatus(FromHalStatus(status));
    if (status != HAL_OK) {
      break;
    }

    transferred += chunk;
  }

  return transferred;
#else
  SetLastStatus(SpiStatus::kUnavailable);
  return 0U;
#endif
}

bool SpiMaster::WriteByte(uint8_t data) {
  return Write(&data, 1U) == 1U;
}

bool SpiMaster::ReadByte(uint8_t *data) {
  return Read(data, 1U) == 1U;
}

bool SpiMaster::TransferByte(uint8_t tx_data, uint8_t *rx_data) {
  if (rx_data == nullptr) {
    SetLastStatus(SpiStatus::kError);
    return false;
  }

  return Transfer(&tx_data, rx_data, 1U) == 1U;
}

bool SpiMaster::Abort() {
#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  if ((hspi == nullptr) || (hspi->Instance == nullptr)) {
    SetLastStatus(SpiStatus::kNotReady);
    return false;
  }

  const HAL_StatusTypeDef status = HAL_SPI_Abort(hspi);
  SetLastStatus(FromHalStatus(status));
  return status == HAL_OK;
#else
  SetLastStatus(SpiStatus::kUnavailable);
  return false;
#endif
}

bool SpiMaster::IsReady() const {
#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  return (hspi != nullptr) && (hspi->Instance != nullptr);
#else
  return false;
#endif
}

bool SpiMaster::IsBusy() const {
#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  if ((hspi == nullptr) || (hspi->Instance == nullptr)) {
    return false;
  }

  const HAL_SPI_StateTypeDef state = HAL_SPI_GetState(hspi);
  return (state == HAL_SPI_STATE_BUSY) ||
         (state == HAL_SPI_STATE_BUSY_TX) ||
         (state == HAL_SPI_STATE_BUSY_RX) ||
         (state == HAL_SPI_STATE_BUSY_TX_RX);
#else
  return false;
#endif
}

SpiPortId SpiMaster::Port() const {
  return port_;
}

SpiStatus SpiMaster::LastStatus() const {
  return last_status_;
}

uint32_t SpiMaster::ErrorCode() const {
#if defined(HAL_SPI_MODULE_ENABLED)
  SPI_HandleTypeDef *hspi = DefaultHandleForPort(port_);
  return ((hspi != nullptr) && (hspi->Instance != nullptr)) ? HAL_SPI_GetError(hspi) : 0U;
#else
  return 0U;
#endif
}

uint32_t SpiMaster::Timeout() const {
  return timeout_ms_;
}

void SpiMaster::SetTimeout(uint32_t timeout_ms) {
  timeout_ms_ = timeout_ms;
}

void SpiMaster::SetLastStatus(SpiStatus status) {
  last_status_ = status;
}

SpiChipSelect::SpiChipSelect(const Config &config) {
  (void)Init(config);
}

bool SpiChipSelect::Init(const Config &config) {
  port_ = config.port;
  pin_ = config.pin;
  active_state_ = config.active_state;
  selected_ = false;

  if (!IsReady()) {
    return false;
  }

  return !config.inactive_on_init || Release();
}

void SpiChipSelect::Deinit() {
  (void)Release();
  port_ = GpioPortId::kNone;
  pin_ = GpioPinId::kNone;
  active_state_ = GpioPinState::kReset;
  selected_ = false;
}

void SpiChipSelect::AttachHardware(GpioPortId port,
                                   GpioPinId pin,
                                   GpioPinState active_state) {
  (void)Release();
  port_ = port;
  pin_ = pin;
  active_state_ = active_state;
  selected_ = false;
}

bool SpiChipSelect::Select() {
  if (!WriteState(active_state_)) {
    return false;
  }

  selected_ = true;
  return true;
}

bool SpiChipSelect::Release() {
  if (!WriteState(InactiveState())) {
    selected_ = false;
    return false;
  }

  selected_ = false;
  return true;
}

bool SpiChipSelect::IsReady() const {
  return (gpio_detail::ToHalPort(port_) != nullptr) &&
         (gpio_detail::ToHalPin(pin_) != 0U);
}

bool SpiChipSelect::IsSelected() const {
  return selected_;
}

GpioPinState SpiChipSelect::InactiveState() const {
  return OppositeState(active_state_);
}

bool SpiChipSelect::WriteState(GpioPinState state) const {
  if (!IsReady()) {
    return false;
  }

  HAL_GPIO_WritePin(gpio_detail::ToHalPort(port_),
                    gpio_detail::ToHalPin(pin_),
                    gpio_detail::ToHalPinState(state));
  return true;
}

SpiDevice::SpiDevice(const Config &config) {
  (void)Init(config);
}

bool SpiDevice::Init(const Config &config) {
  bus_ = config.bus;
  chip_select_ = config.chip_select;
  return IsReady();
}

void SpiDevice::Deinit() {
  EndTransaction();
  bus_ = nullptr;
  chip_select_ = nullptr;
}

bool SpiDevice::BeginTransaction() {
  if (!IsReady()) {
    return false;
  }

  return (chip_select_ == nullptr) || chip_select_->Select();
}

void SpiDevice::EndTransaction() {
  if (chip_select_ != nullptr) {
    (void)chip_select_->Release();
  }
}

uint32_t SpiDevice::Write(const uint8_t *data, uint32_t len) {
  if (!BeginTransaction()) {
    return 0U;
  }

  const uint32_t written = bus_->Write(data, len);
  EndTransaction();
  return written;
}

uint32_t SpiDevice::Read(uint8_t *data, uint32_t len) {
  if (!BeginTransaction()) {
    return 0U;
  }

  const uint32_t received = bus_->Read(data, len);
  EndTransaction();
  return received;
}

uint32_t SpiDevice::Transfer(const uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
  if (!BeginTransaction()) {
    return 0U;
  }

  const uint32_t transferred = bus_->Transfer(tx_data, rx_data, len);
  EndTransaction();
  return transferred;
}

bool SpiDevice::IsReady() const {
  return (bus_ != nullptr) && bus_->IsReady() &&
         ((chip_select_ == nullptr) || chip_select_->IsReady());
}

} // namespace iFly

