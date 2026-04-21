/**
 * @file can.hpp
 * @brief CAN 底层服务接口。
 */
#ifndef IFLY_CAN_HPP
#define IFLY_CAN_HPP

#include <stdint.h>

#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 软件层统一定义的 CAN 逻辑端口号。
 */
enum class CanPortId : uint8_t {
  kCan1 = 0U, /**< 逻辑 CAN1 端口。 */
  kCan2 = 1U, /**< 逻辑 CAN2 端口。 */
  kCount = 2U /**< 逻辑端口总数。 */
};

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

static_assert(sizeof(CanFramePacket) == 16U, "CanFramePacket size must stay fixed at 16 bytes.");

/**
 * @brief CAN 运行时服务。
 */
class CanService final {
public:
  static constexpr uint8_t kMaxPorts = 2U; /**< 最大逻辑端口数。 */
  static constexpr uint32_t kCanFramePacketSize = sizeof(CanFramePacket); /**< 单帧软件封包大小。 */
  static constexpr uint32_t kFixedTxQueueFrameCount = 64U; /**< 发送队列最大缓存帧数。 */
  static constexpr uint32_t kFixedTxQueueStorageSize =
      (kFixedTxQueueFrameCount * kCanFramePacketSize) + 1U; /**< 发送队列总存储大小。 */

  static CanService &Instance();
  void AttachHardware(CanPortId port, void *hcan);
  bool InitPort(CanPortId port, LockFreeQueueBase *rxQueue);
  void DeinitPort(CanPortId port);
  uint32_t Write(CanPortId port, const uint8_t *data, uint32_t len);
  bool WriteFrame(CanPortId port, const CanFramePacket &frame);
  uint32_t TxFree(CanPortId port) const;
  uint32_t TxUsed(CanPortId port) const;
  uint32_t RxDropped(CanPortId port) const;
  bool IsReady(CanPortId port) const;
  void ServiceRxPath(CanPortId port);
  void OnRxFifoPending(void *hcan, uint32_t fifo);
  void OnRxFifoFull(void *hcan, uint32_t fifo);
  void OnTxComplete(void *hcan);
  void OnTxAbort(void *hcan);
  void OnError(void *hcan);

private:
  CanService() = default;
};

} // namespace iFly

#endif /* IFLY_CAN_HPP */
