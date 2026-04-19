#ifndef IFLY_UART_DMA_HPP
#define IFLY_UART_DMA_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"

struct __UART_HandleTypeDef;
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

namespace iFly {

/**
 * @brief 软件层统一定义的 UART 端口编号。
 *
 * @details
 * 这里强调的是“软件支持 8 路槽位”，而不是当前芯片一定真的有 8 路 UART 外设。
 * 当前工程里：
 * - `USART1/2/3/UART4/5/USART6` 已能映射到底层 `huart*`
 * - `UART7/8` 先保留逻辑编号，后续如果芯片或工程支持，可以继续接入
 */
enum class UartPortId : uint8_t {
  kUsart1 = 0U,
  kUsart2 = 1U,
  kUsart3 = 2U,
  kUart4 = 3U,
  kUart5 = 4U,
  kUsart6 = 5U,
  kUart7 = 6U,
  kUart8 = 7U,
  kCount = 8U
};

const char *ToString(UartPortId port);

/**
 * @brief 硬件 UART DMA 传输服务。
 *
 * @details
 * 这个类是硬件串口底层的核心管理器，主要负责三件事：
 *
 * 1. 维护每个逻辑端口的运行时状态
 *    包括 `UART_HandleTypeDef`、RX 用户队列、TX 无锁队列、DMA 缓冲等。
 *
 * 2. 处理 TX 方向
 *    数据流是：
 *    `上层 Write() -> TX 无锁队列 -> TX 双缓冲 DMA staging -> HAL_UART_Transmit_DMA()`
 *
 * 3. 处理 RX 方向
 *    数据流是：
 *    `UART DMA 环形缓冲 -> HAL_UARTEx_RxEventCallback() -> 用户层 RX 无锁队列`
 *
 * 这样分层后，上层 `HardwareUart` 不需要接触 HAL 细节，
 * 只要像使用 USB CDC 那样调统一接口即可。
 */
class UartDmaService final {
public:
  /** @brief 软件最大支持的端口数。 */
  static constexpr uint8_t kMaxPorts = 8U;
  /** @brief 默认 TX 无锁队列总大小。 */
  static constexpr uint32_t kFixedTxQueueStorageSize = 120U;
  /** @brief 默认单个 TX DMA 分包缓冲大小。 */
  static constexpr uint16_t kFixedTxDmaBufferSize = 120U;
  /** @brief 默认 RX DMA 环形缓冲区大小。 */
  static constexpr uint16_t kFixedRxDmaBufferSize = 120U;

  /** @brief 获取单例。 */
  static UartDmaService &Instance();

  /**
   * @brief 手动把一个端口绑定到具体 `UART_HandleTypeDef`。
   *
   * @details
   * 当前实现即使不显式调用，也会在 `InitPort()` 里按默认映射自动查找；
   * 这个接口主要是给后续扩展或特殊板级重绑定预留的。
   */
  void AttachHardware(UartPortId port, UART_HandleTypeDef *huart);
  /**
   * @brief 初始化某个端口。
   *
   * @param port               逻辑端口号
   * @param rxQueue            上层统一 RX 无锁队列
   * @param txQueueStorageSize 底层 TX 队列大小
   * @param txDmaBufferSize    TX 双缓冲每个槽位的大小
   * @param rxDmaBufferSize    RX DMA 环形缓冲区大小
   *
   * @return 初始化成功返回 true。
   */
  bool InitPort(UartPortId port, LockFreeQueueBase *rxQueue);
  /** @brief 反初始化某个端口，释放动态缓冲并停止接收。 */
  void DeinitPort(UartPortId port);

  /** @brief 向某个端口的发送无锁队列写入数据。 */
  uint32_t Write(UartPortId port, const uint8_t *data, uint32_t len);
  /** @brief 查询某个端口 TX 队列剩余空间。 */
  uint32_t TxFree(UartPortId port) const;
  /** @brief 查询某个端口 TX 队列已用空间。 */
  uint32_t TxUsed(UartPortId port) const;
  /** @brief 查询某个端口 RX 上抛过程中的累计丢字节数。 */
  uint32_t RxDropped(UartPortId port) const;
  /** @brief 查询某个端口是否已经初始化完毕且具备 DMA RX/TX 资源。 */
  bool IsReady(UartPortId port) const;

  /** @brief HAL UART 接收事件回调入口。 */
  void OnRxEvent(UART_HandleTypeDef *huart, uint16_t size);
  /** @brief HAL UART DMA 发送完成回调入口。 */
  void OnTxComplete(UART_HandleTypeDef *huart);
  /** @brief HAL UART 错误回调入口。 */
  void OnError(UART_HandleTypeDef *huart);

private:
  UartDmaService() = default;
};

} // namespace iFly

#endif /* IFLY_UART_DMA_HPP */
