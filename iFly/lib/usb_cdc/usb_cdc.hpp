#ifndef IFLY_USB_CDC_HPP
#define IFLY_USB_CDC_HPP

#include <stddef.h>
#include <stdint.h>
#include "main.h"

namespace ifly {

/**
 * @brief 轻量级 USB CDC ACM 设备实现。
 *
 * @details
 * - 基于 STM32 HAL PCD 回调直接实现，不依赖 ST USB Device 中间件。
 * - 发送路径采用双缓冲静态队列。
 * - 接收路径采用“单包缓存 + 环形缓冲区”。
 * - 全部缓冲区静态分配，不使用动态内存。
 */
class UsbCdcAcm final {
public:
  /** @brief 获取单例对象。 */
  static UsbCdcAcm &Instance();

  /** @brief 初始化协议层并启动 USB 外设。 */
  void Init(PCD_HandleTypeDef *hpcd);
  /** @brief 向发送双缓冲写入待发数据。 */
  uint32_t Write(const uint8_t *data, uint32_t len);
  /** @brief 从接收环形缓冲区中取走数据。 */
  uint32_t Read(uint8_t *data, uint32_t len);
  /** @brief 查询接收环形缓冲区当前可读字节数。 */
  uint32_t Available() const;
  /** @brief 查询主机是否已完成 SET_CONFIGURATION。 */
  bool IsConfigured() const;

  /** @brief HAL_PCD_ResetCallback 对应入口。 */
  void OnReset(PCD_HandleTypeDef *hpcd);
  /** @brief HAL_PCD_SetupStageCallback 对应入口。 */
  void OnSetupStage(PCD_HandleTypeDef *hpcd);
  /** @brief HAL_PCD_DataInStageCallback 对应入口。 */
  void OnDataInStage(PCD_HandleTypeDef *hpcd, uint8_t epnum);
  /** @brief HAL_PCD_DataOutStageCallback 对应入口。 */
  void OnDataOutStage(PCD_HandleTypeDef *hpcd, uint8_t epnum);
  /** @brief HAL_PCD_SuspendCallback 对应入口。 */
  void OnSuspend(PCD_HandleTypeDef *hpcd);
  /** @brief HAL_PCD_ResumeCallback 对应入口。 */
  void OnResume(PCD_HandleTypeDef *hpcd);

private:
  UsbCdcAcm() = default;

  /**
   * @brief 标准 USB Setup 包结构。
   * @note HAL 会把 8 字节 setup 数据放入 hpcd->Setup。
   */
  struct SetupPacket {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
  };

  /** @brief CDC 线路编码参数，对应主机串口工具可设置的格式。 */
  struct LineCoding {
    uint32_t baudrate;
    uint8_t stopBits;
    uint8_t parityType;
    uint8_t dataBits;
  };

  /**
   * @brief 发送双缓冲中的单个槽位。
   * @note len 表示本槽位总字节数，sent 表示已经发出的字节数。
   */
  struct TxSlot {
    uint8_t data[256];
    uint16_t len;
    uint16_t sent;
    bool queued;
  };

  /** @brief EP0 OUT 当前所处的接收状态。 */
  enum class Ep0OutState : uint8_t {
    /** 等待新的控制 OUT 数据阶段。 */
    kIdle = 0U,
    /** 正在接收 SET_LINE_CODING 的 7 字节参数。 */
    kSetLineCoding = 1U
  };

  static constexpr uint8_t kEp0Mps = 64U;
  // 端点布局与 STM32 官方 CDC ACM 示例保持一致。
  static constexpr uint8_t kEpCdcDataIn = 0x81U;
  static constexpr uint8_t kEpCdcDataOut = 0x01U;
  static constexpr uint8_t kEpCdcCmdIn = 0x82U;
  static constexpr uint16_t kEpDataMps = 64U;
  static constexpr uint16_t kEpCmdMps = 8U;
  static constexpr uint32_t kRxRingSize = 1024U;

  /** @brief 将运行期状态恢复到“未配置”的初始值。 */
  void ResetRuntimeState();
  /** @brief 打开 EP0 IN/OUT 控制端点。 */
  void OpenControlEndpoints();
  /** @brief 打开 CDC 命令端点和数据端点。 */
  void OpenDataEndpoints();
  /** @brief 关闭 CDC 数据相关端点，并清空发送状态。 */
  void CloseDataEndpoints();
  /** @brief 重新挂起一次 BULK OUT 接收，等待主机发来下一包。 */
  void PrimeOutEndpoint();

  /** @brief 处理标准 USB 请求。 */
  void HandleStandardRequest(const SetupPacket &setup);
  /** @brief 处理 CDC 类请求。 */
  void HandleClassRequest(const SetupPacket &setup);
  /** @brief 根据请求类型返回设备/配置/字符串描述符。 */
  void HandleGetDescriptor(const SetupPacket &setup);

  /** @brief 启动一次 EP0 IN 控制传输。 */
  void StartControlInTransfer(const uint8_t *data, uint16_t len, uint16_t requestLen);
  /** @brief 在 EP0 IN 回调中继续后续分包发送。 */
  void ContinueControlInTransfer();
  /** @brief 发送 EP0 状态阶段零长度包。 */
  void SendControlStatus();
  /** @brief 让 EP0 进入 STALL，表示当前请求不支持。 */
  void StallControlEndpoint();

  /** @brief 把一包 OUT 数据搬运到接收环形缓冲区。 */
  void PushRxData(const uint8_t *data, uint32_t len);
  /** @brief 从接收环形缓冲区弹出数据给上层。 */
  uint32_t PopRxData(uint8_t *data, uint32_t len);

  /** @brief 若发送端空闲，则从双缓冲中启动下一次 BULK IN 发送。 */
  void TryStartTxTransfer();

private:
  PCD_HandleTypeDef *pcd_ = nullptr;
  volatile bool configured_ = false;
  volatile bool suspended_ = false;
  volatile uint8_t currentConfig_ = 0U;
  volatile uint8_t currentInterface_ = 0U;
  LineCoding lineCoding_ {115200U, 0U, 0U, 8U};
  volatile uint8_t controlLineState_ = 0U;

  // EP0 OUT 的接收状态和暂存缓冲区。
  Ep0OutState ep0OutState_ = Ep0OutState::kIdle;
  uint8_t ep0OutBuffer_[64] {};
  uint16_t ep0OutExpectedLen_ = 0U;

  // EP0 IN 的发送游标，用于控制传输分包。
  const uint8_t *ep0InPtr_ = nullptr;
  uint16_t ep0InRemaining_ = 0U;
  uint16_t ep0InRequestLen_ = 0U;
  uint8_t ep0ZlpDummy_ = 0U;

  // 发送端采用双缓冲，主循环与中断共享两个槽位。
  TxSlot txSlots_[2] {};
  volatile bool txBusy_ = false;
  int8_t txActiveSlot_ = -1;
  uint16_t txLastPacketLen_ = 0U;

  // 接收端先落到单包缓存，再搬运到环形缓冲区。
  uint8_t rxPacketBuffer_[kEpDataMps] {};
  uint8_t rxRing_[kRxRingSize] {};
  volatile uint32_t rxHead_ = 0U;
  volatile uint32_t rxTail_ = 0U;
  volatile uint32_t rxDropped_ = 0U;
};

} // namespace ifly

#endif /* IFLY_USB_CDC_HPP */
