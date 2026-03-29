#ifndef IFLY_CAN_HPP
#define IFLY_CAN_HPP

#include <stdint.h>

#include "can.h"
#include "lock_free_queue.hpp"

namespace iFly {

enum class CanPortId : uint8_t {
  kCan1 = 0U,
  kCan2 = 1U,
  kCount = 2U
};

const char *ToString(CanPortId port) noexcept;

enum CanFrameFlags : uint8_t {
  kCanFrameFlagExtendedId = 1U << 0,
  kCanFrameFlagRemoteFrame = 1U << 1,
  kCanFrameFlagRxFifo1 = 1U << 2
};

/**
 * @brief CAN 上下层统一使用的固定帧封包格式。
 *
 * @details
 * - `Write()` 传入的数据必须按该结构体整帧排列。
 * - `Read()` 读出的数据也按该结构体整帧返回。
 * - 这样既保留了 `SerialIoBase` 的字节流接口，又不会丢失 CAN 帧边界。
 */
struct CanFramePacket final {
  uint32_t id = 0U;
  uint8_t dlc = 0U;
  uint8_t flags = 0U;
  uint8_t filterIndex = 0U;
  uint8_t reserved = 0U;
  uint8_t data[8] {};
};

static_assert(sizeof(CanFramePacket) == 16U, "CanFramePacket size must stay fixed at 16 bytes.");

class CanService final {
public:
  static constexpr uint8_t kMaxPorts = 2U;
  static constexpr uint32_t kCanFramePacketSize = sizeof(CanFramePacket);
  static constexpr uint32_t kFixedTxQueueFrameCount = 16U;
  static constexpr uint32_t kFixedTxQueueStorageSize =
      (kFixedTxQueueFrameCount * kCanFramePacketSize) + 1U;
  static constexpr uint8_t kFixedRxRingFrameCount = 16U;

  static CanService &Instance() noexcept;

  void AttachHardware(CanPortId port, CAN_HandleTypeDef *hcan) noexcept;
  bool InitPort(CanPortId port, LockFreeQueueBase *rxQueue) noexcept;
  void DeinitPort(CanPortId port) noexcept;

  uint32_t Write(CanPortId port, const uint8_t *data, uint32_t len) noexcept;
  bool WriteFrame(CanPortId port, const CanFramePacket &frame) noexcept;
  uint32_t TxFree(CanPortId port) const noexcept;
  uint32_t TxUsed(CanPortId port) const noexcept;
  uint32_t RxDropped(CanPortId port) const noexcept;
  bool IsReady(CanPortId port) const noexcept;

  void ServiceRxPath(CanPortId port) noexcept;

  void OnRxFifoPending(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept;
  void OnRxFifoFull(CAN_HandleTypeDef *hcan, uint32_t fifo) noexcept;
  void OnTxComplete(CAN_HandleTypeDef *hcan) noexcept;
  void OnTxAbort(CAN_HandleTypeDef *hcan) noexcept;
  void OnError(CAN_HandleTypeDef *hcan) noexcept;

private:
  CanService() noexcept = default;
};

} // namespace iFly

#endif /* IFLY_CAN_HPP */
