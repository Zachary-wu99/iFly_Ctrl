/**
 * @file led.hpp
 * @brief LED 控制接口。
 */
#ifndef IFLY_LED_HPP
#define IFLY_LED_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 软件层统一定义的 GPIO 逻辑端口编号。
 */
enum class GpioPortId : uint8_t {
  kA = 0U, /**< 逻辑 GPIOA 端口。 */
  kB = 1U, /**< 逻辑 GPIOB 端口。 */
  kC = 2U, /**< 逻辑 GPIOC 端口。 */
  kD = 3U, /**< 逻辑 GPIOD 端口。 */
  kE = 4U, /**< 逻辑 GPIOE 端口。 */
  kF = 5U, /**< 逻辑 GPIOF 端口。 */
  kG = 6U, /**< 逻辑 GPIOG 端口。 */
  kH = 7U, /**< 逻辑 GPIOH 端口。 */
  kI = 8U, /**< 逻辑 GPIOI 端口。 */
  kNone = 0xFFU /**< 未绑定 GPIO 端口。 */
};

/**
 * @brief 软件层统一定义的 GPIO 逻辑引脚编号。
 */
enum class GpioPinId : uint8_t {
  kPin0 = 0U, /**< 逻辑 GPIO 引脚 0。 */
  kPin1 = 1U, /**< 逻辑 GPIO 引脚 1。 */
  kPin2 = 2U, /**< 逻辑 GPIO 引脚 2。 */
  kPin3 = 3U, /**< 逻辑 GPIO 引脚 3。 */
  kPin4 = 4U, /**< 逻辑 GPIO 引脚 4。 */
  kPin5 = 5U, /**< 逻辑 GPIO 引脚 5。 */
  kPin6 = 6U, /**< 逻辑 GPIO 引脚 6。 */
  kPin7 = 7U, /**< 逻辑 GPIO 引脚 7。 */
  kPin8 = 8U, /**< 逻辑 GPIO 引脚 8。 */
  kPin9 = 9U, /**< 逻辑 GPIO 引脚 9。 */
  kPin10 = 10U, /**< 逻辑 GPIO 引脚 10。 */
  kPin11 = 11U, /**< 逻辑 GPIO 引脚 11。 */
  kPin12 = 12U, /**< 逻辑 GPIO 引脚 12。 */
  kPin13 = 13U, /**< 逻辑 GPIO 引脚 13。 */
  kPin14 = 14U, /**< 逻辑 GPIO 引脚 14。 */
  kPin15 = 15U, /**< 逻辑 GPIO 引脚 15。 */
  kNone = 0xFFU /**< 未绑定 GPIO 引脚。 */
};

/**
 * @brief LED 点亮有效电平。
 */
enum class LedActiveLevel : uint8_t {
  kHigh = 0U, /**< GPIO 输出高电平时 LED 点亮。 */
  kLow = 1U   /**< GPIO 输出低电平时 LED 点亮。 */
};

/**
 * @brief LED 引脚的物理电平状态。
 */
enum class LedPinState : uint8_t {
  kReset = 0U, /**< 引脚处于低电平。 */
  kSet = 1U    /**< 引脚处于高电平。 */
};

/**
 * @brief 单个 LED 的硬件绑定配置。
 */
struct LedConfig final {
  GpioPortId port = GpioPortId::kNone; /**< 目标 GPIO 逻辑端口，例如 `GpioPortId::kA`。 */
  GpioPinId pin = GpioPinId::kNone; /**< 目标 GPIO 逻辑引脚，例如 `GpioPinId::kPin8`。 */
  LedActiveLevel activeLevel = LedActiveLevel::kHigh; /**< LED 的有效电平配置。 */
  bool defaultOn = false; /**< 初始化后是否立即点亮。 */
};

/**
 * @brief 单个 LED 控制对象。
 */
class Led final {
public:
  Led() = default;

  /**
   * @brief 根据配置直接构造 LED 对象。
   *
   * @param config LED 初始化配置。
   */
  explicit Led(const LedConfig &config);

  /**
   * @brief 初始化 LED 绑定关系。
   *
   * @param config LED 初始化配置。
   * @param applyDefaultState 是否立即应用 `config.defaultOn`。
   * @return 初始化成功返回 `true`。
   */
  bool Init(const LedConfig &config, bool applyDefaultState = true);

  /**
   * @brief 解除当前 LED 的硬件绑定。
   */
  void Deinit();

  /**
   * @brief 判断 LED 是否已绑定有效硬件。
   *
   * @return 已完成初始化返回 `true`。
   */
  bool IsReady() const;

  /**
   * @brief 获取当前生效的配置。
   *
   * @return 已初始化时返回配置地址，否则返回 `nullptr`。
   */
  const LedConfig *GetConfig() const;

  /**
   * @brief 重新绑定 GPIO 端口和引脚。
   *
   * @param port GPIO 逻辑端口。
   * @param pin GPIO 逻辑引脚。
   */
  void AttachHardware(GpioPortId port, GpioPinId pin);

  /**
   * @brief 获取当前绑定的 GPIO 逻辑端口。
   *
   * @return GPIO 逻辑端口。
   */
  GpioPortId Handle() const;

  /**
   * @brief 获取当前绑定的 GPIO 逻辑引脚。
   *
   * @return GPIO 逻辑引脚。
   */
  GpioPinId Pin() const;

  /**
   * @brief 按逻辑亮灭状态设置 LED。
   *
   * @param on `true` 表示点亮，`false` 表示熄灭。
   * @return 设置成功返回 `true`。
   */
  bool Set(bool on) const;

  /**
   * @brief 点亮 LED。
   *
   * @return 操作成功返回 `true`。
   */
  bool On() const;

  /**
   * @brief 熄灭 LED。
   *
   * @return 操作成功返回 `true`。
   */
  bool Off() const;

  /**
   * @brief 翻转 LED 当前状态。
   *
   * @return 操作成功返回 `true`。
   */
  bool Toggle() const;

  /**
   * @brief 查询 LED 当前逻辑状态。
   *
   * @return 点亮返回 `true`，否则返回 `false`。
   */
  bool IsOn() const;

  /**
   * @brief 读取 LED 引脚的物理电平。
   *
   * @return 当前引脚电平。
   */
  LedPinState ReadPin() const;

private:
  /**
   * @brief 将逻辑亮灭状态转换为物理电平。
   *
   * @param on `true` 表示点亮。
   * @return 对应的物理电平值。
   */
  LedPinState LogicalToPhysical(bool on) const;

  /**
   * @brief 检查当前配置是否有效。
   *
   * @return 配置合法返回 `true`。
   */
  bool IsConfigValid() const;

  LedConfig config_ {}; /**< 当前 LED 的运行时配置。 */
  bool initialized_ = false; /**< 是否已完成有效初始化。 */
};

/**
 * @brief 批量 LED 控制器。
 */
class LedController final {
public:
  LedController() = default;

  /**
   * @brief 批量初始化一组 LED。
   *
   * @param leds LED 对象数组。
   * @param ledCount `leds` 数组元素个数。
   * @param configs LED 配置数组。
   * @param configCount `configs` 数组元素个数。
   * @param applyDefaultState 是否立即应用每个 LED 的默认状态。
   * @return 全部 LED 初始化成功返回 `true`。
   */
  bool Init(Led *leds,
            uint32_t ledCount,
            const LedConfig *configs,
            uint32_t configCount,
            bool applyDefaultState = true);

  /**
   * @brief 释放当前接管的所有 LED。
   */
  void Deinit();

  /**
   * @brief 获取当前受管 LED 数量。
   *
   * @return LED 数量。
   */
  uint32_t Count() const;

  /**
   * @brief 按索引获取某个 LED 对象。
   *
   * @param index LED 索引。
   * @return 索引有效时返回对象地址，否则返回 `nullptr`。
   */
  Led *At(uint32_t index) const;

  /**
   * @brief 设置指定 LED 的逻辑状态。
   *
   * @param index LED 索引。
   * @param on `true` 表示点亮。
   * @return 操作成功返回 `true`。
   */
  bool Set(uint32_t index, bool on) const;

  /**
   * @brief 点亮指定 LED。
   *
   * @param index LED 索引。
   * @return 操作成功返回 `true`。
   */
  bool On(uint32_t index) const;

  /**
   * @brief 熄灭指定 LED。
   *
   * @param index LED 索引。
   * @return 操作成功返回 `true`。
   */
  bool Off(uint32_t index) const;

  /**
   * @brief 翻转指定 LED 的状态。
   *
   * @param index LED 索引。
   * @return 操作成功返回 `true`。
   */
  bool Toggle(uint32_t index) const;

  /**
   * @brief 查询指定 LED 是否点亮。
   *
   * @param index LED 索引。
   * @return 点亮返回 `true`。
   */
  bool IsOn(uint32_t index) const;

  /**
   * @brief 统一设置全部 LED 的逻辑状态。
   *
   * @param on `true` 表示全部点亮。
   */
  void SetAll(bool on) const;

  /**
   * @brief 点亮全部 LED。
   */
  void AllOn() const;

  /**
   * @brief 熄灭全部 LED。
   */
  void AllOff() const;

private:
  Led *leds_ = nullptr; /**< 当前受管的 LED 对象数组。 */
  uint32_t count_ = 0U; /**< 当前受管的 LED 数量。 */
};

} // namespace iFly

#endif /* IFLY_LED_HPP */
