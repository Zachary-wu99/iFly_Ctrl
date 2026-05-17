/**
 * @file soft_timer.hpp
 * @brief 基于 SysTick 的软定时器服务接口。
 */
#ifndef IFLY_SOFT_TIMER_HPP
#define IFLY_SOFT_TIMER_HPP

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在 SysTick 中断中推进软定时器时基。
 */
void ifly_soft_timer_systick_tick(void);

#ifdef __cplusplus
}

namespace iFly {

/**
 * @brief 基于 SysTick 的软件定时器服务。
 */
class SoftTimerService final {
public:
  using TaskCallback = void (*)(void *context); /**< 定时任务回调签名。 */
  using TaskHandle = uint32_t; /**< 软定时任务句柄类型。 */

  static constexpr TaskHandle kInvalidTaskHandle = 0U; /**< 无效任务句柄。 */
  static constexpr uint8_t kHighestPriority = 0U; /**< 最高优先级。 */
  static constexpr uint8_t kLowestPriority = 255U; /**< 最低优先级。 */
  static constexpr uint8_t kMaxTasks = 16U; /**< 最大任务数量。 */
  static constexpr uint32_t kUseIntervalAsStartDelay =
      0xFFFFFFFFUL; /**< 使用周期作为首次启动延时。 */

  /**
   * @brief 软定时任务创建配置。
   */
  struct TaskConfig final {
    TaskCallback callback = nullptr; /**< 定时任务回调函数。 */
    void *context = nullptr; /**< 回调上下文指针。 */
    uint32_t interval_ms = 0U; /**< 任务周期，单位为毫秒。 */
    uint32_t start_delay_ms = kUseIntervalAsStartDelay; /**< 首次触发延时，单位为毫秒。 */
    uint8_t priority = kLowestPriority; /**< 调度优先级，值越小优先级越高。 */
    bool auto_reload = true; /**< 是否自动重装。 */
  };

  /**
   * @brief 获取软定时器服务单例。
   *
   * @return 单例引用。
   */
  static SoftTimerService &Instance();

  /**
   * @brief 创建一个软定时任务。
   *
   * @param config 任务配置。
   * @return 创建成功返回有效任务句柄，否则返回 `kInvalidTaskHandle`。
   */
  TaskHandle CreateTask(const TaskConfig &config);

  /**
   * @brief 请求延后当前正在执行的任务。
   *
   * @param delay_ms 下一次唤醒延时，单位为毫秒。
   * @return 请求成功返回 `true`。
   */
  bool DelayCurrentTask(uint32_t delay_ms);

  /**
   * @brief 删除指定任务。
   *
   * @param handle 目标任务句柄。
   * @return 删除成功返回 `true`。
   */
  bool DeleteTask(TaskHandle handle);

  /**
   * @brief 删除全部任务。
   */
  void DeleteAllTasks();

  /**
   * @brief 在主循环中派发所有已到期任务。
   *
   * @return 本次实际执行的任务数量。
   */
  uint32_t Dispatch();

  /**
   * @brief 获取当前软时基，单位为毫秒。
   *
   * @return 当前毫秒计数。
   */
  uint32_t Now() const;

  /**
   * @brief 在 SysTick 中断中递增软时基。
   */
  void OnSysTick();

  /**
   * @brief 判断任务句柄是否有效。
   *
   * @param handle 待检查句柄。
   * @return 句柄有效返回 `true`。
   */
  static bool IsValidTaskHandle(TaskHandle handle);

private:
  /**
   * @brief 软定时器任务槽位。
   */
  struct TaskSlot final {
    TaskCallback callback = nullptr; /**< 任务回调。 */
    void *context = nullptr; /**< 回调上下文指针。 */
    uint32_t interval_ms = 0U; /**< 固定触发周期，单位为毫秒。 */
    uint32_t next_release_tick = 0U; /**< 下一次唤醒时间点。 */
    uint32_t requested_delay_ms = 0U; /**< 当前任务请求的额外延时。 */
    uint32_t creation_order = 0U; /**< 创建顺序编号。 */
    uint32_t generation = 0U; /**< 槽位代数计数。 */
    uint8_t priority = kLowestPriority; /**< 当前任务优先级。 */
    bool allocated = false; /**< 槽位是否已分配。 */
    bool auto_reload = true; /**< 是否自动重装。 */
    bool running = false; /**< 当前是否处于回调执行中。 */
    bool delay_requested = false; /**< 是否已请求延后执行。 */
    bool pending_delete = false; /**< 是否等待回调结束后删除。 */
  };

  SoftTimerService() = default;

  /**
   * @brief 组合任务句柄。
   *
   * @param slot_index 槽位索引。
   * @param generation 槽位代数。
   * @return 组合后的任务句柄。
   */
  static TaskHandle MakeTaskHandle(uint8_t slot_index, uint32_t generation);

  /**
   * @brief 从任务句柄中提取槽位索引。
   *
   * @param handle 任务句柄。
   * @return 槽位索引。
   */
  static uint8_t ExtractTaskIndex(TaskHandle handle);

  /**
   * @brief 从任务句柄中提取槽位代数。
   *
   * @param handle 任务句柄。
   * @return 槽位代数。
   */
  static uint32_t ExtractTaskGeneration(TaskHandle handle);

  /**
   * @brief 查找当前已到期且可运行的任务槽位。
   *
   * @param now 当前毫秒时间戳。
   * @return 可运行任务槽位索引。
   */
  uint8_t FindReadyTaskIndex(uint32_t now) const;

  /**
   * @brief 判断句柄是否与指定槽位匹配。
   *
   * @param slot 待检查槽位。
   * @param handle 目标任务句柄。
   * @param slot_index 槽位索引。
   * @return 匹配返回 `true`。
   */
  bool IsHandleMatch(const TaskSlot &slot, TaskHandle handle,
                     uint8_t slot_index) const;

  /**
   * @brief 清空指定任务槽位。
   *
   * @param slot_index 槽位索引。
   */
  void ClearTaskSlot(uint8_t slot_index);

  volatile uint32_t tick_count_ = 0U; /**< 由 SysTick 驱动的 1ms 软时基。 */
  uint32_t creation_counter_ = 0U; /**< 任务创建序号。 */
  bool running_dispatch_ = false; /**< 是否正在执行 `Dispatch()`。 */
  uint8_t current_task_index_ = kMaxTasks; /**< 当前回调中的任务槽位索引。 */
  TaskHandle current_task_handle_ = kInvalidTaskHandle; /**< 当前回调中的任务句柄。 */
  TaskSlot tasks_[kMaxTasks] {}; /**< 固定大小任务表。 */
};

using SoftTimer = SoftTimerService;

} // namespace iFly

#endif

#endif /* IFLY_SOFT_TIMER_HPP */

