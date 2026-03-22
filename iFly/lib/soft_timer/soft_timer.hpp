#ifndef IFLY_SOFT_TIMER_HPP
#define IFLY_SOFT_TIMER_HPP

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 供 SysTick 中断直接调用的 C 接口。
 * @details
 * - 该函数只做 1ms 软时基计数递增，不执行任何任务回调。
 * - 之所以保留为 C 接口，是为了便于在 `stm32f4xx_it.c` 中直接包含并调用。
 */
void ifly_soft_timer_systick_tick(void);

#ifdef __cplusplus
}

namespace iFly {

/**
 * @brief 基于 SysTick 的软件定时器服务。
 * @details
 * - SysTick 中断每 1ms 调用一次 `OnSysTick()`，仅维护软件 tick。
 * - 业务主循环中周期调用 `Dispatch()`，按“到期 + 优先级”规则执行任务。
 * - 调度器是非抢占式的：一个任务回调运行期间不会被另一个任务打断。
 * - 创建和删除任务时会短暂关中断，避免与 SysTick 并发访问内部表。
 */
class SoftTimerService final {
public:
  /** @brief 任务回调函数签名。 */
  using TaskCallback = void (*)(void *context);
  /** @brief 任务句柄类型。 */
  using TaskHandle = uint32_t;

  /** @brief 无效句柄，创建失败时返回该值。 */
  static constexpr TaskHandle kInvalidTaskHandle = 0U;
  /** @brief 最高优先级，数值越小优先级越高。 */
  static constexpr uint8_t kHighestPriority = 0U;
  /** @brief 最低优先级。 */
  static constexpr uint8_t kLowestPriority = 255U;
  /** @brief 当前实现支持的最大任务数。 */
  static constexpr uint8_t kMaxTasks = 16U;
  /** @brief 使用 `interval_ms` 作为首次触发延时的特殊标记值。 */
  static constexpr uint32_t kUseIntervalAsStartDelay = 0xFFFFFFFFUL;

  /**
   * @brief 创建任务时使用的配置结构体。
   * @details
   * - `callback`：任务函数，不能为空。
   * - `context`：回调参数，调度器原样传入。
   * - `interval_ms`：任务周期，单位 ms，必须大于 0。
   * - `start_delay_ms`：首次触发延时，默认等于 `interval_ms`。
   * - `priority`：非抢占式优先级，数值越小越先执行。
   * - `auto_reload`：`true` 表示周期任务，`false` 表示单次任务。
   */
  struct TaskConfig final {
    TaskCallback callback = nullptr;
    void *context = nullptr;
    uint32_t interval_ms = 0U;
    uint32_t start_delay_ms = kUseIntervalAsStartDelay;
    uint8_t priority = kLowestPriority;
    bool auto_reload = true;
  };

  /** @brief 获取单例对象。 */
  static SoftTimerService &Instance() noexcept;

  /**
   * @brief 创建一个软件定时任务。
   * @param config 任务配置。
   * @return 创建成功返回有效句柄，失败返回 `kInvalidTaskHandle`。
   */
  TaskHandle CreateTask(const TaskConfig &config) noexcept;

  /**
   * @brief 删除指定任务。
   * @details
   * - 若任务当前未在执行，则立即删除。
   * - 若任务正在回调中，则标记为“回调返回后删除”。
   */
  bool DeleteTask(TaskHandle handle) noexcept;

  /** @brief 删除全部任务。 */
  void DeleteAllTasks() noexcept;

  /**
   * @brief 在主循环中派发所有已到期任务。
   * @return 本次调用实际执行的任务数量。
   */
  uint32_t Dispatch() noexcept;

  /** @brief 获取当前软件 tick，单位 ms。 */
  uint32_t Now() const noexcept;

  /** @brief 由 SysTick 中断调用，递增软件 tick。 */
  void OnSysTick() noexcept;

  /** @brief 判断句柄是否为非零有效值。 */
  static bool IsValidTaskHandle(TaskHandle handle) noexcept;

private:
  /**
   * @brief 定时器任务槽位。
   * @details
   * 每个槽位代表一个逻辑任务，调度器通过固定数组管理，避免运行时动态分配内存。
   */
  struct TaskSlot final {
    TaskCallback callback = nullptr;
    void *context = nullptr;
    uint32_t interval_ms = 0U;
    uint32_t next_release_tick = 0U;
    uint32_t creation_order = 0U;
    uint32_t generation = 0U;
    uint8_t priority = kLowestPriority;
    bool allocated = false;
    bool auto_reload = true;
    bool running = false;
    bool pending_delete = false;
  };

  SoftTimerService() noexcept = default;

  /** @brief 根据槽位索引和代数编码出任务句柄。 */
  static TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation) noexcept;
  /** @brief 从句柄中解析槽位索引。 */
  static uint8_t ExtractTaskIndex(TaskHandle handle) noexcept;
  /** @brief 从句柄中解析任务代数。 */
  static uint32_t ExtractTaskGeneration(TaskHandle handle) noexcept;

  /**
   * @brief 在当前时刻选择最应该先执行的任务。
   * @details
   * 选择顺序：
   * 1. 只考虑已经到期的任务；
   * 2. 优先级数值更小者优先；
   * 3. 若优先级相同，则按创建顺序更早者优先。
   */
  uint8_t FindReadyTaskIndex(uint32_t now) const noexcept;

  /** @brief 判断句柄是否仍然匹配当前槽位中的任务。 */
  bool IsHandleMatch(const TaskSlot &slot, TaskHandle handle, uint8_t slot_index) const noexcept;

  /** @brief 清空一个槽位并释放它。 */
  void ClearTaskSlot(uint8_t slot_index) noexcept;

private:
  /** @brief 由 SysTick 驱动的 1ms 递增软件时间基准。 */
  volatile uint32_t tick_count_ = 0U;
  /** @brief 任务创建序号，用于同优先级下的先后排序。 */
  uint32_t creation_counter_ = 0U;
  /** @brief 防止 `Dispatch()` 重入。 */
  bool running_dispatch_ = false;
  /** @brief 固定大小任务表。 */
  TaskSlot tasks_[kMaxTasks] {};
};

/** @brief 简写别名，方便业务层使用。 */
using SoftTimer = SoftTimerService;

} // namespace iFly
#endif

#endif /* IFLY_SOFT_TIMER_HPP */
