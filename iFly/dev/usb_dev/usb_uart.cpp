#include "usb_uart.hpp"

namespace iFly {

UsbUart::UsbUart(uint32_t rxQueueStorageSize) noexcept
  : DynamicLockFreeQueue(rxQueueStorageSize) {
}

/*
 * 初始化顺序说明：
 * 1. 先确保当前 usb_uart 对象自带的接收队列已经创建成功；
 * 2. 再把该队列注册给 UsbCdcAcm，作为“usb_cdc -> 上层应用”的无锁队列；
 * 3. 最后启动 USB CDC 协议层。
 *
 * 这样应用层完全不需要了解 PCD、端点号和 HAL 回调，只需把 usb_uart 当普通串口使用。
 */
void UsbUart::Init() {
  if (!IsCreated()) {
    (void)Recreate(kDefaultRxQueueStorageSize);
  }

  Device().AttachRxQueue(this);
  Device().Init();
}

/*
 * 发送方向保持串口式封装：
 * - 上层只管写入字节流；
 * - 底层会先进入 usb_cdc 的发送队列；
 * - 再由 USB PCD 双缓冲发送状态机自动逐包发出。
 */
uint32_t UsbUart::Write(const uint8_t *data, uint32_t len) const {
  return Device().Write(data, len);
}

/*
 * 接收方向直接从当前对象继承而来的无锁队列中取数。
 * 为了让“传输层接收环形缓冲区”中的数据尽快上抛到应用队列，
 * 这里先让 UsbCdcAcm 做一次链路服务，再执行真正的出队读取。
 */
uint32_t UsbUart::Read(uint8_t *data, uint32_t len) {
  Device().Service();
  return Dequeue(data, len);
}

/* 返回当前应用层接收队列中的可读字节数。 */
uint32_t UsbUart::Available() const {
  return UsedSize();
}

/* 返回 USB CDC 内部发送队列剩余空间。 */
uint32_t UsbUart::TxFree() const {
  return Device().TxFree();
}

/* 返回 USB CDC 内部发送队列已用空间。 */
uint32_t UsbUart::TxUsed() const {
  return Device().TxUsed();
}

/* 返回当前对象接收队列剩余空间。 */
uint32_t UsbUart::RxFree() const {
  return FreeSize();
}

/* 返回当前对象接收队列已用空间。 */
uint32_t UsbUart::RxUsed() const {
  return UsedSize();
}

/* 返回底层接收链路累计丢包字节数。 */
uint32_t UsbUart::RxDropped() const {
  return Device().RxDropped();
}

/* 查询主机是否已经完成 USB 枚举配置。 */
bool UsbUart::IsConnected() const {
  return Device().IsConfigured();
}

/* 统一封装底层单例获取逻辑。 */
UsbCdcAcm &UsbUart::Device() {
  return UsbCdcAcm::Instance();
}

} // namespace iFly
