/**
 * @file usb_cdc.hpp
 * @brief USB CDC ACM 协议层接口。
 */
#ifndef IFLY_USB_CDC_HPP
#define IFLY_USB_CDC_HPP

#include <stddef.h>
#include <stdint.h>

#include "double_buffer.hpp"
#include "lock_free_queue.hpp"

namespace iFly {

static constexpr uint16_t kDefaultUsbEndpointPacketSize = 64U; /**< 默认 USB CDC 数据端点包长。 */
static constexpr uint16_t kDefaultUsbEndpointBufferSize = 200U; /**< 默认 USB CDC 端点双缓冲区大小。 */
static constexpr uint32_t kDefaultUsbTxQueueStorageSize = 200U; /**< 默认 USB CDC 发送队列容量。 */

/**
 * @brief USB 数据端点使用的双缓冲区。
 */
class UsbEndpointDoubleBufferBase {
public:
  UsbEndpointDoubleBufferBase() = default;

  UsbEndpointDoubleBufferBase(const UsbEndpointDoubleBufferBase &) = delete;
  UsbEndpointDoubleBufferBase &operator=(const UsbEndpointDoubleBufferBase &) = delete;

  /**
   * @brief 使用外部存储区创建双缓冲区。
   *
   * @param buffer0 第 0 个缓冲槽首地址。
   * @param buffer1 第 1 个缓冲槽首地址。
   * @param bufferSize 单个缓冲槽大小。
   * @param packetSize 单个端点包的大小。
   * @return 创建成功返回 `true`。
   */
  bool Create(uint8_t *buffer0, uint8_t *buffer1, uint16_t bufferSize, uint16_t packetSize);

  /**
   * @brief 解除双缓冲区与外部存储区的绑定。
   */
  void Delete();

  /**
   * @brief 清空缓冲区状态。
   */
  void Clear();

  /**
   * @brief 判断双缓冲区是否已经创建成功。
   *
   * @return 已创建返回 `true`。
   */
  bool IsCreated() const;

  /**
   * @brief 获取当前有效包长。
   *
   * @return 当前包长。
   */
  uint16_t PacketSize() const;

  /**
   * @brief 获取活动缓冲槽的可写指针。
   *
   * @return 活动缓冲区首地址。
   */
  uint8_t *ActiveBuffer();

  /**
   * @brief 设置当前活动缓冲槽的长度。
   *
   * @param length 新的活动长度。
   */
  void SetActiveLength(uint16_t length);

  /**
   * @brief 清空当前活动缓冲槽的长度。
   */
  void ClearActive();

  /**
   * @brief 获取备用缓冲槽的可写指针。
   *
   * @return 备用缓冲区首地址。
   */
  uint8_t *InactiveBuffer();

  /**
   * @brief 获取当前备用缓冲槽的长度。
   *
   * @return 当前备用长度。
   */
  uint16_t InactiveLength() const;

  /**
   * @brief 设置当前备用缓冲槽的长度。
   *
   * @param length 新的备用长度。
   */
  void SetInactiveLength(uint16_t length);

  /**
   * @brief 判断当前备用缓冲槽是否存在有效数据。
   *
   * @return 存在有效数据返回 `true`。
   */
  bool HasInactiveData() const;

  /**
   * @brief 交换活动缓冲槽与备用缓冲槽。
   */
  void SwapBuffers();

private:
  /**
   * @brief 重置两个缓冲槽的长度状态。
   */
  void ResetLengths();

  /**
   * @brief 获取当前备用缓冲槽索引。
   *
   * @return 备用缓冲槽索引。
   */
  uint8_t InactiveSlotIndex() const;

  uint8_t *buffers_[2] {}; /**< 两个外部缓冲槽首地址。 */
  uint16_t bufferSize_ = 0U; /**< 单个缓冲槽大小。 */
  uint16_t packetSize_ = 0U; /**< 当前有效包长。 */
  uint16_t lengths_[2] {}; /**< 两个缓冲槽各自的长度信息。 */
  uint8_t activeSlot_ = 0U; /**< 当前活动缓冲槽索引。 */
};

/**
 * @brief 带静态存储区的 USB 数据端点双缓冲区。
 *
 * @tparam kBufferSize 单个缓冲槽的固定容量。
 */
template <uint16_t kBufferSize = kDefaultUsbEndpointBufferSize>
class UsbEndpointDoubleBuffer final : public UsbEndpointDoubleBufferBase {
public:
  static_assert(kBufferSize >= kDefaultUsbEndpointPacketSize,
                "kBufferSize must hold at least one endpoint packet.");

  /**
   * @brief 构造时自动绑定内部静态缓冲区。
   */
  UsbEndpointDoubleBuffer() {
    Recreate();
  }

  /**
   * @brief 使用指定包长重新绑定内部静态缓冲区。
   *
   * @param packetSize 单个端点包的大小。
   * @return 绑定成功返回 `true`。
   */
  bool Recreate(uint16_t packetSize = kDefaultUsbEndpointPacketSize) {
    return Create(storage_[0], storage_[1], kBufferSize, packetSize);
  }

private:
  uint8_t storage_[2][kBufferSize] {}; /**< 两个固定大小的端点缓冲槽。 */
};

/**
 * @brief 轻量级 USB CDC ACM 设备协议层。
 */
class UsbCdcAcm final {
public:
  /**
   * @brief 获取 USB CDC ACM 单例。
   *
   * @return 单例引用。
   */
  static UsbCdcAcm &Instance();

  /**
   * @brief 初始化 USB CDC ACM 协议层。
   */
  void Init();

  /**
   * @brief 绑定上层接收队列。
   *
   * @param queue 上层统一接收队列。
   */
  void AttachRxQueue(LockFreeQueueBase *queue);

  /**
   * @brief 绑定底层发送队列和端点双缓冲区。
   *
   * @param txQueue 发送方向字节队列。
   * @param rxEndpointBuffer OUT 端点双缓冲区。
   * @param txEndpointBuffer IN 端点双缓冲区。
   */
  void AttachStorage(LockFreeQueueBase *txQueue,
                     UsbEndpointDoubleBufferBase *rxEndpointBuffer,
                     UsbEndpointDoubleBufferBase *txEndpointBuffer);

  /**
   * @brief 驱动后台服务逻辑。
   */
  void Service();

  /**
   * @brief 写入待发送数据。
   *
   * @param data 待发送数据首地址。
   * @param len 待发送数据长度，单位为字节。
   * @return 实际接受写入的字节数。
   */
  uint32_t Write(const uint8_t *data, uint32_t len);

  /**
   * @brief 从上层接收队列中读取数据。
   *
   * @param data 输出缓冲区首地址。
   * @param len 期望读取长度，单位为字节。
   * @return 实际读取的字节数。
   */
  uint32_t Read(uint8_t *data, uint32_t len);

  /**
   * @brief 获取当前可读字节数。
   *
   * @return 当前可读字节数。
   */
  uint32_t Available() const;

  /**
   * @brief 获取发送队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t TxUsed() const;

  /**
   * @brief 获取发送队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t TxFree() const;

  /**
   * @brief 获取接收队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t RxUsed() const;

  /**
   * @brief 获取接收队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t RxFree() const;

  /**
   * @brief 获取接收链路累计丢字节数。
   *
   * @return 累计丢失字节数。
   */
  uint32_t RxDropped() const;

  /**
   * @brief 判断 USB 设备是否已完成配置。
   *
   * @return 已完成配置返回 `true`。
   */
  bool IsConfigured() const;

  /**
   * @brief 处理 USB 复位事件。
   */
  void OnReset();

  /**
   * @brief 处理 USB Setup 阶段事件。
   */
  void OnSetupStage();

  /**
   * @brief 处理 USB IN 方向数据阶段事件。
   *
   * @param epnum 端点编号。
   */
  void OnDataInStage(uint8_t epnum);

  /**
   * @brief 处理 USB OUT 方向数据阶段事件。
   *
   * @param epnum 端点编号。
   */
  void OnDataOutStage(uint8_t epnum);

  /**
   * @brief 处理 USB 挂起事件。
   */
  void OnSuspend();

  /**
   * @brief 处理 USB 恢复事件。
   */
  void OnResume();

private:
  /**
   * @brief 构造 USB CDC ACM 协议层对象。
   */
  UsbCdcAcm();

  /**
   * @brief USB Setup 包结构。
   */
  struct SetupPacket final {
    uint8_t bmRequestType = 0U; /**< 控制请求类型。 */
    uint8_t bRequest = 0U; /**< 控制请求编号。 */
    uint16_t wValue = 0U; /**< 控制请求参数值。 */
    uint16_t wIndex = 0U; /**< 控制请求索引值。 */
    uint16_t wLength = 0U; /**< 控制请求数据长度。 */
  };

  /**
   * @brief 串口线编码配置。
   */
  struct LineCoding final {
    uint32_t baudrate = 0U; /**< 波特率。 */
    uint8_t stopBits = 0U; /**< 停止位配置。 */
    uint8_t parityType = 0U; /**< 校验位配置。 */
    uint8_t dataBits = 0U; /**< 数据位宽。 */
  };

  /**
   * @brief 端点 0 OUT 方向状态。
   */
  enum class Ep0OutState : uint8_t {
    kIdle = 0U, /**< 空闲状态。 */
    kSetLineCoding = 1U /**< 正在接收设置串口格式请求。 */
  };

  static constexpr uint8_t kEp0Mps = 64U; /**< 控制端点最大包长。 */
  static constexpr uint8_t kEpCdcDataIn = 0x81U; /**< CDC 数据 IN 端点地址。 */
  static constexpr uint8_t kEpCdcDataOut = 0x01U; /**< CDC 数据 OUT 端点地址。 */
  static constexpr uint8_t kEpCdcCmdIn = 0x82U; /**< CDC 命令 IN 端点地址。 */
  static constexpr uint16_t kEpDataMps = kDefaultUsbEndpointPacketSize; /**< 数据端点最大包长。 */
  static constexpr uint16_t kEpCmdMps = 8U; /**< 命令端点最大包长。 */

  /**
   * @brief 重置运行时状态。
   */
  void ResetRuntimeState();

  /**
   * @brief 打开控制端点。
   */
  void OpenControlEndpoints();

  /**
   * @brief 打开数据端点。
   */
  void OpenDataEndpoints();

  /**
   * @brief 关闭数据端点。
   */
  void CloseDataEndpoints();

  /**
   * @brief 预挂起下一次 OUT 端点接收。
   */
  void PrimeOutEndpoint();

  /**
   * @brief 处理标准控制请求。
   *
   * @param setup 当前 Setup 包。
   */
  void HandleStandardRequest(const SetupPacket &setup);

  /**
   * @brief 处理类控制请求。
   *
   * @param setup 当前 Setup 包。
   */
  void HandleClassRequest(const SetupPacket &setup);

  /**
   * @brief 处理描述符读取请求。
   *
   * @param setup 当前 Setup 包。
   */
  void HandleGetDescriptor(const SetupPacket &setup);

  /**
   * @brief 启动控制端点 IN 方向传输。
   *
   * @param data 待发送数据首地址。
   * @param len 实际数据长度。
   * @param requestLen 请求方声明的长度。
   */
  void StartControlInTransfer(const uint8_t *data, uint16_t len, uint16_t requestLen);

  /**
   * @brief 继续分片发送控制端点 IN 数据。
   */
  void ContinueControlInTransfer();

  /**
   * @brief 发送控制传输状态阶段。
   */
  void SendControlStatus();

  /**
   * @brief 将控制端点置为 STALL 状态。
   */
  void StallControlEndpoint();

  /**
   * @brief 将接收到的数据推入上层队列。
   *
   * @param data 接收数据首地址。
   * @param len 接收数据长度。
   */
  void PushReceivedPacket(const uint8_t *data, uint32_t len);

  /**
   * @brief 驱动发送路径继续工作。
   */
  void ServiceTxPath();

  /**
   * @brief 从发送队列装载一个包到备用缓冲区。
   *
   * @return 本次装载的字节数。
   */
  uint32_t LoadTxPacketToInactiveBuffer();

  /**
   * @brief 获取上层接收队列已用空间。
   *
   * @return 已使用字节数。
   */
  uint32_t UpperRxUsed() const;

  /**
   * @brief 获取上层接收队列剩余空间。
   *
   * @return 剩余可写字节数。
   */
  uint32_t UpperRxFree() const;

  LockFreeQueueBase *appRxQueue_ = nullptr; /**< 上层接收队列。 */

  std::atomic<bool> initialized_ {false}; /**< 协议层是否已初始化。 */
  std::atomic<bool> configured_ {false}; /**< 是否已被主机配置。 */
  std::atomic<bool> suspended_ {false}; /**< 当前是否处于挂起状态。 */
  volatile uint8_t currentConfig_ = 0U; /**< 当前 USB 配置值。 */
  volatile uint8_t currentInterface_ = 0U; /**< 当前接口编号。 */
  LineCoding lineCoding_ {115200U, 0U, 0U, 8U}; /**< 当前串口线编码。 */
  volatile uint8_t controlLineState_ = 0U; /**< 当前控制线状态。 */

  Ep0OutState ep0OutState_ = Ep0OutState::kIdle; /**< 控制端点 OUT 状态。 */
  uint8_t ep0OutBuffer_[kEp0Mps] {}; /**< 控制端点 OUT 缓冲区。 */
  uint16_t ep0OutExpectedLen_ = 0U; /**< 控制端点 OUT 期望接收长度。 */

  const uint8_t *ep0InPtr_ = nullptr; /**< 控制端点 IN 当前发送指针。 */
  uint16_t ep0InRemaining_ = 0U; /**< 控制端点 IN 剩余发送长度。 */
  uint16_t ep0InRequestLen_ = 0U; /**< 控制请求声明的数据长度。 */
  uint8_t ep0ZlpDummy_ = 0U; /**< 零长度包占位字节。 */
  uint8_t lineCodingBuffer_[7] {}; /**< LineCoding 临时编码缓冲区。 */

  UsbEndpointDoubleBufferBase *rxEndpointBuffer_ = nullptr; /**< OUT 端点双缓冲区。 */
  LockFreeQueueBase *txQueue_ = nullptr; /**< 发送队列。 */
  UsbEndpointDoubleBufferBase *txEndpointBuffer_ = nullptr; /**< IN 端点双缓冲区。 */

  std::atomic<bool> txBusy_ {false}; /**< 当前是否正在发送数据。 */
  std::atomic<uint32_t> rxDropped_ {0U}; /**< 接收链路累计丢字节数。 */
  std::atomic<uint32_t> txServiceRequests_ {0U}; /**< 待处理发送服务请求计数。 */
  std::atomic<bool> txServiceRunning_ {false}; /**< 发送服务是否正在运行。 */
};

} // namespace iFly

#endif /* IFLY_USB_CDC_HPP */
