#include "hardware_uart.hpp"

namespace iFly {

/* 构造时只记录端口号与 RX 队列配置，不在这里触碰硬件。 */
HardwareUart::HardwareUart(UartPortId port, uint32_t rxQueueStorageSize)
    : SerialIoBase(rxQueueStorageSize), port_(port) {
}

/*
 * 初始化流程：
 * 1. 先确保统一 RX 队列已经创建成功；
 * 2. 再把这条队列交给 `UartDmaService`；
 * 3. 由底层服务完成 DMA RX/TX 缓冲创建与 HAL DMA 接收启动。
 */
void HardwareUart::Init() {
  if (!EnsureRxQueueCreated()) {
    return;
  }

  (void)Device().InitPort(port_, RxQueue());
}

/* 上层写入的数据不会直接立刻发硬件，而是先进入底层 TX 无锁队列。 */
uint32_t HardwareUart::Write(const uint8_t *data, uint32_t len) {
  return Device().Write(port_, data, len);
}

/* 查询当前端口发送方向剩余空间。 */
uint32_t HardwareUart::TxFree() const {
  return Device().TxFree(port_);
}

/* 查询当前端口发送方向已用空间。 */
uint32_t HardwareUart::TxUsed() const {
  return Device().TxUsed(port_);
}

/* 查询当前端口从 DMA RX 缓冲上抛到用户队列过程中的累计丢字节数。 */
uint32_t HardwareUart::RxDropped() const {
  return Device().RxDropped(port_);
}

/* 当前端口是否已经具备可工作的 DMA RX/TX 资源。 */
bool HardwareUart::IsConnected() const {
  return Device().IsReady(port_);
}

/* 只是返回逻辑端口号，方便上层调试或日志打印。 */
UartPortId HardwareUart::Port() const {
  return port_;
}

/* 统一收口到底层单例，避免每个成员函数都重复写 `UartDmaService::Instance()`。 */
UartDmaService &HardwareUart::Device() {
  return UartDmaService::Instance();
}

} // namespace iFly
