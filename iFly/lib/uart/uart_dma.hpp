/**
 * @file uart_dma.hpp
 * @brief UART DMA 底层服务接口。
 */
#ifndef IFLY_UART_DMA_HPP
#define IFLY_UART_DMA_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"
#include "stm32f4xx_hal.h"

namespace iFly {

/**
 * @brief 软件层统一定义的 UART 逻辑端口编号。
 */
enum class UartPortId : uint8_t {
  kUart1 = 0U, /**< 逻辑 USART1。 */
  kUart2 = 1U, /**< 逻辑 USART2。 */
  kUart3 = 2U, /**< 逻辑 USART3。 */
  kUart4 = 3U, /**< 逻辑 UART4。 */
  kUart5 = 4U, /**< 逻辑 UART5。 */
  kUart6 = 5U, /**< 逻辑 USART6。 */
  kUart7 = 6U, /**< 逻辑 UART7。 */
  kUart8 = 7U, /**< 逻辑 UART8。 */
  kCount = 8U /**< 逻辑端口总数。 */
};

/**
 * @brief 将 UART 逻辑端口编号转换为可读字符串。
 *
 * @param port UART 逻辑端口编号。
 * @return 对应的字符串常量。
 */
const char *ToString(UartPortId port);

/**
 * @brief UART DMA 发送方向双缓冲区基类。
 */
class UartDmaTxDoubleBufferBase {
public:
  UartDmaTxDoubleBufferBase() = default;

  UartDmaTxDoubleBufferBase(const UartDmaTxDoubleBufferBase &) = delete;
  UartDmaTxDoubleBufferBase &operator=(const UartDmaTxDoubleBufferBase &) = delete;

  /**
   * @brief 使用外部存储区创建双缓冲区。
   *
   * @param buffer0 第 0 个缓冲槽首地址。
   * @param buffer1 第 1 个缓冲槽首地址。
   * @param bufferSize 单个缓冲槽大小。
   * @return 创建成功返回 `true`。
   */
  bool Create(uint8_t *buffer0, uint8_t *buffer1, uint16_t bufferSize);

  /**
   * @brief 解除双缓冲区与外部存储区的绑定。
   */
  void Delete();

  /**
   * @brief 清空双缓冲区长度状态。
   */
  void Clear();

  /**
   * @brief 判断双缓冲区是否已经创建成功。
   *
   * @return 已创建返回 `true`。
   */
  bool IsCreated() const;

  /**
   * @brief 获取单个缓冲槽的有效包长。
   *
   * @return 当前有效包长。
   */
  uint16_t PacketSize() const;

  /**
   * @brief 获取备用缓冲槽的可写指针。
   *
   * @return 备用缓冲槽首地址。
   */
  uint8_t *InactiveBuffer();

  /**
   * @brief 设置备用缓冲槽的有效长度。
   *
   * @param length 新的备用长度。
   */
  void SetInactiveLength(uint16_t length);

  /**
   * @brief 获取备用缓冲槽的有效长度。
   *
   * @return 当前备用长度。
   */
  uint16_t InactiveLength() const;

  /**
   * @brief 清空当前活动缓冲槽的长度。
   */
  void ClearActive();

  /**
   * @brief 判断备用缓冲槽是否存在有效数据。
   *
   * @return 存在有效数据返回 `true`。
   */
  bool HasInactiveData() const;

  /**
   * @brief 交换活动缓冲槽与备用缓冲槽。
   */
  void SwapBuffers();

private:
  /**
   * @brief 重置两个缓冲槽的长度状态。
   */
  void ResetLengths();

  /**
   * @brief 获取当前备用缓冲槽索引。
   *
   * @return 备用缓冲槽索引。
   */
  uint8_t InactiveSlotIndex() const;

  uint8_t *buffers_[2] {}; /**< 两个外部缓冲槽首地址。 */
  uint16_t bufferSize_ = 0U; /**< 单个缓冲槽大小。 */
  uint16_t lengths_[2] {}; /**< 两个缓冲槽各自的长度信息。 */
  uint8_t activeSlot_ = 0U; /**< 当前活动缓冲槽索引。 */
};

/**
 * @brief 硬件 UART DMA 传输服务。
 */
class UartDmaService final {
public:
  static constexpr uint8_t kMaxPorts = 8U; /**< 最大逻辑端口数。 */
  static constexpr uint32_t kDefaultTxQueueStorageSize = 120U; /**< 默认发送队列总容量。 */
  static constexpr uint16_t kDefaultTxDmaBufferSize = 120U; /**< 默认发送 DMA 双缓冲区大小。 */
  static constexpr uint16_t kDefaultRxDmaBufferSize = 256U; /**< 默认接收 DMA 环形缓冲区大小。 */

  /**
   * @brief 获取 UART DMA 服务单例。
   *
   * @return 单例引用。
   */
  static UartDmaService &Instance();

  /**
   * @brief 手动绑定逻辑串口与底层 HAL UART 句柄。
   *
   * @param port 逻辑 UART 端口编号。
   * @param huart HAL UART 句柄。
   */
  void AttachHardware(UartPortId port, UART_HandleTypeDef *huart);

  /**
   * @brief 初始化指定串口。
   *
   * @param port 逻辑 UART 端口编号。
   * @param rxQueue 上层统一接收队列。
   * @param txQueue 发送方向字节队列。
   * @param txBuffers 发送方向 DMA 双缓冲区。
   * @param rxDmaBuffer 接收方向 DMA 环形缓冲区。
   * @param rxDmaBufferSize 接收方向 DMA 环形缓冲区大小。
   * @return 初始化成功返回 `true`。
   */
  bool InitPort(UartPortId port,
                LockFreeQueueBase *rxQueue,
                LockFreeQueueBase *txQueue,
                UartDmaTxDoubleBufferBase *txBuffers,
                uint8_t *rxDmaBuffer,
                uint16_t rxDmaBufferSize);

  /**
   * @brief 反初始化指定串口。
   *
   * @param port 逻辑 UART 端口编号。
   */
  void DeinitPort(UartPortId port);

  /**
   * @brief 向指定串口写入待发送数据。
   *
   * @param port 逻辑 UART 端口编号。
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际写入的字节数。
   */
  uint32_t Write(UartPortId port, const uint8_t *data, uint32_t len);

  /**
   * @brief 获取发送队列剩余空间。
   *
   * @param port 逻辑 UART 端口编号。
   * @return 剩余可写字节数。
   */
  uint32_t TxFree(UartPortId port) const;

  /**
   * @brief 获取发送队列已用空间。
   *
   * @param port 逻辑 UART 端口编号。
   * @return 已使用字节数。
   */
  uint32_t TxUsed(UartPortId port) const;

  /**
   * @brief 获取接收链路累计丢字节数。
   *
   * @param port 逻辑 UART 端口编号。
   * @return 累计丢失字节数。
   */
  uint32_t RxDropped(UartPortId port) const;

  /**
   * @brief 判断指定串口是否已准备就绪。
   *
   * @param port 逻辑 UART 端口编号。
   * @return 就绪返回 `true`。
   */
  bool IsReady(UartPortId port) const;

  /**
   * @brief 处理 HAL 接收事件。
   *
   * @param huart HAL UART 句柄。
   * @param size 本次接收事件报告的字节数。
   */
  void OnRxEvent(UART_HandleTypeDef *huart, uint16_t size);

  /**
   * @brief 处理 HAL 发送完成事件。
   *
   * @param huart HAL UART 句柄。
   */
  void OnTxComplete(UART_HandleTypeDef *huart);

  /**
   * @brief 处理 HAL 错误事件。
   *
   * @param huart HAL UART 句柄。
   */
  void OnError(UART_HandleTypeDef *huart);

private:
  UartDmaService() = default;
};

/**
 * @brief 带静态存储区的 UART DMA 发送双缓冲区。
 *
 * @tparam kBufferSize 单个缓冲槽的固定容量。
 */
template <uint16_t kBufferSize = UartDmaService::kDefaultTxDmaBufferSize>
class UartDmaTxDoubleBuffer final : public UartDmaTxDoubleBufferBase {
public:
  static_assert(kBufferSize > 0U, "kBufferSize must be greater than 0.");

  /**
   * @brief 构造时自动绑定内部静态缓冲区。
   */
  UartDmaTxDoubleBuffer() {
    Recreate();
  }

  /**
   * @brief 重新绑定内部静态缓冲区并重置状态。
   */
  void Recreate() {
    (void)Create(storage_[0], storage_[1], kBufferSize);
  }

private:
  uint8_t storage_[2][kBufferSize] {}; /**< 两个固定大小的发送缓冲槽。 */
};

} // namespace iFly

#endif /* IFLY_UART_DMA_HPP */
