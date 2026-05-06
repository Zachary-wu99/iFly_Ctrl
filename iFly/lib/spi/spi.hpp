/**
 * @file spi.hpp
 * @brief SPI 抽象接口。
 */
#ifndef IFLY_SPI_HPP
#define IFLY_SPI_HPP

#include <stdint.h>

#include "gpio.hpp"

namespace iFly {

/**
 * @brief 软件层统一定义的 SPI 逻辑端口编号。
 */
enum class SpiPortId : uint8_t {
  kSpi1 = 0U, /**< 逻辑 SPI1。 */
  kSpi2 = 1U, /**< 逻辑 SPI2。 */
  kSpi3 = 2U, /**< 逻辑 SPI3。 */
  kCount = 3U /**< 逻辑端口总数，也用于表示未绑定。 */
};

/**
 * @brief 将 SPI 逻辑端口编号转换为可读字符串。
 *
 * @param port SPI 逻辑端口编号。
 * @return 对应的字符串常量。
 */
const char *ToString(SpiPortId port);

/**
 * @brief SPI 抽象层统一状态码。
 */
enum class SpiStatus : uint8_t {
  kOk = 0U, /**< 操作成功。 */
  kError = 1U, /**< 底层返回错误。 */
  kBusy = 2U, /**< 底层忙。 */
  kTimeout = 3U, /**< 操作超时。 */
  kNotReady = 4U, /**< 端口未就绪。 */
  kUnavailable = 5U /**< 当前工程未启用 SPI HAL。 */
};

/**
 * @brief SPI 运行时句柄。
 *
 * @details
 * 该对象只保存软件层 SPI 逻辑端口和超时时间。SPI 模式、波特率、NSS、位宽等
 * 硬件配置全部由 CubeMX/HAL 生成代码完成，上层不直接接触 HAL 原生句柄。
 */
class SpiMaster final {
public:
  static constexpr uint32_t kDefaultTimeoutMs = 100U; /**< 默认阻塞等待超时时间。 */

  /**
   * @brief SPI 句柄初始化配置。
   */
  struct Config final {
    SpiPortId port = SpiPortId::kCount; /**< SPI 逻辑端口。 */
    uint32_t timeout_ms = kDefaultTimeoutMs; /**< 阻塞接口超时时间。 */
  };

  SpiMaster() = default;

  /**
   * @brief 使用给定 SPI 逻辑端口直接构造对象。
   *
   * @param port SPI 逻辑端口。
   * @param timeout_ms 阻塞接口超时时间。
   */
  explicit SpiMaster(SpiPortId port, uint32_t timeout_ms = kDefaultTimeoutMs);

  /**
   * @brief 使用给定配置直接构造对象。
   *
   * @param config SPI 句柄初始化配置。
   */
  explicit SpiMaster(const Config &config);

  /**
   * @brief 绑定 SPI 逻辑端口。
   *
   * @param port SPI 逻辑端口。
   * @param timeout_ms 阻塞接口超时时间。
   * @return 绑定成功返回 `true`。
   */
  bool Init(SpiPortId port, uint32_t timeout_ms = kDefaultTimeoutMs);

  /**
   * @brief 使用给定配置绑定 SPI 逻辑端口。
   *
   * @param config SPI 句柄初始化配置。
   * @return 绑定成功返回 `true`。
   */
  bool Init(const Config &config);

  /**
   * @brief 解除当前 SPI 逻辑端口绑定。
   */
  void Deinit();

  /**
   * @brief 重新绑定 SPI 逻辑端口。
   *
   * @param port SPI 逻辑端口。
   */
  void AttachPort(SpiPortId port);

  /**
   * @brief 写入一段 SPI 数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际完成发送的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len);

  /**
   * @brief 读取一段 SPI 数据。
   *
   * @param data 接收缓冲区首地址。
   * @param len 期望读取长度，单位为字节。
   * @return 实际完成读取的字节数。
   */
  uint32_t Read(uint8_t *data, uint32_t len);

  /**
   * @brief 同步收发一段 SPI 数据。
   *
   * @param tx_data 待发送数据首地址，可为空。
   * @param rx_data 接收缓冲区首地址，可为空。
   * @param len 收发长度，单位为字节。
   * @return 实际完成收发的字节数。
   */
  uint32_t Transfer(const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

  /**
   * @brief 写入单字节数据。
   *
   * @param data 待发送字节。
   * @return 写入成功返回 `true`。
   */
  bool WriteByte(uint8_t data);

  /**
   * @brief 读取单字节数据。
   *
   * @param data 接收字节输出地址。
   * @return 读取成功返回 `true`。
   */
  bool ReadByte(uint8_t *data);

  /**
   * @brief 同步收发单字节数据。
   *
   * @param tx_data 待发送字节。
   * @param rx_data 接收字节输出地址。
   * @return 收发成功返回 `true`。
   */
  bool TransferByte(uint8_t tx_data, uint8_t *rx_data);

  /**
   * @brief 中止当前 SPI 传输。
   *
   * @return 中止成功返回 `true`。
   */
  bool Abort();

  /**
   * @brief 判断当前端口是否已绑定到底层有效 SPI 句柄。
   *
   * @return 端口可用返回 `true`。
   */
  bool IsReady() const;

  /**
   * @brief 判断当前 SPI 端口是否忙。
   *
   * @return 正在传输返回 `true`。
   */
  bool IsBusy() const;

  /**
   * @brief 获取当前绑定的 SPI 逻辑端口。
   *
   * @return SPI 逻辑端口。
   */
  SpiPortId Port() const;

  /**
   * @brief 获取最近一次底层调用返回状态。
   *
   * @return 最近一次状态。
   */
  SpiStatus LastStatus() const;

  /**
   * @brief 获取底层 SPI 错误码。
   *
   * @return 错误码。当前工程未启用 SPI HAL 时返回 0。
   */
  uint32_t ErrorCode() const;

  /**
   * @brief 获取当前阻塞接口超时时间。
   *
   * @return 超时时间，单位为毫秒。
   */
  uint32_t Timeout() const;

  /**
   * @brief 设置阻塞接口超时时间。
   *
   * @param timeout_ms 超时时间，单位为毫秒。
   */
  void SetTimeout(uint32_t timeout_ms);

private:
  /**
   * @brief 标记最近一次调用状态。
   *
   * @param status 新状态。
   */
  void SetLastStatus(SpiStatus status);

  SpiPortId port_ = SpiPortId::kCount; /**< 当前绑定的 SPI 逻辑端口。 */
  uint32_t timeout_ms_ = kDefaultTimeoutMs; /**< 当前阻塞接口超时时间。 */
  SpiStatus last_status_ = SpiStatus::kOk; /**< 最近一次底层调用状态。 */
};

/**
 * @brief SPI 片选引脚控制对象。
 */
class SpiChipSelect final {
public:
  /**
   * @brief 片选引脚配置。
   */
  struct Config final {
    GpioPortId port = GpioPortId::kNone; /**< 片选 GPIO 端口。 */
    GpioPinId pin = GpioPinId::kNone; /**< 片选 GPIO 引脚。 */
    GpioPinState active_state = GpioPinState::kReset; /**< 选中设备时的 GPIO 电平。 */
    bool inactive_on_init = true; /**< 初始化后是否立即释放片选。 */
  };

  SpiChipSelect() = default;

  /**
   * @brief 使用给定配置直接构造片选对象。
   *
   * @param config 片选引脚配置。
   */
  explicit SpiChipSelect(const Config &config);

  /**
   * @brief 初始化片选引脚绑定。
   *
   * @param config 片选引脚配置。
   * @return 初始化成功返回 `true`。
   */
  bool Init(const Config &config);

  /**
   * @brief 解除当前片选引脚绑定。
   */
  void Deinit();

  /**
   * @brief 重新绑定片选引脚。
   *
   * @param port 片选 GPIO 端口。
   * @param pin 片选 GPIO 引脚。
   * @param active_state 选中设备时的 GPIO 电平。
   */
  void AttachHardware(GpioPortId port,
                      GpioPinId pin,
                      GpioPinState active_state = GpioPinState::kReset);

  /**
   * @brief 拉有效片选。
   *
   * @return 操作成功返回 `true`。
   */
  bool Select();

  /**
   * @brief 释放片选。
   *
   * @return 操作成功返回 `true`。
   */
  bool Release();

  /**
   * @brief 判断片选引脚是否绑定有效。
   *
   * @return 绑定有效返回 `true`。
   */
  bool IsReady() const;

  /**
   * @brief 判断当前片选是否处于有效状态。
   *
   * @return 片选有效返回 `true`。
   */
  bool IsSelected() const;

private:
  /**
   * @brief 获取片选释放状态对应的 GPIO 电平。
   *
   * @return 片选释放电平。
   */
  GpioPinState InactiveState() const;

  /**
   * @brief 写入片选 GPIO 电平。
   *
   * @param state 目标 GPIO 电平。
   * @return 写入成功返回 `true`。
   */
  bool WriteState(GpioPinState state) const;

  GpioPortId port_ = GpioPortId::kNone; /**< 片选 GPIO 端口。 */
  GpioPinId pin_ = GpioPinId::kNone; /**< 片选 GPIO 引脚。 */
  GpioPinState active_state_ = GpioPinState::kReset; /**< 片选有效电平。 */
  bool selected_ = false; /**< 当前片选状态缓存。 */
};

/**
 * @brief 带可选片选控制的 SPI 设备对象。
 */
class SpiDevice final {
public:
  /**
   * @brief SPI 设备配置。
   */
  struct Config final {
    SpiMaster *bus = nullptr; /**< 设备所在 SPI 总线。 */
    SpiChipSelect *chip_select = nullptr; /**< 设备片选对象，可为空。 */
  };

  SpiDevice() = default;

  /**
   * @brief 使用给定配置直接构造 SPI 设备对象。
   *
   * @param config SPI 设备配置。
   */
  explicit SpiDevice(const Config &config);

  /**
   * @brief 初始化 SPI 设备对象。
   *
   * @param config SPI 设备配置。
   * @return 初始化成功返回 `true`。
   */
  bool Init(const Config &config);

  /**
   * @brief 解除当前设备绑定。
   */
  void Deinit();

  /**
   * @brief 开始一次 SPI 事务并拉有效片选。
   *
   * @return 操作成功返回 `true`。
   */
  bool BeginTransaction();

  /**
   * @brief 结束一次 SPI 事务并释放片选。
   */
  void EndTransaction();

  /**
   * @brief 写入一段 SPI 数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际完成发送的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len);

  /**
   * @brief 读取一段 SPI 数据。
   *
   * @param data 接收缓冲区首地址。
   * @param len 期望读取长度，单位为字节。
   * @return 实际完成读取的字节数。
   */
  uint32_t Read(uint8_t *data, uint32_t len);

  /**
   * @brief 同步收发一段 SPI 数据。
   *
   * @param tx_data 待发送数据首地址，可为空。
   * @param rx_data 接收缓冲区首地址，可为空。
   * @param len 收发长度，单位为字节。
   * @return 实际完成收发的字节数。
   */
  uint32_t Transfer(const uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

  /**
   * @brief 判断当前设备绑定是否有效。
   *
   * @return 绑定有效返回 `true`。
   */
  bool IsReady() const;

private:
  SpiMaster *bus_ = nullptr; /**< SPI 总线对象。 */
  SpiChipSelect *chip_select_ = nullptr; /**< SPI 片选对象。 */
};

using spi_master = SpiMaster;
using spi_chip_select = SpiChipSelect;
using spi_device = SpiDevice;

} // namespace iFly

#endif /* IFLY_SPI_HPP */
