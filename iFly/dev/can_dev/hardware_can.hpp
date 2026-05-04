/**
 * @file hardware_can.hpp
 * @brief 硬件 CAN 设备封装接口。
 */
#ifndef IFLY_HARDWARE_CAN_HPP
#define IFLY_HARDWARE_CAN_HPP

#include <stdint.h>

#include "can.hpp"

namespace iFly {

static constexpr uint32_t kDefaultCanRxPoolFrameCount = 30U; /**< 默认 CAN 接收对象池帧数。 */
static constexpr uint32_t kDefaultCanTxPoolFrameCount = 20U; /**< 默认 CAN 发送对象池帧数。 */

/**
 * @brief 面向业务层的单路 CAN 设备封装。
 *
 * @details
 * 该类把 CAN 端口封装成面向 CAN 帧的直接读写接口。
 *
 * @tparam kRxPoolFrameCount 接收对象池帧数。
 * @tparam kTxPoolFrameCount 发送对象池帧数。
 */
template <uint32_t kRxPoolFrameCount = kDefaultCanRxPoolFrameCount,
          uint32_t kTxPoolFrameCount = kDefaultCanTxPoolFrameCount>
class HardwareCan final {
public:
  static_assert(kRxPoolFrameCount > 0U,
                "kRxPoolFrameCount must be greater than 0.");
  static_assert(kTxPoolFrameCount > 0U,
                "kTxPoolFrameCount must be greater than 0.");

  /**
   * @brief 构造一个逻辑 CAN 设备对象。
   *
   * @param port 逻辑 CAN 端口号。
   */
  explicit HardwareCan(CanPortId port) : port_(port) {
  }

  /**
   * @brief 初始化当前 CAN 端口。
   */
  void Init() {
    rxPool_.Recreate();
    txPool_.Recreate();
    (void)Device().InitPort(port_, &rxPool_, &txPool_);
  }

  /**
   * @brief 获取发送队列池剩余帧数。
   *
   * @return 剩余可写帧数。
   */
  uint32_t TxFree() const {
    return Device().TxFree(port_);
  }

  /**
   * @brief 获取发送队列池已用帧数。
   *
   * @return 已保存帧数。
   */
  uint32_t TxUsed() const {
    return Device().TxUsed(port_);
  }

  /**
   * @brief 获取累计丢弃的接收帧数。
   *
   * @return 累计丢弃帧数。
   */
  uint32_t RxDropped() const {
    return Device().RxDropped(port_);
  }

  /**
   * @brief 判断当前 CAN 端口是否可用。
   *
   * @return 已完成初始化且底层句柄有效时返回 `true`。
   */
  bool IsConnected() const {
    return Device().IsReady(port_);
  }

  /**
   * @brief 获取当前绑定的逻辑 CAN 端口。
   *
   * @return 逻辑 CAN 端口号。
   */
  CanPortId Port() const {
    return port_;
  }

  /**
   * @brief 直接发送一帧 CAN 报文。
   *
   * @param frame 待发送的 CAN 报文。
   * @return 发送成功返回 `true`。
   */
  bool WriteFrame(const CanFramePacket &frame) {
    return Device().WriteFrame(port_, frame);
  }

  /**
   * @brief 直接读取一帧 CAN 报文。
   *
   * @param frame 输出报文对象。
   * @return 读取成功返回 `true`。
   */
  bool ReadFrame(CanFramePacket *frame) {
    if (frame == nullptr) {
      return false;
    }

    return rxPool_.Pop(frame);
  }

  /**
   * @brief 获取接收对象池剩余帧数。
   *
   * @return 剩余可写帧数。
   */
  uint32_t RxFree() const {
    return rxPool_.FreeSize();
  }

  /**
   * @brief 获取接收对象池已用帧数。
   *
   * @return 已保存帧数。
   */
  uint32_t RxUsed() const {
    return rxPool_.UsedSize();
  }

private:
  /**
   * @brief 获取底层 CAN 服务单例。
   *
   * @return `CanService` 单例引用。
   */
  static CanService &Device() {
    return CanService::Instance();
  }

  CanPortId port_; /**< 当前对象绑定的逻辑 CAN 端口。 */
  StaticLockFreePool<CanFramePacket, kRxPoolFrameCount> rxPool_ {}; /**< 接收方向帧对象池。 */
  StaticLockFreePool<CanFramePacket, kTxPoolFrameCount> txPool_ {}; /**< 发送方向帧对象池。 */
};

using can_port = HardwareCan<>;

} // namespace iFly

#endif /* IFLY_HARDWARE_CAN_HPP */
