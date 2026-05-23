/**
 * @file can.hpp
 * @brief CAN 底层服务接口。
 */
#ifndef IFLY_CAN_HPP
#define IFLY_CAN_HPP

#include <stdint.h>

#include "LockFreePool.hpp"
#include "stm32f4xx_hal.h"

namespace iFly {

/**
 * @brief 软件层统一定义的 CAN 逻辑端口编号。
 */
enum class CanPortId : uint8_t {
  kCan1 = 0U, /**< 逻辑 CAN1 端口。 */
  kCan2 = 1U, /**< 逻辑 CAN2 端口。 */
  kCount = 2U /**< 逻辑端口总数。 */
};

/**
 * @brief 将 CAN 逻辑端口编号转换为可读字符串。
 *
 * @param port CAN 逻辑端口编号。
 * @return 对应的字符串常量。
 */
const char *ToString(CanPortId port);

/**
 * @brief CAN 报文标志位定义。
 */
enum CanFrameFlags : uint8_t {
  kCanFrameFlagExtendedId = 1U << 0, /**< 使用扩展帧 ID。 */
  kCanFrameFlagRemoteFrame = 1U << 1, /**< 远程帧标志。 */
  kCanFrameFlagRxFifo1 = 1U << 2 /**< 接收来源为 FIFO1。 */
};

/**
 * @brief 软件层统一使用的 CAN 帧封装格式。
 */
struct CanFramePacket final {
  uint32_t id = 0U; /**< CAN 报文 ID。 */
  uint8_t dlc = 0U; /**< 数据长度码。 */
  uint8_t flags = 0U; /**< 报文标志位。 */
  uint8_t filterIndex = 0U; /**< 命中的硬件滤波器索引。 */
  uint8_t reserved = 0U; /**< 预留字段。 */
  uint8_t data[8] {}; /**< 最多 8 字节有效负载。 */
};

static_assert(sizeof(CanFramePacket) == 16U,
              "CanFramePacket size must stay fixed at 16 bytes.");

/**
 * @brief CAN 运行时服务。
 */
class CanService final {
public:
  static constexpr uint8_t kMaxPorts = 2U; /**< 最大逻辑端口数。 */
  static constexpr uint32_t kCanFramePacketSize =
      sizeof(CanFramePacket); /**< 单帧软件封包大小。 */

  /**
   * @brief 获取 CAN 服务单例。
   *
   * @return 单例引用。
   */
  static CanService &Instance();

  /**
   * @brief 手动绑定逻辑端口与底层 HAL CAN 句柄。
   *
   * @param port 逻辑 CAN 端口编号。
   * @param hcan HAL CAN 句柄。
   */
  void AttachHardware(CanPortId port, CAN_HandleTypeDef *hcan);

  /**
   * @brief 初始化指定 CAN 端口。
   *
   * @param port 逻辑 CAN 端口编号。
   * @param rxPool 上层接收方向的帧队列池。
   * @param txPool 上层发送方向的帧队列池。
   * @return 初始化成功返回 `true`。
   */
  bool InitPort(CanPortId port,
                LockFreePoolBase<CanFramePacket> *rxPool,
                LockFreePoolBase<CanFramePacket> *txPool);

  /**
   * @brief 反初始化指定 CAN 端口。
   *
   * @param port 逻辑 CAN 端口编号。
   */
  void DeinitPort(CanPortId port);

  /**
   * @brief 写入单帧 CAN 报文。
   *
   * @param port 逻辑 CAN 端口编号。
   * @param frame 待发送的 CAN 帧。
   * @return 写入成功返回 `true`。
   */
  bool WriteFrame(CanPortId port, const CanFramePacket &frame);

  /**
   * @brief 获取发送队列池剩余帧数。
   *
   * @param port 逻辑 CAN 端口编号。
   * @return 剩余可写帧数。
   */
  uint32_t TxFree(CanPortId port) const;

  /**
   * @brief 获取发送队列池已用帧数。
   *
   * @param port 逻辑 CAN 端口编号。
   * @return 已保存帧数。
   */
  uint32_t TxUsed(CanPortId port) const;

  /**
   * @brief 获取接收链路累计丢帧数。
   *
   * @param port 逻辑 CAN 端口编号。
   * @return 累计丢失帧数。
   */
  uint32_t RxDropped(CanPortId port) const;

  /**
   * @brief 判断指定 CAN 端口是否已准备就绪。
   *
   * @param port 逻辑 CAN 端口编号。
   * @return 就绪返回 `true`。
   */
  bool IsReady(CanPortId port) const;

  /**
   * @brief 处理 HAL 接收 FIFO 待处理事件。
   *
   * @param hcan HAL CAN 句柄。
   * @param fifo FIFO 编号。
   */
  void OnRxFifoPending(CAN_HandleTypeDef *hcan, uint32_t fifo);

  /**
   * @brief 处理 HAL 接收 FIFO 满事件。
   *
   * @param hcan HAL CAN 句柄。
   * @param fifo FIFO 编号。
   */
  void OnRxFifoFull(CAN_HandleTypeDef *hcan, uint32_t fifo);

  /**
   * @brief 处理 HAL 发送完成事件。
   *
   * @param hcan HAL CAN 句柄。
   */
  void OnTxComplete(CAN_HandleTypeDef *hcan);

  /**
   * @brief 处理 HAL 发送中止事件。
   *
   * @param hcan HAL CAN 句柄。
   */
  void OnTxAbort(CAN_HandleTypeDef *hcan);

  /**
   * @brief 处理 HAL 错误事件。
   *
   * @param hcan HAL CAN 句柄。
   */
  void OnError(CAN_HandleTypeDef *hcan);

private:
  CanService() = default;
};

} // namespace iFly

#endif /* IFLY_CAN_HPP */

