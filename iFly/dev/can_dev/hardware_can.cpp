// 硬件 CAN 设备封装实现。
// 负责端口初始化以及与底层 CanService 的对接。
#include "hardware_can.hpp"

namespace iFly {

// 构造时只记录端口号，并把接收队列容量交给 SerialIoBase 基类管理。
HardwareCan::HardwareCan(CanPortId port, uint32_t rxQueueStorageSize)
    : SerialIoBase(rxQueueStorageSize), port_(port) {
}

// 初始化一路 CAN。
//
// 这里要先确保 SerialIoBase 内部的 RX 队列已经创建成功，
// 然后再把这个队列注册给底层 CanService。
void HardwareCan::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  (void)Device().InitPort(port_, RxQueue());
}

// 兼容统一串口风格的写接口。
// 实际仍然是把 CanFramePacket 放到底层发送队列中。
uint32_t HardwareCan::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(port_, data, len);
}

// 查询底层发送队列状态。
uint32_t HardwareCan::TxFree() const {
  return Device().TxFree(port_);
}

// 返回发送缓冲已用空间。
uint32_t HardwareCan::TxUsed() const {
  return Device().TxUsed(port_);
}

// 返回接收链路累计丢弃的数据量。
uint32_t HardwareCan::RxDropped() const {
  return Device().RxDropped(port_);
}

// 只有底层 CAN 口已经就绪，才认为当前设备“已连接”。
bool HardwareCan::IsConnected() const {
  return Device().IsReady(port_);
}

// 返回当前绑定的逻辑端口。
CanPortId HardwareCan::Port() const {
  return port_;
}

// 更符合 CAN 直觉的发帧接口。
bool HardwareCan::WriteFrame(const CanFramePacket &frame) {
  return Device().WriteFrame(port_, frame);
}

// 从上层 RX 队列中读取一整帧 CAN 报文。
bool HardwareCan::ReadFrame(CanFramePacket *frame) {
  if (frame == nullptr) {
    return false;
  }

  BeforeRead();
  if (SerialIoBase::Available() < sizeof(CanFramePacket)) {
    return false;
  }

  return Dequeue(reinterpret_cast<uint8_t *>(frame), sizeof(CanFramePacket)) == sizeof(CanFramePacket);
}

// 返回当前上层统一接收队列里可读的字节数。
// 由于队列中存的是完整 CanFramePacket，所以通常应按整帧理解。
uint32_t HardwareCan::Available() {
  BeforeRead();
  return SerialIoBase::Available();
}

// 查询 RX 队列剩余空间。
uint32_t HardwareCan::RxFree() {
  BeforeRead();
  return SerialIoBase::RxFree();
}

// 查询 RX 队列已使用空间。
uint32_t HardwareCan::RxUsed() {
  BeforeRead();
  return SerialIoBase::RxUsed();
}

// 返回底层 CAN 服务单例。
CanService &HardwareCan::Device() {
  return CanService::Instance();
}

// 仅为了与其他 `SerialIoBase` 设备保持接口一致。
void HardwareCan::BeforeRead() {
  (void)port_;
}

} // namespace iFly
