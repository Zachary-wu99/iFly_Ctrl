/**
 * @file w25q32.hpp
 * @brief W25Q32 SPI Flash 驱动接口。
 */
#ifndef IFLY_APP_FLASH_W25Q32_HPP
#define IFLY_APP_FLASH_W25Q32_HPP

#include <stdint.h>

#include "spi.hpp"
#include "tick.hpp"

namespace iFly {

/**
 * @brief W25Q32 驱动统一状态码。
 */
enum class W25q32Status : uint8_t {
  kOk = 0U, /**< 操作成功。 */
  kNotReady, /**< SPI 或片选引脚尚未就绪。 */
  kInvalidArgument, /**< 传入参数非法。 */
  kOutOfRange, /**< 地址或长度超出芯片容量。 */
  kPageBoundaryCrossed, /**< 页编程跨越页边界。 */
  kBusy, /**< 芯片当前忙。 */
  kTimeout, /**< 等待芯片空闲超时。 */
  kSpiError, /**< SPI 传输失败。 */
  kWriteEnableFailed, /**< 写使能锁存未置位。 */
  kUnexpectedId /**< 读取到的 JEDEC ID 不是 W25Q32。 */
};

/**
 * @brief W25Q32 JEDEC ID。
 */
struct W25q32JedecId final {
  uint8_t manufacturer_id = 0U; /**< 厂商 ID。 */
  uint8_t memory_type = 0U; /**< 存储类型 ID。 */
  uint8_t capacity_id = 0U; /**< 容量 ID。 */

  /**
   * @brief 判断当前 ID 是否匹配 W25Q32。
   *
   * @return 匹配 W25Q32 返回 `true`。
   */
  bool IsW25q32() const {
    return (manufacturer_id == 0xEFU) && (memory_type == 0x40U) &&
           (capacity_id == 0x16U);
  }

  /**
   * @brief 获取 24 位 JEDEC ID 原始值。
   *
   * @return 按厂商、类型、容量拼接后的原始 ID。
   */
  uint32_t Raw() const {
    return (static_cast<uint32_t>(manufacturer_id) << 16U) |
           (static_cast<uint32_t>(memory_type) << 8U) |
           static_cast<uint32_t>(capacity_id);
  }
};

namespace w25q32_detail {

static constexpr uint8_t kCommandWriteEnable = 0x06U;
static constexpr uint8_t kCommandWriteDisable = 0x04U;
static constexpr uint8_t kCommandReadStatus1 = 0x05U;
static constexpr uint8_t kCommandReadStatus2 = 0x35U;
static constexpr uint8_t kCommandReadStatus3 = 0x15U;
static constexpr uint8_t kCommandPageProgram = 0x02U;
static constexpr uint8_t kCommandReadData = 0x03U;
static constexpr uint8_t kCommandFastRead = 0x0BU;
static constexpr uint8_t kCommandSectorErase = 0x20U;
static constexpr uint8_t kCommandBlockErase32K = 0x52U;
static constexpr uint8_t kCommandBlockErase64K = 0xD8U;
static constexpr uint8_t kCommandChipErase = 0xC7U;
static constexpr uint8_t kCommandPowerDown = 0xB9U;
static constexpr uint8_t kCommandReleasePowerDown = 0xABU;
static constexpr uint8_t kCommandReadManufacturerDeviceId = 0x90U;
static constexpr uint8_t kCommandReadUniqueId = 0x4BU;
static constexpr uint8_t kCommandReadJedecId = 0x9FU;
static constexpr uint8_t kCommandEnableReset = 0x66U;
static constexpr uint8_t kCommandResetDevice = 0x99U;

static constexpr uint8_t kStatus1BusyMask = 0x01U;
static constexpr uint8_t kStatus1WriteEnableMask = 0x02U;

constexpr uint32_t Min(uint32_t lhs, uint32_t rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

/**
 * @brief 判断访问区间是否落在芯片容量范围内。
 *
 * @param address 起始地址。
 * @param length 访问长度，单位为字节。
 * @param capacity 芯片容量，单位为字节。
 * @return 区间有效返回 `true`。
 */
constexpr bool IsRangeValid(uint32_t address, uint32_t length, uint32_t capacity) {
  if (length == 0U) {
    return address <= capacity;
  }

  return (address < capacity) && (length <= (capacity - address));
}

/**
 * @brief 填充带 24 位地址的 W25Q32 命令缓冲区。
 *
 * @param command 命令码。
 * @param address 24 位目标地址。
 * @param buffer 输出命令缓冲区，至少 4 字节。
 */
inline void FillCommandAddress(uint8_t command, uint32_t address, uint8_t *buffer) {
  buffer[0] = command;
  buffer[1] = static_cast<uint8_t>((address >> 16U) & 0xFFU);
  buffer[2] = static_cast<uint8_t>((address >> 8U) & 0xFFU);
  buffer[3] = static_cast<uint8_t>(address & 0xFFU);
}

} // namespace w25q32_detail

/**
 * @brief W25Q32 SPI Flash 设备对象。
 *
 * @tparam SpiPortValue 绑定的 SPI 逻辑端口。
 * @tparam CsPortValue 片选 GPIO 端口。
 * @tparam CsPinValue 片选 GPIO 引脚。
 * @tparam CsActiveStateValue 片选有效电平，W25Q32 默认为低有效。
 */
template <SpiPortId SpiPortValue,
          GpioPortId CsPortValue,
          GpioPinId CsPinValue,
          GpioPinState CsActiveStateValue = GpioPinState::kReset>
class W25q32 final {
public:
  static_assert(SpiPortValue != SpiPortId::kCount,
                "W25q32 needs a concrete SPI port.");
  static_assert(CsPortValue != GpioPortId::kNone,
                "W25q32 needs a concrete CS GPIO port.");
  static_assert(CsPinValue != GpioPinId::kNone,
                "W25q32 needs a concrete CS GPIO pin.");

  static constexpr SpiPortId kSpiPort = SpiPortValue; /**< 绑定的 SPI 逻辑端口。 */
  static constexpr GpioPortId kCsPort = CsPortValue; /**< 片选 GPIO 端口。 */
  static constexpr GpioPinId kCsPin = CsPinValue; /**< 片选 GPIO 引脚。 */
  static constexpr GpioPinState kCsActiveState =
      CsActiveStateValue; /**< 片选有效电平。 */

  static constexpr uint32_t kCapacityBytes = 4U * 1024U * 1024U; /**< 芯片容量。 */
  static constexpr uint32_t kPageSize = 256U; /**< 页大小，单位为字节。 */
  static constexpr uint32_t kSectorSize = 4U * 1024U; /**< 扇区大小，单位为字节。 */
  static constexpr uint32_t kBlock32Size = 32U * 1024U; /**< 32KB 块大小。 */
  static constexpr uint32_t kBlock64Size = 64U * 1024U; /**< 64KB 块大小。 */
  static constexpr uint32_t kPageCount = kCapacityBytes / kPageSize; /**< 页数量。 */
  static constexpr uint32_t kSectorCount =
      kCapacityBytes / kSectorSize; /**< 扇区数量。 */
  static constexpr uint32_t kBlock32Count =
      kCapacityBytes / kBlock32Size; /**< 32KB 块数量。 */
  static constexpr uint32_t kBlock64Count =
      kCapacityBytes / kBlock64Size; /**< 64KB 块数量。 */
  static constexpr uint32_t kUniqueIdSize = 8U; /**< 唯一 ID 长度。 */
  static constexpr uint32_t kWaitForever = 0xFFFFFFFFU; /**< 永久等待标记。 */

  static constexpr uint32_t kDefaultProgramTimeoutMs = 5U; /**< 页编程默认超时。 */
  static constexpr uint32_t kDefaultSectorEraseTimeoutMs = 500U; /**< 扇区擦除默认超时。 */
  static constexpr uint32_t kDefaultBlock32EraseTimeoutMs = 2000U; /**< 32KB 块擦除默认超时。 */
  static constexpr uint32_t kDefaultBlock64EraseTimeoutMs = 2500U; /**< 64KB 块擦除默认超时。 */
  static constexpr uint32_t kDefaultChipEraseTimeoutMs = 60000U; /**< 全片擦除默认超时。 */
  static constexpr uint32_t kDefaultStatusWriteTimeoutMs = 20U; /**< 状态寄存器写入默认超时。 */
  static constexpr uint32_t kPowerDownDelayUs = 3U; /**< 进入掉电模式等待时间。 */
  static constexpr uint32_t kReleasePowerDownDelayUs = 3U; /**< 退出掉电模式等待时间。 */
  static constexpr uint32_t kResetDelayUs = 30U; /**< 软复位后等待时间。 */

  W25q32() = default;

  /**
   * @brief 初始化 SPI 总线和片选控制。
   *
   * @param spi_timeout_ms SPI 阻塞传输超时时间，单位为毫秒。
   * @return 初始化成功返回 `true`。
   */
  bool Init(uint32_t spi_timeout_ms = SpiMaster::kDefaultTimeoutMs) {
    initialized_ = false;
    last_status_ = W25q32Status::kOk;

    const bool spi_ready = spi_.Init(kSpiPort, spi_timeout_ms);

    SpiChipSelect::Config cs_config {};
    cs_config.port = kCsPort;
    cs_config.pin = kCsPin;
    cs_config.active_state = kCsActiveState;
    cs_config.inactive_on_init = true;
    const bool cs_ready = chip_select_.Init(cs_config);

    initialized_ = spi_ready && cs_ready;
    if (!initialized_) {
      SetLastStatus(W25q32Status::kNotReady);
    }

    return initialized_;
  }

  /**
   * @brief 解除 SPI 和片选绑定。
   */
  void Deinit() {
    chip_select_.Deinit();
    spi_.Deinit();
    initialized_ = false;
    last_status_ = W25q32Status::kOk;
  }

  /**
   * @brief 判断驱动实例是否已经就绪。
   *
   * @return SPI 与片选均就绪返回 `true`。
   */
  bool IsReady() const {
    return initialized_ && spi_.IsReady() && chip_select_.IsReady();
  }

  /**
   * @brief 获取绑定的 SPI 逻辑端口。
   *
   * @return SPI 逻辑端口。
   */
  static constexpr SpiPortId SpiPort() {
    return kSpiPort;
  }

  /**
   * @brief 获取片选 GPIO 端口。
   *
   * @return 片选 GPIO 端口。
   */
  static constexpr GpioPortId ChipSelectPort() {
    return kCsPort;
  }

  /**
   * @brief 获取片选 GPIO 引脚。
   *
   * @return 片选 GPIO 引脚。
   */
  static constexpr GpioPinId ChipSelectPin() {
    return kCsPin;
  }

  /**
   * @brief 获取最近一次 W25Q32 操作状态。
   *
   * @return 最近一次操作状态。
   */
  W25q32Status LastStatus() const {
    return last_status_;
  }

  /**
   * @brief 获取最近一次底层 SPI 操作状态。
   *
   * @return SPI 状态码。
   */
  SpiStatus LastSpiStatus() const {
    return spi_.LastStatus();
  }

  /**
   * @brief 获取底层 SPI 错误码。
   *
   * @return SPI 错误码。
   */
  uint32_t SpiErrorCode() const {
    return spi_.ErrorCode();
  }

  /**
   * @brief 获取当前 SPI 阻塞传输超时时间。
   *
   * @return 超时时间，单位为毫秒。
   */
  uint32_t SpiTimeout() const {
    return spi_.Timeout();
  }

  /**
   * @brief 设置 SPI 阻塞传输超时时间。
   *
   * @param timeout_ms 超时时间，单位为毫秒。
   */
  void SetSpiTimeout(uint32_t timeout_ms) {
    spi_.SetTimeout(timeout_ms);
  }

  /**
   * @brief 探测芯片并校验 JEDEC ID。
   *
   * @return ID 匹配 W25Q32 返回 `true`。
   */
  bool Probe() {
    W25q32JedecId id {};
    if (!ReadJedecId(&id)) {
      return false;
    }

    if (!id.IsW25q32()) {
      SetLastStatus(W25q32Status::kUnexpectedId);
      return false;
    }

    SetLastStatus(W25q32Status::kOk);
    return true;
  }

  /**
   * @brief 读取 JEDEC ID。
   *
   * @param id 输出 JEDEC ID。
   * @return 读取成功返回 `true`。
   */
  bool ReadJedecId(W25q32JedecId *id) {
    if (id == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    uint8_t raw[3] {};
    const uint8_t command = w25q32_detail::kCommandReadJedecId;
    if (!TransferCommandRead(&command, 1U, raw, sizeof(raw))) {
      return false;
    }

    id->manufacturer_id = raw[0];
    id->memory_type = raw[1];
    id->capacity_id = raw[2];
    return true;
  }

  /**
   * @brief 读取厂商 ID 和设备 ID。
   *
   * @param manufacturer_id 输出厂商 ID。
   * @param device_id 输出设备 ID。
   * @return 读取成功返回 `true`。
   */
  bool ReadManufacturerDeviceId(uint8_t *manufacturer_id, uint8_t *device_id) {
    if ((manufacturer_id == nullptr) || (device_id == nullptr)) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    uint8_t command[4] {
      w25q32_detail::kCommandReadManufacturerDeviceId,
      0x00U,
      0x00U,
      0x00U
    };
    uint8_t raw[2] {};

    if (!TransferCommandRead(command, sizeof(command), raw, sizeof(raw))) {
      return false;
    }

    *manufacturer_id = raw[0];
    *device_id = raw[1];
    return true;
  }

  /**
   * @brief 读取芯片唯一 ID。
   *
   * @param uid 输出缓冲区，长度至少为 `kUniqueIdSize`。
   * @return 读取成功返回 `true`。
   */
  bool ReadUniqueId(uint8_t *uid) {
    if (uid == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    uint8_t command[5] {
      w25q32_detail::kCommandReadUniqueId,
      0x00U,
      0x00U,
      0x00U,
      0x00U
    };

    return TransferCommandRead(command, sizeof(command), uid, kUniqueIdSize);
  }

  /**
   * @brief 读取状态寄存器 1。
   *
   * @param status 输出状态寄存器值。
   * @return 读取成功返回 `true`。
   */
  bool ReadStatus1(uint8_t *status) {
    return ReadStatusRegister(w25q32_detail::kCommandReadStatus1, status);
  }

  /**
   * @brief 读取状态寄存器 2。
   *
   * @param status 输出状态寄存器值。
   * @return 读取成功返回 `true`。
   */
  bool ReadStatus2(uint8_t *status) {
    return ReadStatusRegister(w25q32_detail::kCommandReadStatus2, status);
  }

  /**
   * @brief 读取状态寄存器 3。
   *
   * @param status 输出状态寄存器值。
   * @return 读取成功返回 `true`。
   */
  bool ReadStatus3(uint8_t *status) {
    return ReadStatusRegister(w25q32_detail::kCommandReadStatus3, status);
  }

  /**
   * @brief 查询芯片忙状态。
   *
   * @param busy 输出忙状态。
   * @return 查询成功返回 `true`。
   */
  bool IsBusy(bool *busy) {
    if (busy == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    uint8_t status = 0U;
    if (!ReadStatus1(&status)) {
      return false;
    }

    *busy = (status & w25q32_detail::kStatus1BusyMask) != 0U;
    return true;
  }

  /**
   * @brief 查询芯片当前是否忙。
   *
   * @return 查询成功且芯片忙返回 `true`。
   */
  bool IsBusy() {
    bool busy = false;
    return IsBusy(&busy) && busy;
  }

  /**
   * @brief 查询写使能锁存状态。
   *
   * @param enabled 输出写使能状态。
   * @return 查询成功返回 `true`。
   */
  bool IsWriteEnabled(bool *enabled) {
    if (enabled == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    uint8_t status = 0U;
    if (!ReadStatus1(&status)) {
      return false;
    }

    *enabled = (status & w25q32_detail::kStatus1WriteEnableMask) != 0U;
    return true;
  }

  /**
   * @brief 阻塞等待芯片退出忙状态。
   *
   * @param timeout_ms 超时时间，单位为毫秒；`kWaitForever` 表示永久等待。
   * @return 芯片空闲返回 `true`。
   */
  bool WaitWhileBusy(uint32_t timeout_ms = kWaitForever) {
    if (!IsReady()) {
      SetLastStatus(W25q32Status::kNotReady);
      return false;
    }

    const uint32_t start_ms = tick::NowMs();
    while (true) {
      uint8_t status = 0U;
      if (!ReadStatus1(&status)) {
        return false;
      }

      if ((status & w25q32_detail::kStatus1BusyMask) == 0U) {
        SetLastStatus(W25q32Status::kOk);
        return true;
      }

      if (HasTimedOut(start_ms, timeout_ms)) {
        SetLastStatus(W25q32Status::kTimeout);
        return false;
      }

      tick::DelayMs(1U);
    }
  }

  /**
   * @brief 发送写使能命令并确认锁存置位。
   *
   * @return 写使能成功返回 `true`。
   */
  bool WriteEnable() {
    if (!RunCommand(w25q32_detail::kCommandWriteEnable)) {
      return false;
    }

    bool enabled = false;
    if (!IsWriteEnabled(&enabled)) {
      return false;
    }

    if (!enabled) {
      SetLastStatus(W25q32Status::kWriteEnableFailed);
      return false;
    }

    SetLastStatus(W25q32Status::kOk);
    return true;
  }

  /**
   * @brief 发送写禁止命令。
   *
   * @return 命令发送成功返回 `true`。
   */
  bool WriteDisable() {
    return RunCommand(w25q32_detail::kCommandWriteDisable);
  }

  /**
   * @brief 读取一段 Flash 数据。
   *
   * @param address 起始地址。
   * @param data 接收缓冲区。
   * @param length 读取长度，单位为字节。
   * @return 读取成功返回 `true`。
   */
  bool Read(uint32_t address, uint8_t *data, uint32_t length) {
    if (length == 0U) {
      SetLastStatus(W25q32Status::kOk);
      return true;
    }

    if (data == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    if (!w25q32_detail::IsRangeValid(address, length, kCapacityBytes)) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    if (!EnsureIdle()) {
      return false;
    }

    uint8_t command[4] {};
    w25q32_detail::FillCommandAddress(w25q32_detail::kCommandReadData,
                                      address,
                                      command);
    return TransferCommandRead(command, sizeof(command), data, length);
  }

  /**
   * @brief 使用 Fast Read 命令读取一段 Flash 数据。
   *
   * @param address 起始地址。
   * @param data 接收缓冲区。
   * @param length 读取长度，单位为字节。
   * @return 读取成功返回 `true`。
   */
  bool FastRead(uint32_t address, uint8_t *data, uint32_t length) {
    if (length == 0U) {
      SetLastStatus(W25q32Status::kOk);
      return true;
    }

    if (data == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    if (!w25q32_detail::IsRangeValid(address, length, kCapacityBytes)) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    if (!EnsureIdle()) {
      return false;
    }

    uint8_t command[5] {};
    w25q32_detail::FillCommandAddress(w25q32_detail::kCommandFastRead,
                                      address,
                                      command);
    command[4] = 0x00U;
    return TransferCommandRead(command, sizeof(command), data, length);
  }

  /**
   * @brief 对单页内数据执行页编程。
   *
   * @param address 起始地址。
   * @param data 待写入数据。
   * @param length 写入长度，不能跨越页边界。
   * @param wait_complete 是否等待写入完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 页编程启动或完成成功返回 `true`。
   */
  bool PageProgram(uint32_t address,
                   const uint8_t *data,
                   uint32_t length,
                   bool wait_complete = true,
                   uint32_t timeout_ms = kDefaultProgramTimeoutMs) {
    if (length == 0U) {
      SetLastStatus(W25q32Status::kOk);
      return true;
    }

    if (data == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    if (!w25q32_detail::IsRangeValid(address, length, kCapacityBytes)) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    if (length > PageBytesUntilBoundary(address)) {
      SetLastStatus(W25q32Status::kPageBoundaryCrossed);
      return false;
    }

    if (!EnsureIdle() || !WriteEnable()) {
      return false;
    }

    uint8_t command[4] {};
    w25q32_detail::FillCommandAddress(w25q32_detail::kCommandPageProgram,
                                      address,
                                      command);

    if (!TransferCommandWrite(command, sizeof(command), data, length)) {
      return false;
    }

    return !wait_complete || WaitWhileBusy(timeout_ms);
  }

  /**
   * @brief 自动按页切分写入一段 Flash 数据。
   *
   * @param address 起始地址。
   * @param data 待写入数据。
   * @param length 写入长度，单位为字节。
   * @param page_timeout_ms 单页编程超时时间，单位为毫秒。
   * @return 实际写入字节数。
   */
  uint32_t Write(uint32_t address,
                 const uint8_t *data,
                 uint32_t length,
                 uint32_t page_timeout_ms = kDefaultProgramTimeoutMs) {
    if (length == 0U) {
      SetLastStatus(W25q32Status::kOk);
      return 0U;
    }

    if (data == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return 0U;
    }

    if (!w25q32_detail::IsRangeValid(address, length, kCapacityBytes)) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return 0U;
    }

    uint32_t written = 0U;
    while (written < length) {
      const uint32_t current_address = address + written;
      const uint32_t chunk =
          w25q32_detail::Min(length - written, PageBytesUntilBoundary(current_address));

      if (!PageProgram(current_address,
                       data + written,
                       chunk,
                       true,
                       page_timeout_ms)) {
        return written;
      }

      written += chunk;
    }

    SetLastStatus(W25q32Status::kOk);
    return written;
  }

  /**
   * @brief 自动按页切分并要求完整写入。
   *
   * @param address 起始地址。
   * @param data 待写入数据。
   * @param length 写入长度，单位为字节。
   * @param page_timeout_ms 单页编程超时时间，单位为毫秒。
   * @return 全部写入成功返回 `true`。
   */
  bool WriteAll(uint32_t address,
                const uint8_t *data,
                uint32_t length,
                uint32_t page_timeout_ms = kDefaultProgramTimeoutMs) {
    return Write(address, data, length, page_timeout_ms) == length;
  }

  /**
   * @brief 擦除地址所在的 4KB 扇区。
   *
   * @param address 扇区内任意地址。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseSector(uint32_t address,
                   bool wait_complete = true,
                   uint32_t timeout_ms = kDefaultSectorEraseTimeoutMs) {
    return EraseAddressCommand(w25q32_detail::kCommandSectorErase,
                               address,
                               wait_complete,
                               timeout_ms);
  }

  /**
   * @brief 按扇区索引擦除 4KB 扇区。
   *
   * @param sector_index 扇区索引。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseSectorByIndex(uint32_t sector_index,
                          bool wait_complete = true,
                          uint32_t timeout_ms = kDefaultSectorEraseTimeoutMs) {
    if (sector_index >= kSectorCount) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    return EraseSector(sector_index * kSectorSize, wait_complete, timeout_ms);
  }

  /**
   * @brief 擦除地址所在的 32KB 块。
   *
   * @param address 块内任意地址。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseBlock32(uint32_t address,
                    bool wait_complete = true,
                    uint32_t timeout_ms = kDefaultBlock32EraseTimeoutMs) {
    return EraseAddressCommand(w25q32_detail::kCommandBlockErase32K,
                               address,
                               wait_complete,
                               timeout_ms);
  }

  /**
   * @brief 按块索引擦除 32KB 块。
   *
   * @param block_index 32KB 块索引。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseBlock32ByIndex(uint32_t block_index,
                           bool wait_complete = true,
                           uint32_t timeout_ms = kDefaultBlock32EraseTimeoutMs) {
    if (block_index >= kBlock32Count) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    return EraseBlock32(block_index * kBlock32Size, wait_complete, timeout_ms);
  }

  /**
   * @brief 擦除地址所在的 64KB 块。
   *
   * @param address 块内任意地址。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseBlock64(uint32_t address,
                    bool wait_complete = true,
                    uint32_t timeout_ms = kDefaultBlock64EraseTimeoutMs) {
    return EraseAddressCommand(w25q32_detail::kCommandBlockErase64K,
                               address,
                               wait_complete,
                               timeout_ms);
  }

  /**
   * @brief 按块索引擦除 64KB 块。
   *
   * @param block_index 64KB 块索引。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseBlock64ByIndex(uint32_t block_index,
                           bool wait_complete = true,
                           uint32_t timeout_ms = kDefaultBlock64EraseTimeoutMs) {
    if (block_index >= kBlock64Count) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    return EraseBlock64(block_index * kBlock64Size, wait_complete, timeout_ms);
  }

  /**
   * @brief 执行全片擦除。
   *
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool ChipErase(bool wait_complete = true,
                 uint32_t timeout_ms = kDefaultChipEraseTimeoutMs) {
    if (!EnsureIdle() || !WriteEnable()) {
      return false;
    }

    if (!RunCommand(w25q32_detail::kCommandChipErase)) {
      return false;
    }

    return !wait_complete || WaitWhileBusy(timeout_ms);
  }

  /**
   * @brief 进入低功耗掉电模式。
   *
   * @return 命令执行成功返回 `true`。
   */
  bool PowerDown() {
    if (!EnsureIdle()) {
      return false;
    }

    if (!RunCommand(w25q32_detail::kCommandPowerDown)) {
      return false;
    }

    tick::DelayUs(kPowerDownDelayUs);
    return true;
  }

  /**
   * @brief 退出低功耗掉电模式。
   *
   * @return 命令执行成功返回 `true`。
   */
  bool ReleasePowerDown() {
    if (!RunCommand(w25q32_detail::kCommandReleasePowerDown)) {
      return false;
    }

    tick::DelayUs(kReleasePowerDownDelayUs);
    return true;
  }

  /**
   * @brief 执行芯片软复位。
   *
   * @param wait_if_busy 复位前是否等待芯片空闲。
   * @param wait_timeout_ms 等待空闲超时时间，单位为毫秒。
   * @return 复位成功返回 `true`。
   */
  bool Reset(bool wait_if_busy = true,
             uint32_t wait_timeout_ms = kDefaultChipEraseTimeoutMs) {
    if (wait_if_busy && !WaitWhileBusy(wait_timeout_ms)) {
      return false;
    }

    if (!RunCommand(w25q32_detail::kCommandEnableReset)) {
      return false;
    }

    if (!RunCommand(w25q32_detail::kCommandResetDevice)) {
      return false;
    }

    tick::DelayUs(kResetDelayUs);
    SetLastStatus(W25q32Status::kOk);
    return true;
  }

  /**
   * @brief 将扇区索引转换为起始地址。
   *
   * @param sector_index 扇区索引。
   * @return 扇区起始地址。
   */
  static constexpr uint32_t SectorAddress(uint32_t sector_index) {
    return sector_index * kSectorSize;
  }

  /**
   * @brief 将 32KB 块索引转换为起始地址。
   *
   * @param block_index 32KB 块索引。
   * @return 32KB 块起始地址。
   */
  static constexpr uint32_t Block32Address(uint32_t block_index) {
    return block_index * kBlock32Size;
  }

  /**
   * @brief 将 64KB 块索引转换为起始地址。
   *
   * @param block_index 64KB 块索引。
   * @return 64KB 块起始地址。
   */
  static constexpr uint32_t Block64Address(uint32_t block_index) {
    return block_index * kBlock64Size;
  }

  /**
   * @brief 获取指定地址所在页的起始地址。
   *
   * @param address 任意芯片地址。
   * @return 页起始地址。
   */
  static constexpr uint32_t PageStartAddress(uint32_t address) {
    return address & ~(kPageSize - 1U);
  }

  /**
   * @brief 获取指定地址所在扇区的起始地址。
   *
   * @param address 任意芯片地址。
   * @return 扇区起始地址。
   */
  static constexpr uint32_t SectorStartAddress(uint32_t address) {
    return address & ~(kSectorSize - 1U);
  }

  /**
   * @brief 获取指定地址所在 32KB 块的起始地址。
   *
   * @param address 任意芯片地址。
   * @return 32KB 块起始地址。
   */
  static constexpr uint32_t Block32StartAddress(uint32_t address) {
    return address & ~(kBlock32Size - 1U);
  }

  /**
   * @brief 获取指定地址所在 64KB 块的起始地址。
   *
   * @param address 任意芯片地址。
   * @return 64KB 块起始地址。
   */
  static constexpr uint32_t Block64StartAddress(uint32_t address) {
    return address & ~(kBlock64Size - 1U);
  }

  /**
   * @brief 计算当前地址到页边界的剩余字节数。
   *
   * @param address 任意芯片地址。
   * @return 当前页剩余可写字节数。
   */
  static constexpr uint32_t PageBytesUntilBoundary(uint32_t address) {
    return kPageSize - (address & (kPageSize - 1U));
  }

private:
  /**
   * @brief 更新最近一次驱动状态。
   *
   * @param status 新状态。
   */
  void SetLastStatus(W25q32Status status) {
    last_status_ = status;
  }

  /**
   * @brief 判断等待过程是否超时。
   *
   * @param start_ms 起始毫秒时间戳。
   * @param timeout_ms 超时时间，单位为毫秒。
   * @return 已超时返回 `true`。
   */
  bool HasTimedOut(uint32_t start_ms, uint32_t timeout_ms) const {
    return (timeout_ms != kWaitForever) && (tick::ElapsedMs(start_ms) >= timeout_ms);
  }

  /**
   * @brief 确认芯片当前空闲。
   *
   * @return 空闲返回 `true`。
   */
  bool EnsureIdle() {
    bool busy = false;
    if (!IsBusy(&busy)) {
      return false;
    }

    if (busy) {
      SetLastStatus(W25q32Status::kBusy);
      return false;
    }

    SetLastStatus(W25q32Status::kOk);
    return true;
  }

  /**
   * @brief 开始一次 SPI 事务并拉低片选。
   *
   * @return 事务开始成功返回 `true`。
   */
  bool BeginTransaction() {
    if (!IsReady()) {
      SetLastStatus(W25q32Status::kNotReady);
      return false;
    }

    if (!chip_select_.Select()) {
      SetLastStatus(W25q32Status::kSpiError);
      return false;
    }

    return true;
  }

  /**
   * @brief 结束一次 SPI 事务并释放片选。
   */
  void EndTransaction() {
    (void)chip_select_.Release();
  }

  /**
   * @brief 发送命令后读取数据。
   *
   * @param command 命令缓冲区。
   * @param command_length 命令长度，单位为字节。
   * @param rx_data 接收缓冲区。
   * @param rx_length 读取长度，单位为字节。
   * @return 传输成功返回 `true`。
   */
  bool TransferCommandRead(const uint8_t *command,
                           uint32_t command_length,
                           uint8_t *rx_data,
                           uint32_t rx_length) {
    if (((command == nullptr) && (command_length > 0U)) ||
        ((rx_data == nullptr) && (rx_length > 0U))) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    if (!BeginTransaction()) {
      return false;
    }

    bool ok = true;
    if (command_length > 0U) {
      ok = spi_.Write(command, command_length) == command_length;
    }

    if (ok && (rx_length > 0U)) {
      ok = spi_.Read(rx_data, rx_length) == rx_length;
    }

    EndTransaction();
    SetLastStatus(ok ? W25q32Status::kOk : W25q32Status::kSpiError);
    return ok;
  }

  /**
   * @brief 发送命令后写入数据。
   *
   * @param command 命令缓冲区。
   * @param command_length 命令长度，单位为字节。
   * @param tx_data 待写入数据。
   * @param tx_length 写入长度，单位为字节。
   * @return 传输成功返回 `true`。
   */
  bool TransferCommandWrite(const uint8_t *command,
                            uint32_t command_length,
                            const uint8_t *tx_data,
                            uint32_t tx_length) {
    if (((command == nullptr) && (command_length > 0U)) ||
        ((tx_data == nullptr) && (tx_length > 0U))) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    if (!BeginTransaction()) {
      return false;
    }

    bool ok = true;
    if (command_length > 0U) {
      ok = spi_.Write(command, command_length) == command_length;
    }

    if (ok && (tx_length > 0U)) {
      ok = spi_.Write(tx_data, tx_length) == tx_length;
    }

    EndTransaction();
    SetLastStatus(ok ? W25q32Status::kOk : W25q32Status::kSpiError);
    return ok;
  }

  /**
   * @brief 发送无数据命令。
   *
   * @param command 命令码。
   * @return 命令发送成功返回 `true`。
   */
  bool RunCommand(uint8_t command) {
    return TransferCommandWrite(&command, 1U, nullptr, 0U);
  }

  /**
   * @brief 读取单字节状态寄存器。
   *
   * @param command 状态寄存器读取命令。
   * @param status 输出状态寄存器值。
   * @return 读取成功返回 `true`。
   */
  bool ReadStatusRegister(uint8_t command, uint8_t *status) {
    if (status == nullptr) {
      SetLastStatus(W25q32Status::kInvalidArgument);
      return false;
    }

    return TransferCommandRead(&command, 1U, status, 1U);
  }

  /**
   * @brief 执行带 24 位地址的擦除命令。
   *
   * @param command 擦除命令码。
   * @param address 目标地址。
   * @param wait_complete 是否等待擦除完成。
   * @param timeout_ms 等待完成超时时间，单位为毫秒。
   * @return 擦除命令启动或完成成功返回 `true`。
   */
  bool EraseAddressCommand(uint8_t command,
                           uint32_t address,
                           bool wait_complete,
                           uint32_t timeout_ms) {
    if (address >= kCapacityBytes) {
      SetLastStatus(W25q32Status::kOutOfRange);
      return false;
    }

    if (!EnsureIdle() || !WriteEnable()) {
      return false;
    }

    uint8_t command_buffer[4] {};
    w25q32_detail::FillCommandAddress(command, address, command_buffer);

    if (!TransferCommandWrite(command_buffer, sizeof(command_buffer), nullptr, 0U)) {
      return false;
    }

    return !wait_complete || WaitWhileBusy(timeout_ms);
  }

  SpiMaster spi_ {}; /**< SPI 总线对象。 */
  SpiChipSelect chip_select_ {}; /**< SPI 片选对象。 */
  bool initialized_ = false; /**< 驱动是否已经完成初始化。 */
  W25q32Status last_status_ = W25q32Status::kOk; /**< 最近一次驱动状态。 */
};

template <SpiPortId SpiPortValue,
          GpioPortId CsPortValue,
          GpioPinId CsPinValue,
          GpioPinState CsActiveStateValue = GpioPinState::kReset>
using w25q32 = W25q32<SpiPortValue, CsPortValue, CsPinValue, CsActiveStateValue>;

} // namespace iFly

#endif /* IFLY_APP_FLASH_W25Q32_HPP */
﻿
