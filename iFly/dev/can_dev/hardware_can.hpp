/**
 * @file hardware_can.hpp
 * @brief 硬件 CAN 设备封装接口。
 */
#ifndef IFLY_HARDWARE_CAN_HPP
#define IFLY_HARDWARE_CAN_HPP

#include <stdint.h>

#include "can.hpp"
#include "serial_io_base.hpp"

namespace iFly {

static constexpr uint32_t kDefaultCanRxQueueStorageSize = 2048U; /**< 默认 CAN 接收队列容量。 */

/**
 * @brief 面向业务层的单路 CAN 设备封装。
 *
 * @details
 * 该类把 CAN 端口封装成与其他串行设备一致的统一接口，同时保留
 * 面向 CAN 帧的直接读写能力。
 *
 * @tparam kTxQueueStorageSize 发送队列总容量。
 * @tparam kRxQueueStorageSize 接收队列总容量。
 */
template <uint32_t kTxQueueStorageSize = CanService::kDefaultTxQueueStorageSize,
          uint32_t kRxQueueStorageSize = kDefaultCanRxQueueStorageSize>
class HardwareCan final : public SerialIoBase {
public:
  static_assert(kTxQueueStorageSize >= (CanService::kCanFramePacketSize + 1U),
                "kTxQueueStorageSize must hold at least one CAN frame.");
  static_assert(kRxQueueStorageSize >= (CanService::kCanFramePacketSize + 1U),
                "kRxQueueStorageSize must hold at least one CAN frame.");

  static constexpr uint32_t kDefaultRxQueueStorageSize =
      iFly::kDefaultCanRxQueueStorageSize; /**< 默认 CAN 接收队列容量。 */

  /**
   * @brief 构造一个逻辑 CAN 设备对象。
   *
   * @param port 逻辑 CAN 端口号。
   * @param rxQueueStorageSize 接收队列总容量，默认使用模板参数。
   */
  explicit HardwareCan(CanPortId port,
                       uint32_t rxQueueStorageSize = kRxQueueStorageSize)
      : SerialIoBase(rxQueueStorageSize), port_(port) {
  }

  /**
   * @brief 初始化当前 CAN 端口。
   */
  void Init() override {
    if (!EnsureRxQueueCreated()) {
      return;
    }

    txQueue_.Recreate();
    txBuffers_.Recreate();
    (void)Device().InitPort(port_, RxQueue(), &txQueue_, &txBuffers_);
  }

  /**
   * @brief 以兼容字节流的方式写入待发送数据。
   *
   * @param data 待发送数据首地址，内容需按 `CanFramePacket` 顺序排列。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际写入的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len) override {
    return Device().Write(port_, data, len);
  }

  /**
   * @brief 获取发送队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t TxFree() const override {
    return Device().TxFree(port_);
  }

  /**
   * @brief 获取发送队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t TxUsed() const override {
    return Device().TxUsed(port_);
  }

  /**
   * @brief 获取累计丢弃的接收字节数。
   *
   * @return 累计丢弃字节数。
   */
  uint32_t RxDropped() const override {
    return Device().RxDropped(port_);
  }

  /**
   * @brief 判断当前 CAN 端口是否可用。
   *
   * @return 已完成初始化且底层句柄有效时返回 `true`。
   */
  bool IsConnected() const override {
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

    BeforeRead();
    if (SerialIoBase::Available() < sizeof(CanFramePacket)) {
      return false;
    }

    return Dequeue(reinterpret_cast<uint8_t *>(frame), sizeof(CanFramePacket)) == sizeof(CanFramePacket);
  }

  /**
   * @brief 获取当前可读字节数。
   *
   * @return 可读字节数。
   */
  uint32_t Available() {
    BeforeRead();
    return SerialIoBase::Available();
  }

  /**
   * @brief 获取接收队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t RxFree() {
    BeforeRead();
    return SerialIoBase::RxFree();
  }

  /**
   * @brief 获取接收队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t RxUsed() {
    BeforeRead();
    return SerialIoBase::RxUsed();
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

  /**
   * @brief 为统一接口保留的读前钩子。
   */
  void BeforeRead() override {
    (void)port_;
  }

  CanPortId port_; /**< 当前对象绑定的逻辑 CAN 端口。 */
  StaticLockFreeQueue<kTxQueueStorageSize> txQueue_ {}; /**< 发送方向字节队列。 */
  CanTxDoubleBuffer txBuffers_ {}; /**< 发送方向单帧双缓冲区。 */
};

using can_port = HardwareCan<>;

} // namespace iFly

#endif /* IFLY_HARDWARE_CAN_HPP */
