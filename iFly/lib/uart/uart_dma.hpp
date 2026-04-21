/**
 * @file uart_dma.hpp
 * @brief UART DMA 底层服务接口。
 */
#ifndef IFLY_UART_DMA_HPP
#define IFLY_UART_DMA_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 软件层统一定义的 UART 逻辑端口号。
 */
enum class UartPortId : uint8_t {
  kUsart1 = 0U, /**< 逻辑 USART1。 */
  kUsart2 = 1U, /**< 逻辑 USART2。 */
  kUsart3 = 2U, /**< 逻辑 USART3。 */
  kUart4 = 3U, /**< 逻辑 UART4。 */
  kUart5 = 4U, /**< 逻辑 UART5。 */
  kUsart6 = 5U, /**< 逻辑 USART6。 */
  kUart7 = 6U, /**< 逻辑 UART7。 */
  kUart8 = 7U, /**< 逻辑 UART8。 */
  kCount = 8U /**< 逻辑端口总数。 */
};

const char *ToString(UartPortId port);

/**
 * @brief 硬件 UART DMA 传输服务。
 */
class UartDmaService final {
public:
  static constexpr uint8_t kMaxPorts = 8U; /**< 最大逻辑端口数。 */
  static constexpr uint32_t kFixedTxQueueStorageSize = 120U; /**< 默认发送队列总容量。 */
  static constexpr uint16_t kFixedTxDmaBufferSize = 120U; /**< 默认单个发送 DMA 缓冲长度。 */
  static constexpr uint16_t kFixedRxDmaBufferSize = 120U; /**< 默认接收 DMA 环形缓冲长度。 */

  static UartDmaService &Instance();

  /**
   * @brief 手动绑定逻辑串口与底层 HAL 句柄。
   *
   * @param port 逻辑串口号。
   * @param huart 底层 HAL UART 句柄。
   */
  void AttachHardware(UartPortId port, void *huart);

  /**
   * @brief 初始化指定串口。
   *
   * @param port 逻辑串口号。
   * @param rxQueue 上层统一接收队列。
   * @return 初始化成功返回 `true`。
   */
  bool InitPort(UartPortId port, LockFreeQueueBase *rxQueue);

  /**
   * @brief 反初始化指定串口。
   *
   * @param port 逻辑串口号。
   */
  void DeinitPort(UartPortId port);

  /**
   * @brief 向指定串口写入待发送数据。
   *
   * @param port 逻辑串口号。
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度。
   * @return 实际写入字节数。
   */
  uint32_t Write(UartPortId port, const uint8_t *data, uint32_t len);

  uint32_t TxFree(UartPortId port) const;
  uint32_t TxUsed(UartPortId port) const;
  uint32_t RxDropped(UartPortId port) const;
  bool IsReady(UartPortId port) const;
  void OnRxEvent(void *huart, uint16_t size);
  void OnTxComplete(void *huart);
  void OnError(void *huart);

private:
  UartDmaService() = default;
};

} // namespace iFly

#endif /* IFLY_UART_DMA_HPP */
