#ifndef IFLY_HARDWARE_CAN_HPP
#define IFLY_HARDWARE_CAN_HPP

#include <stdint.h>

#include "can.hpp"
#include "serial_io_base.hpp"

namespace iFly {

// 面向业务层的一路 CAN 设备封装。
//
// 这个类的目标不是把 HAL 完整暴露出来，而是把 CAN 也包装成和
// UART / USB CDC 类似的统一 IO 接口，便于上层代码复用 SerialIoBase。
//
// 但要注意：
// 1. UART/USB 本质上是字节流
// 2. CAN 本质上是“帧”
//
// 因此这里同时提供两类接口：
// 1. Write()/Read 体系：为了兼容 SerialIoBase 的统一风格
//    实际上传输的是固定大小的 CanFramePacket
// 2. WriteFrame()/ReadFrame()：更符合 CAN 使用习惯，推荐优先使用
class HardwareCan final : public SerialIoBase {
public:
  static constexpr uint32_t kDefaultCanRxQueueStorageSize = 2048U;

  // rxQueueStorageSize 是“上层统一接收队列”的容量，单位是字节。
  // 因为队列里存的是 CanFramePacket，所以建议按 16 字节整数倍配置。
  explicit HardwareCan(CanPortId port,
                       uint32_t rxQueueStorageSize = kDefaultCanRxQueueStorageSize) noexcept;

  // 初始化本路 CAN，并把本对象持有的 RX 队列注册给 CanService。
  void Init() override;

  // 兼容 SerialIoBase 的写接口。
  //
  // 注意这里不是随便写任意字节流，而是要求 data 里按顺序存放
  // 一个或多个 CanFramePacket。len 不是整帧整数倍的尾部字节会被忽略。
  uint32_t Write(const uint8_t *data, uint32_t len) override;

  // 发送队列剩余空间 / 已用空间，单位都是字节。
  // 如果换算成帧数，需要再除以 sizeof(CanFramePacket)。
  uint32_t TxFree() const override;
  uint32_t TxUsed() const override;

  // Returns dropped RX bytes when the upper queue has no space.
  uint32_t RxDropped() const override;

  // 当前端口是否已经初始化成功且底层句柄有效。
  bool IsConnected() const override;

  // 查询当前对象绑定的是哪一路逻辑 CAN 端口。
  CanPortId Port() const noexcept;

  // 直接写一帧 CAN，业务层推荐使用这个接口。
  bool WriteFrame(const CanFramePacket &frame);

  // 直接读一帧 CAN。
  // 成功返回 true，并把完整帧写入 *frame。
  bool ReadFrame(CanFramePacket *frame);

  // These helpers keep the shared read hook for API compatibility.
  uint32_t Available();
  uint32_t RxFree();
  uint32_t RxUsed();

private:
  // 统一拿到底层 CAN 单例服务。
  static CanService &Device() noexcept;

  // Kept only for interface compatibility. RX already lands in the upper queue.
  void BeforeRead() override;

private:
  // 当前对象绑定的逻辑 CAN 口，例如 kCan1 / kCan2。
  CanPortId port_;
};

using can_port = HardwareCan;

} // namespace iFly

#endif /* IFLY_HARDWARE_CAN_HPP */
