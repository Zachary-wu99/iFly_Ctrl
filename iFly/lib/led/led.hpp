#ifndef IFLY_LED_HPP
#define IFLY_LED_HPP

#include <stdint.h>

#include "gpio.h"

namespace iFly {

/**
 * @brief LED 有效电平定义。
 *
 * @details
 * 不同硬件板上的 LED 驱动方式可能不同：
 * - `kHigh`：GPIO 输出高电平时，LED 点亮
 * - `kLow` ：GPIO 输出低电平时，LED 点亮
 *
 * 这样做可以兼容高电平点亮和低电平点亮两类硬件，无需改业务层逻辑。
 */
enum class LedActiveLevel : uint8_t {
  kHigh = 0U,
  kLow = 1U
};

/**
 * @brief 单个 LED 通道的配置表项。
 *
 * @details
 * `Led` 类本身不负责初始化 GPIO 复用、模式、上下拉等硬件属性，
 * 这些仍然由 CubeMX 生成的 HAL 初始化代码负责。
 *
 * 本结构体只描述“这个 LED 绑定到哪个 GPIO、什么电平点亮、默认是否亮”。
 */
struct LedConfig final {
  /** GPIO 端口，例如 GPIOA / GPIOB / GPIOC。*/
  GPIO_TypeDef *port = nullptr;
  /** GPIO 引脚掩码，例如 GPIO_PIN_8。*/
  uint16_t pin = 0U;
  /** LED 的有效电平。*/
  LedActiveLevel activeLevel = LedActiveLevel::kHigh;
  /** 初始化时是否默认点亮。*/
  bool defaultOn = false;
};

/**
 * @brief 单个 LED 对象。
 *
 * @details
 * 一个 `Led` 实例只管理一个 GPIO 输出引脚。
 * 通过配置表可以把它绑定到任意 HAL GPIO 引脚，而不依赖固定的
 * `LED_R`、`LED_G` 之类工程宏，因此更适合复用和移植。
 */
class Led final {
public:
  Led() = default;
  /** 使用配置表直接构造一个 LED 对象。*/
  explicit Led(const LedConfig &config);

  /** 绑定 LED 配置，并可选是否立即应用默认状态。*/
  bool Init(const LedConfig &config, bool applyDefaultState = true);
  /** 清除当前绑定配置。*/
  void Deinit();

  /** 查询当前 LED 是否已经完成配置绑定。*/
  bool IsReady() const;
  /** 获取当前 LED 的配置指针，未初始化时返回空指针。*/
  const LedConfig *GetConfig() const;

  /** 按逻辑状态设置 LED，`true` 表示亮，`false` 表示灭。*/
  bool Set(bool on) const;
  /** 点亮 LED。*/
  bool On() const;
  /** 熄灭 LED。*/
  bool Off() const;
  /** 翻转 LED 当前状态。*/
  bool Toggle() const;
  /** 读取 LED 当前逻辑状态，返回 `true` 表示亮。*/
  bool IsOn() const;
  /** 直接读取 GPIO 当前物理电平。*/
  GPIO_PinState ReadPin() const;

private:
  /** 把逻辑亮灭状态转换成实际 GPIO 输出电平。*/
  GPIO_PinState LogicalToPhysical(bool on) const;
  /** 检查当前配置是否有效。*/
  bool IsConfigValid() const;

  LedConfig config_ {};
  bool initialized_ = false;
};

/**
 * @brief 基于参数表的 LED 控制器。
 *
 * @details
 * 这个类适合批量管理一组 LED。
 * 它本身不做动态内存分配，调用者提供：
 * - `Led` 对象数组
 * - `LedConfig` 配置表
 *
 * 这样在更换板卡时，一般只需要调整配置表，不需要改控制逻辑。
 */
class LedController final {
public:
  LedController() = default;

  /** 使用对象数组和配置表批量初始化 LED。*/
  bool Init(Led *leds,
            uint32_t ledCount,
            const LedConfig *configs,
            uint32_t configCount,
            bool applyDefaultState = true);
  /** 反初始化所有受管 LED。*/
  void Deinit();

  /** 返回当前已接管的 LED 数量。*/
  uint32_t Count() const;
  /** 按索引获取某个 LED 对象，越界时返回空指针。*/
  Led *At(uint32_t index) const;

  /** 设置指定索引 LED 的逻辑状态。*/
  bool Set(uint32_t index, bool on) const;
  /** 点亮指定索引的 LED。*/
  bool On(uint32_t index) const;
  /** 熄灭指定索引的 LED。*/
  bool Off(uint32_t index) const;
  /** 翻转指定索引 LED 的状态。*/
  bool Toggle(uint32_t index) const;
  /** 查询指定索引 LED 当前是否点亮。*/
  bool IsOn(uint32_t index) const;

  /** 统一设置全部 LED 的逻辑状态。*/
  void SetAll(bool on) const;
  /** 点亮全部 LED。*/
  void AllOn() const;
  /** 熄灭全部 LED。*/
  void AllOff() const;

private:
  Led *leds_ = nullptr;
  uint32_t count_ = 0U;
};

} // namespace iFly

#endif /* IFLY_LED_HPP */
