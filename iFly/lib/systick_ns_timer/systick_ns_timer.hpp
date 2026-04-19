#ifndef IFLY_SYSTICK_NS_TIMER_HPP
#define IFLY_SYSTICK_NS_TIMER_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 供 C 中断入口直接调用的 DWT 计时扩展钩子。
 *
 * @details
 * - 这个接口给 `Core/Src/stm32f4xx_it.c` 中的 `SysTick_Handler()` 使用。
 * - 它不再依赖 SysTick 做高精度计时，只是借助 1ms 节拍去检查
 *   `DWT->CYCCNT` 是否发生了 32 位回绕。
 */
void ifly_systick_ns_timer_tick(void);

#ifdef __cplusplus
}

namespace iFly {

/**
 * @brief 基于 `DWT->CYCCNT` 的高精度时间戳工具。
 *
 * @details
 * - `DWT->CYCCNT` 会按 CPU 时钟周期递增，适合做 ns/us 级计时。
 * - 由于 `CYCCNT` 只有 32 位，在 168MHz 下大约 25.6s 回绕一次，
 *   这里额外维护软件高 32 位，把它扩展成逻辑上的 64 位周期计数器。
 * - `SysTick` 钩子只负责定期检查并补高位，真正的时间读取直接来自 DWT，
 *   不需要像旧版那样在 `NowTicks()` 里短暂关中断。
 *
 * @note
 * - 返回值单位是 ns，但分辨率取决于 CPU 主频，不是真正物理 1ns。
 * - 例如 F405 在 168MHz 下，最小步进约为 5.95ns。
 */
class SysTickNsTimer final {
public:
  /** @brief 获取全局唯一实例。 */
  static SysTickNsTimer &Instance();

  /** @brief 在 SysTick 中断中调用，用于维护 `CYCCNT` 的回绕扩展。 */
  void OnSysTick();

  /** @brief 获取从计数器启动至今累计的 CPU 周期数。 */
  uint64_t NowTicks() const;
  /** @brief 获取从计数器启动至今的 ns 时间戳。 */
  uint64_t NowNs() const;
  /** @brief 计算从给定起始 ns 时间戳到当前的耗时。 */
  uint64_t ElapsedNs(uint64_t start_ns) const;

  /** @brief 获取 DWT 周期计数器的时钟频率，单位 Hz。 */
  uint32_t TickClockHz() const;
  /** @brief 获取单个 CPU 周期对应的最小 ns 步进下界。 */
  uint32_t TickPeriodNsFloor() const;

private:
  SysTickNsTimer() = default;

  /** @brief 确保 DWT 周期计数器已经使能。 */
  void EnsureEnabled() const;
  /** @brief 采样当前低 32 位计数，必要时推进软件高 32 位。 */
  void UpdateWrapState() const;

private:
  /**
   * @brief 软件维护的高 32 位回绕计数。
   *
   * @details
   * 每当检测到 `DWT->CYCCNT` 从大值跳回小值，就说明发生了一次 32 位回绕，
   * 这里加 1，用来和当前低 32 位拼成逻辑上的 64 位周期数。
   */
  mutable volatile uint32_t wrap_high_ = 0U;

  /**
   * @brief 最近一次用于判断回绕的低 32 位采样值。
   */
  mutable volatile uint32_t last_cycle_sample_ = 0U;

  /**
   * @brief DWT 计数器是否已完成初始化。
   */
  mutable volatile bool initialized_ = false;
};

} // namespace iFly

#endif

#endif /* IFLY_SYSTICK_NS_TIMER_HPP */
