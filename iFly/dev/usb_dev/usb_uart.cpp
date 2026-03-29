#include "usb_uart.hpp"

namespace iFly {

/* 构造时只记录 RX 队列配置，不在这里做底层 USB 初始化。 */
UsbUart::UsbUart(uint32_t rxQueueStorageSize) noexcept : SerialIoBase(rxQueueStorageSize) {
}

/*
 * 初始化流程：
 * 1. 先确保统一 RX 队列已经创建成功；
 * 2. 再把该队列注册给 USB CDC 底层；
 * 3. 最后启动 CDC 协议层。
 */
void UsbUart::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  Device().AttachRxQueue(RxQueue());
  Device().Init();
}

/* 上层写入的数据交给 USB CDC 发送路径处理。 */
uint32_t UsbUart::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(data, len);
}

/* 查询 CDC 底层发送队列剩余空间。 */
uint32_t UsbUart::TxFree() const {
  return Device().TxFree();
}

/* 查询 CDC 底层发送队列已用空间。 */
uint32_t UsbUart::TxUsed() const {
  return Device().TxUsed();
}

/* 查询 CDC 接收链路累计丢弃的字节数。 */
uint32_t UsbUart::RxDropped() const {
  return Device().RxDropped();
}

/* 只有在主机完成配置后，CDC 才算真正“连接可用”。 */
bool UsbUart::IsConnected() const {
  return Device().IsConfigured();
}

/* 统一封装底层单例获取逻辑。 */
UsbCdcAcm &UsbUart::Device() noexcept {
  return UsbCdcAcm::Instance();
}

/*
 * `SerialIoBase::Read()` 在真正出队前会先调用这里。
 *
 * USB CDC 的接收链路里还存在一层内部暂存，因此每次读取前
 * 先执行一次 `Service()`，让底层尽量把数据继续推到统一 RX 队列。
 */
void UsbUart::BeforeRead() {
  Device().Service();
}

} // namespace iFly
