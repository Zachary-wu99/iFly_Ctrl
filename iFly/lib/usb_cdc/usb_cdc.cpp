#include "usb_cdc.hpp"
#include "usb_cdc.h"

#include <string.h>

namespace {

// USB 标准请求类型
constexpr uint8_t kReqTypeMask = 0x60U;
constexpr uint8_t kReqTypeStandard = 0x00U;
constexpr uint8_t kReqTypeClass = 0x20U;

constexpr uint8_t kRecipientMask = 0x1FU;
constexpr uint8_t kRecipientDevice = 0x00U;
constexpr uint8_t kRecipientInterface = 0x01U;
constexpr uint8_t kRecipientEndpoint = 0x02U;

// USB 标准请求
constexpr uint8_t kReqGetStatus = 0x00U;
constexpr uint8_t kReqClearFeature = 0x01U;
constexpr uint8_t kReqSetFeature = 0x03U;
constexpr uint8_t kReqSetAddress = 0x05U;
constexpr uint8_t kReqGetDescriptor = 0x06U;
constexpr uint8_t kReqGetConfiguration = 0x08U;
constexpr uint8_t kReqSetConfiguration = 0x09U;
constexpr uint8_t kReqGetInterface = 0x0AU;
constexpr uint8_t kReqSetInterface = 0x0BU;

// CDC 类请求
constexpr uint8_t kCdcReqSetLineCoding = 0x20U;
constexpr uint8_t kCdcReqGetLineCoding = 0x21U;
constexpr uint8_t kCdcReqSetControlLineState = 0x22U;

// 描述符类型
constexpr uint8_t kDescTypeDevice = 0x01U;
constexpr uint8_t kDescTypeConfiguration = 0x02U;
constexpr uint8_t kDescTypeString = 0x03U;

constexpr uint8_t kFeatureEndpointHalt = 0x00U;

constexpr uint8_t kEndpoint0Out = 0x00U;
constexpr uint8_t kEndpoint0In = 0x80U;

constexpr uint16_t kCdcConfigValue = 0x0001U;

constexpr uint16_t MinU16(uint16_t a, uint16_t b) {
  return (a < b) ? a : b;
}

constexpr uint32_t MinU32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

// 使用 RAII 保护临界区，避免中断与主循环并发访问时破坏状态。
class IrqGuard final {
public:
  IrqGuard() : primask_(__get_PRIMASK()) {
    __disable_irq();
  }

  ~IrqGuard() {
    __set_PRIMASK(primask_);
  }

private:
  uint32_t primask_;
};

// 设备描述符：使用标准单 CDC ACM 设备类声明。
const uint8_t kDeviceDescriptor[] = {
  0x12U,                    // 描述符长度
  0x01U,                    // 描述符类型（设备）
  0x00U, 0x02U,             // USB 版本 2.00
  0x02U,                    // 设备类（CDC）
  0x02U,                    // 设备子类（抽象控制模型）
  0x00U,                    // 设备协议
  0x40U,                    // 端点 0 最大包长
  0x83U, 0x04U,             // 厂商 ID（ST）
  0x40U, 0x57U,             // 产品 ID（虚拟串口）
  0x00U, 0x02U,             // 设备版本
  0x01U,                    // 厂商字符串索引
  0x02U,                    // 产品字符串索引
  0x03U,                    // 序列号字符串索引
  0x01U                     // 配置数量
};

// 配置描述符：CDC ACM，2 个接口，3 个端点。
const uint8_t kConfigurationDescriptor[] = {
  0x09U, 0x02U, 0x43U, 0x00U, 0x02U, 0x01U, 0x00U, 0x80U, 0x32U,
  0x09U, 0x04U, 0x00U, 0x00U, 0x01U, 0x02U, 0x02U, 0x01U, 0x00U,
  0x05U, 0x24U, 0x00U, 0x10U, 0x01U,
  0x05U, 0x24U, 0x01U, 0x00U, 0x01U,
  0x04U, 0x24U, 0x02U, 0x02U,
  0x05U, 0x24U, 0x06U, 0x00U, 0x01U,
  0x07U, 0x05U, 0x82U, 0x03U, 0x08U, 0x00U, 0x10U,
  0x09U, 0x04U, 0x01U, 0x00U, 0x02U, 0x0AU, 0x00U, 0x00U, 0x00U,
  0x07U, 0x05U, 0x01U, 0x02U, 0x40U, 0x00U, 0x00U,
  0x07U, 0x05U, 0x81U, 0x02U, 0x40U, 0x00U, 0x00U
};

const uint8_t kLangIdStringDescriptor[] = {
  0x04U, 0x03U, 0x09U, 0x04U
};

const uint8_t kManufacturerStringDescriptor[] = {
  0x0AU, 0x03U, 'i', 0x00U, 'F', 0x00U, 'l', 0x00U, 'y', 0x00U
};

const uint8_t kProductStringDescriptor[] = {
  0x22U, 0x03U,
  'i', 0x00U, 'F', 0x00U, 'l', 0x00U, 'y', 0x00U, ' ', 0x00U,
  'U', 0x00U, 'S', 0x00U, 'B', 0x00U, ' ', 0x00U,
  'C', 0x00U, 'D', 0x00U, 'C', 0x00U, ' ', 0x00U,
  'A', 0x00U, 'C', 0x00U, 'M', 0x00U
};

const uint8_t kSerialStringDescriptor[] = {
  0x1AU, 0x03U,
  'I', 0x00U, 'F', 0x00U, 'L', 0x00U, 'Y', 0x00U, '0', 0x00U,
  '0', 0x00U, '0', 0x00U, '1', 0x00U, '2', 0x00U, '0', 0x00U,
  '2', 0x00U, '6', 0x00U
};

} // 匿名命名空间

namespace ifly {

UsbCdcAcm &UsbCdcAcm::Instance() {
  static UsbCdcAcm instance;
  return instance;
}

/*
 * 绑定底层 PCD 句柄并启动 USB 外设。
 * 这里不直接打开 CDC 数据端点，只有主机完成 SET_CONFIGURATION 后才会打开。
 */
void UsbCdcAcm::Init(PCD_HandleTypeDef *hpcd) {
  if (hpcd == nullptr) {
    return;
  }

  {
    IrqGuard guard;
    pcd_ = hpcd;
    ResetRuntimeState();
  }

  // 配置全速 FIFO 深度，为控制端点和 CDC 数据端点预留空间。
  (void)HAL_PCDEx_SetRxFiFo(pcd_, 0x40U);
  (void)HAL_PCDEx_SetTxFiFo(pcd_, 0U, 0x20U);
  (void)HAL_PCDEx_SetTxFiFo(pcd_, 1U, 0x80U);
  (void)HAL_PCDEx_SetTxFiFo(pcd_, 2U, 0x20U);

  (void)HAL_PCD_Start(pcd_);
}

/*
 * 上层写数据时先拷贝到双缓冲槽位中。
 * 这样即使 USB 中断正在发送另一块数据，也不会破坏调用方传入的原始缓冲区。
 */
uint32_t UsbCdcAcm::Write(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

  uint32_t accepted = 0U;

  {
    IrqGuard guard;

    while (accepted < len) {
      // 查找空闲发送槽位；若两个槽位都被占用，则本次最多只能接收部分数据。
      int freeSlot = -1;
      for (int i = 0; i < 2; ++i) {
        if (!txSlots_[i].queued) {
          freeSlot = i;
          break;
        }
      }

      if (freeSlot < 0) {
        break;
      }

      // 把待发送数据复制到静态缓冲区，由 USB 中断回调负责后续分包发送。
      TxSlot &slot = txSlots_[freeSlot];
      const uint32_t copyLen = MinU32(len - accepted, static_cast<uint32_t>(sizeof(slot.data)));
      (void)memcpy(slot.data, data + accepted, copyLen);
      slot.len = static_cast<uint16_t>(copyLen);
      slot.sent = 0U;
      slot.queued = true;
      accepted += copyLen;
    }

    TryStartTxTransfer();
  }

  return accepted;
}

/*
 * 从接收环形缓冲区取走已经到达的数据。
 * Read 只负责“搬出数据”，不直接操作 USB 端点。
 */
uint32_t UsbCdcAcm::Read(uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return 0U;
  }

  IrqGuard guard;
  return PopRxData(data, len);
}

uint32_t UsbCdcAcm::Available() const {
  IrqGuard guard;
  const uint32_t head = rxHead_;
  const uint32_t tail = rxTail_;
  if (head >= tail) {
    return head - tail;
  }
  return (kRxRingSize - tail) + head;
}

bool UsbCdcAcm::IsConfigured() const {
  return configured_;
}

// -------- 运行期状态与端点管理 --------

/*
 * 恢复到枚举初始状态。
 * 每次 USB 总线复位后都必须把配置、接口、控制传输和收发状态清空。
 */
void UsbCdcAcm::ResetRuntimeState() {
  configured_ = false;
  suspended_ = false;
  currentConfig_ = 0U;
  currentInterface_ = 0U;
  controlLineState_ = 0U;

  ep0OutState_ = Ep0OutState::kIdle;
  ep0OutExpectedLen_ = 0U;
  ep0InPtr_ = nullptr;
  ep0InRemaining_ = 0U;
  ep0InRequestLen_ = 0U;

  txBusy_ = false;
  txActiveSlot_ = -1;
  txLastPacketLen_ = 0U;
  for (int i = 0; i < 2; ++i) {
    txSlots_[i].len = 0U;
    txSlots_[i].sent = 0U;
    txSlots_[i].queued = false;
  }

  rxHead_ = 0U;
  rxTail_ = 0U;
  rxDropped_ = 0U;
}

// 打开控制端点 EP0 的输入和输出方向，所有标准请求与类请求都从这里进入。
void UsbCdcAcm::OpenControlEndpoints() {
  (void)HAL_PCD_EP_Open(pcd_, kEndpoint0Out, kEp0Mps, EP_TYPE_CTRL);
  (void)HAL_PCD_EP_Open(pcd_, kEndpoint0In, kEp0Mps, EP_TYPE_CTRL);
}

// 在主机完成“设置配置”请求后，打开 CDC 的三个数据相关端点。
void UsbCdcAcm::OpenDataEndpoints() {
  (void)HAL_PCD_EP_Open(pcd_, kEpCdcCmdIn, kEpCmdMps, EP_TYPE_INTR);
  (void)HAL_PCD_EP_Open(pcd_, kEpCdcDataOut, kEpDataMps, EP_TYPE_BULK);
  (void)HAL_PCD_EP_Open(pcd_, kEpCdcDataIn, kEpDataMps, EP_TYPE_BULK);
}

// 关闭 CDC 端点并清空发送上下文，避免复位后残留旧发送状态。
void UsbCdcAcm::CloseDataEndpoints() {
  (void)HAL_PCD_EP_Close(pcd_, kEpCdcCmdIn);
  (void)HAL_PCD_EP_Close(pcd_, kEpCdcDataOut);
  (void)HAL_PCD_EP_Close(pcd_, kEpCdcDataIn);
  txBusy_ = false;
  txActiveSlot_ = -1;
  txLastPacketLen_ = 0U;
  for (int i = 0; i < 2; ++i) {
    txSlots_[i].len = 0U;
    txSlots_[i].sent = 0U;
    txSlots_[i].queued = false;
  }
}

// 为批量输出端点挂起下一次接收，使主机后续发包时始终有缓存可写。
void UsbCdcAcm::PrimeOutEndpoint() {
  (void)HAL_PCD_EP_Receive(pcd_, kEpCdcDataOut, rxPacketBuffer_, kEpDataMps);
}

// -------- HAL PCD 回调入口 --------

/*
 * USB 总线复位后重新打开 EP0，并把设备地址恢复为 0。
 * CDC 数据端点不会在这里打开，而是等主机显式配置后再打开。
 */
void UsbCdcAcm::OnReset(PCD_HandleTypeDef *hpcd) {
  if ((pcd_ == nullptr) || (hpcd != pcd_)) {
    return;
  }

  ResetRuntimeState();
  (void)HAL_PCD_SetAddress(pcd_, 0U);
  OpenControlEndpoints();
}

// 解析 8 字节 Setup 包，并按标准请求或类请求分别处理。
void UsbCdcAcm::OnSetupStage(PCD_HandleTypeDef *hpcd) {
  if ((pcd_ == nullptr) || (hpcd != pcd_)) {
    return;
  }

  SetupPacket setup {};
  (void)memcpy(&setup, reinterpret_cast<uint8_t *>(hpcd->Setup), sizeof(SetupPacket));

  const uint8_t reqType = setup.bmRequestType & kReqTypeMask;
  if (reqType == kReqTypeStandard) {
    HandleStandardRequest(setup);
    return;
  }

  if (reqType == kReqTypeClass) {
    HandleClassRequest(setup);
    return;
  }

  StallControlEndpoint();
}

/*
 * IN 方向传输完成回调：
 * 1. 若是 EP0，则继续控制传输的后续分包；
 * 2. 若是 CDC BULK IN，则推进双缓冲槽位的发送进度。
 */
void UsbCdcAcm::OnDataInStage(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  if ((pcd_ == nullptr) || (hpcd != pcd_)) {
    return;
  }

  if ((epnum & 0x7FU) == 0U) {
    ContinueControlInTransfer();
    return;
  }

  if ((epnum & 0x7FU) == (kEpCdcDataIn & 0x7FU)) {
    if ((txActiveSlot_ >= 0) && txBusy_) {
      TxSlot &slot = txSlots_[txActiveSlot_];
      slot.sent = static_cast<uint16_t>(slot.sent + txLastPacketLen_);
      if (slot.sent < slot.len) {
        const uint16_t packetLen = MinU16(static_cast<uint16_t>(slot.len - slot.sent), kEpDataMps);
        txLastPacketLen_ = packetLen;
        (void)HAL_PCD_EP_Transmit(pcd_, kEpCdcDataIn, slot.data + slot.sent, packetLen);
      } else {
        slot.len = 0U;
        slot.sent = 0U;
        slot.queued = false;
        txBusy_ = false;
        txActiveSlot_ = -1;
        txLastPacketLen_ = 0U;
        TryStartTxTransfer();
      }
    }
  }
}

/*
 * OUT 方向传输完成回调：
 * 1. EP0 OUT 用于接收控制请求的数据阶段，例如 SET_LINE_CODING；
 * 2. CDC BULK OUT 收到的数据会先进入单包缓存，再搬运到环形缓冲区。
 */
void UsbCdcAcm::OnDataOutStage(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  if ((pcd_ == nullptr) || (hpcd != pcd_)) {
    return;
  }

  if ((epnum & 0x7FU) == 0U) {
    if (ep0OutState_ == Ep0OutState::kSetLineCoding) {
      const uint32_t rxLen = HAL_PCD_EP_GetRxCount(pcd_, kEndpoint0Out);
      if (rxLen >= 7U) {
        lineCoding_.baudrate = (static_cast<uint32_t>(ep0OutBuffer_[0]) << 0) |
                               (static_cast<uint32_t>(ep0OutBuffer_[1]) << 8) |
                               (static_cast<uint32_t>(ep0OutBuffer_[2]) << 16) |
                               (static_cast<uint32_t>(ep0OutBuffer_[3]) << 24);
        lineCoding_.stopBits = ep0OutBuffer_[4];
        lineCoding_.parityType = ep0OutBuffer_[5];
        lineCoding_.dataBits = ep0OutBuffer_[6];
      }
      ep0OutState_ = Ep0OutState::kIdle;
      ep0OutExpectedLen_ = 0U;
      SendControlStatus();
    }
    return;
  }

  if ((epnum & 0x7FU) == (kEpCdcDataOut & 0x7FU)) {
    const uint32_t rxLen = HAL_PCD_EP_GetRxCount(pcd_, kEpCdcDataOut);
    if (rxLen > 0U) {
      PushRxData(rxPacketBuffer_, rxLen);
    }
    PrimeOutEndpoint();
  }
}

void UsbCdcAcm::OnSuspend(PCD_HandleTypeDef *hpcd) {
  if ((pcd_ != nullptr) && (hpcd == pcd_)) {
    suspended_ = true;
  }
}

void UsbCdcAcm::OnResume(PCD_HandleTypeDef *hpcd) {
  if ((pcd_ != nullptr) && (hpcd == pcd_)) {
    suspended_ = false;
  }
}

// -------- 控制传输处理 --------

/*
 * 处理标准 USB 请求。
 * 这里实现了 CDC 枚举所必需的一组最小请求集合。
 */
void UsbCdcAcm::HandleStandardRequest(const SetupPacket &setup) {
  switch (setup.bRequest) {
  case kReqGetDescriptor:
    HandleGetDescriptor(setup);
    break;

  case kReqSetAddress:
    if ((setup.wIndex == 0U) &&
        (setup.wLength == 0U) &&
        ((setup.wValue & 0x7FU) == setup.wValue)) {
            // 按 STM32 官方 USB 设备协议栈的处理方式，先写入地址，再返回状态包，
      // 避免主机在地址切换后的下一阶段控制传输中重复复位设备。
      (void)HAL_PCD_SetAddress(pcd_, static_cast<uint8_t>(setup.wValue & 0x7FU));
      SendControlStatus();
    } else {
      StallControlEndpoint();
    }
    break;

  case kReqSetConfiguration:
    if ((setup.wLength == 0U) &&
        ((setup.wValue == 0U) || (setup.wValue == kCdcConfigValue))) {
      if (setup.wValue == kCdcConfigValue) {
        if (configured_) {
          CloseDataEndpoints();
        }
        OpenDataEndpoints();
        PrimeOutEndpoint();
        configured_ = true;
        currentConfig_ = 1U;
      } else {
        CloseDataEndpoints();
        configured_ = false;
        currentConfig_ = 0U;
      }
      SendControlStatus();
    } else {
      StallControlEndpoint();
    }
    break;

  case kReqGetConfiguration:
    ep0OutBuffer_[0] = currentConfig_;
    StartControlInTransfer(ep0OutBuffer_, 1U, setup.wLength);
    break;

  case kReqGetStatus: {
    // 当前最小实现不声明远程唤醒和端点暂停状态，因此固定返回 0。
    ep0OutBuffer_[0] = 0U;
    ep0OutBuffer_[1] = 0U;
    StartControlInTransfer(ep0OutBuffer_, 2U, setup.wLength);
    break;
  }

  case kReqSetInterface:
    if ((setup.wLength == 0U) && (setup.wValue == 0U)) {
      currentInterface_ = 0U;
      SendControlStatus();
    } else {
      StallControlEndpoint();
    }
    break;

  case kReqGetInterface:
    ep0OutBuffer_[0] = currentInterface_;
    StartControlInTransfer(ep0OutBuffer_, 1U, setup.wLength);
    break;

  case kReqSetFeature:
  case kReqClearFeature: {
    const uint8_t recipient = setup.bmRequestType & kRecipientMask;
    if ((recipient == kRecipientEndpoint) &&
        (setup.wValue == kFeatureEndpointHalt) &&
        (setup.wLength == 0U)) {
      const uint8_t epAddr = static_cast<uint8_t>(setup.wIndex & 0xFFU);
      if (setup.bRequest == kReqSetFeature) {
        (void)HAL_PCD_EP_SetStall(pcd_, epAddr);
      } else {
        (void)HAL_PCD_EP_ClrStall(pcd_, epAddr);
      }
      SendControlStatus();
    } else {
      StallControlEndpoint();
    }
    break;
  }

  default:
    StallControlEndpoint();
    break;
  }
}

/*
 * 处理 CDC 类请求。
 * 当前实现覆盖了主机常见串口工具会使用的 Line Coding 和 Control Line State。
 */
void UsbCdcAcm::HandleClassRequest(const SetupPacket &setup) {
  const uint8_t recipient = setup.bmRequestType & kRecipientMask;
  if (recipient != kRecipientInterface) {
    StallControlEndpoint();
    return;
  }

  switch (setup.bRequest) {
  case kCdcReqSetLineCoding:
    if ((setup.wLength == 7U) && ((setup.bmRequestType & 0x80U) == 0U)) {
      ep0OutState_ = Ep0OutState::kSetLineCoding;
      ep0OutExpectedLen_ = 7U;
      (void)HAL_PCD_EP_Receive(pcd_, kEndpoint0Out, ep0OutBuffer_, ep0OutExpectedLen_);
    } else {
      StallControlEndpoint();
    }
    break;

  case kCdcReqGetLineCoding:
    if ((setup.wLength == 7U) && ((setup.bmRequestType & 0x80U) != 0U)) {
      ep0OutBuffer_[0] = static_cast<uint8_t>(lineCoding_.baudrate & 0xFFU);
      ep0OutBuffer_[1] = static_cast<uint8_t>((lineCoding_.baudrate >> 8) & 0xFFU);
      ep0OutBuffer_[2] = static_cast<uint8_t>((lineCoding_.baudrate >> 16) & 0xFFU);
      ep0OutBuffer_[3] = static_cast<uint8_t>((lineCoding_.baudrate >> 24) & 0xFFU);
      ep0OutBuffer_[4] = lineCoding_.stopBits;
      ep0OutBuffer_[5] = lineCoding_.parityType;
      ep0OutBuffer_[6] = lineCoding_.dataBits;
      StartControlInTransfer(ep0OutBuffer_, 7U, setup.wLength);
    } else {
      StallControlEndpoint();
    }
    break;

  case kCdcReqSetControlLineState:
    if (setup.wLength == 0U) {
      controlLineState_ = static_cast<uint8_t>(setup.wValue & 0x03U);
      SendControlStatus();
    } else {
      StallControlEndpoint();
    }
    break;

  default:
    StallControlEndpoint();
    break;
  }
}

// 根据 wValue 高字节判断描述符类型，低字节判断字符串描述符索引。
void UsbCdcAcm::HandleGetDescriptor(const SetupPacket &setup) {
  const uint8_t descriptorType = static_cast<uint8_t>((setup.wValue >> 8) & 0xFFU);
  const uint8_t descriptorIndex = static_cast<uint8_t>(setup.wValue & 0xFFU);

  const uint8_t *descriptor = nullptr;
  uint16_t descriptorLen = 0U;

  switch (descriptorType) {
  case kDescTypeDevice:
    descriptor = kDeviceDescriptor;
    descriptorLen = static_cast<uint16_t>(sizeof(kDeviceDescriptor));
    break;

  case kDescTypeConfiguration:
    descriptor = kConfigurationDescriptor;
    descriptorLen = static_cast<uint16_t>(sizeof(kConfigurationDescriptor));
    break;

  case kDescTypeString:
    if (descriptorIndex == 0U) {
      descriptor = kLangIdStringDescriptor;
      descriptorLen = static_cast<uint16_t>(sizeof(kLangIdStringDescriptor));
    } else if (descriptorIndex == 1U) {
      descriptor = kManufacturerStringDescriptor;
      descriptorLen = static_cast<uint16_t>(sizeof(kManufacturerStringDescriptor));
    } else if (descriptorIndex == 2U) {
      descriptor = kProductStringDescriptor;
      descriptorLen = static_cast<uint16_t>(sizeof(kProductStringDescriptor));
    } else if (descriptorIndex == 3U) {
      descriptor = kSerialStringDescriptor;
      descriptorLen = static_cast<uint16_t>(sizeof(kSerialStringDescriptor));
    } else {
      descriptor = nullptr;
      descriptorLen = 0U;
    }
    break;

  default:
    descriptor = nullptr;
    descriptorLen = 0U;
    break;
  }

  if ((descriptor == nullptr) || (descriptorLen == 0U)) {
    StallControlEndpoint();
    return;
  }

  StartControlInTransfer(descriptor, descriptorLen, setup.wLength);
}

/*
 * 启动一次 EP0 IN 控制传输。
 * 若描述符长度大于 EP0 最大包长，则只先发送第一包，后续由 IN 完成回调继续补发。
 */
void UsbCdcAcm::StartControlInTransfer(const uint8_t *data, uint16_t len, uint16_t requestLen) {
  ep0InPtr_ = data;
  ep0InRequestLen_ = requestLen;
  ep0InRemaining_ = MinU16(len, requestLen);

  if (ep0InRemaining_ == 0U) {
    (void)HAL_PCD_EP_Transmit(pcd_, kEndpoint0In, &ep0ZlpDummy_, 0U);
    return;
  }

  const uint16_t packetLen = MinU16(ep0InRemaining_, kEp0Mps);
  (void)HAL_PCD_EP_Transmit(pcd_, kEndpoint0In, const_cast<uint8_t *>(ep0InPtr_), packetLen);
  ep0InPtr_ += packetLen;
  ep0InRemaining_ = static_cast<uint16_t>(ep0InRemaining_ - packetLen);
}

// 在 EP0 输入完成回调里继续后续分包；全部发完后切回状态阶段。
void UsbCdcAcm::ContinueControlInTransfer() {
  if (ep0InRemaining_ == 0U) {
    ep0InPtr_ = nullptr;
    ep0InRequestLen_ = 0U;
    (void)HAL_PCD_EP_Receive(pcd_, kEndpoint0Out, ep0OutBuffer_, 0U);
    return;
  }

  const uint16_t packetLen = MinU16(ep0InRemaining_, kEp0Mps);
  (void)HAL_PCD_EP_Transmit(pcd_, kEndpoint0In, const_cast<uint8_t *>(ep0InPtr_), packetLen);
  ep0InPtr_ += packetLen;
  ep0InRemaining_ = static_cast<uint16_t>(ep0InRemaining_ - packetLen);
}

void UsbCdcAcm::SendControlStatus() {
  (void)HAL_PCD_EP_Transmit(pcd_, kEndpoint0In, &ep0ZlpDummy_, 0U);
}

void UsbCdcAcm::StallControlEndpoint() {
  (void)HAL_PCD_EP_SetStall(pcd_, kEndpoint0In);
  (void)HAL_PCD_EP_SetStall(pcd_, kEndpoint0Out);
}

// -------- 数据缓冲管理 --------

/*
 * 把 USB 收到的一包数据逐字节写入环形缓冲区。
 * 若环形缓冲区已满，则丢弃新字节并累计 rxDropped_ 计数。
 */
void UsbCdcAcm::PushRxData(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return;
  }

  for (uint32_t i = 0U; i < len; ++i) {
    const uint32_t nextHead = (rxHead_ + 1U) % kRxRingSize;
    if (nextHead == rxTail_) {
      rxDropped_++;
      continue;
    }
    rxRing_[rxHead_] = data[i];
    rxHead_ = nextHead;
  }
}

// 从环形缓冲区顺序读出数据，供主循环或其他上层逻辑消费。
uint32_t UsbCdcAcm::PopRxData(uint8_t *data, uint32_t len) {
  uint32_t readLen = 0U;
  while ((readLen < len) && (rxTail_ != rxHead_)) {
    data[readLen++] = rxRing_[rxTail_];
    rxTail_ = (rxTail_ + 1U) % kRxRingSize;
  }
  return readLen;
}

/*
 * 若当前 USB IN 端点空闲，则尝试从双缓冲队列中取出一个槽位启动发送。
 * 真正的连续分包推进由 OnDataInStage 回调完成。
 */
void UsbCdcAcm::TryStartTxTransfer() {
  if ((pcd_ == nullptr) || !configured_ || suspended_ || txBusy_) {
    return;
  }

  for (int i = 0; i < 2; ++i) {
    TxSlot &slot = txSlots_[i];
    if (slot.queued && (slot.sent < slot.len)) {
      const uint16_t packetLen = MinU16(static_cast<uint16_t>(slot.len - slot.sent), kEpDataMps);
      if (HAL_PCD_EP_Transmit(pcd_, kEpCdcDataIn, slot.data + slot.sent, packetLen) == HAL_OK) {
        txBusy_ = true;
        txActiveSlot_ = static_cast<int8_t>(i);
        txLastPacketLen_ = packetLen;
      }
      return;
    }
  }
}

} // ifly 命名空间

extern "C" {

// -------- C 接口与 HAL 回调桥接 --------

void IFly_USBCDC_Init(PCD_HandleTypeDef *hpcd) {
  ifly::UsbCdcAcm::Instance().Init(hpcd);
}

uint32_t IFly_USBCDC_Write(const uint8_t *data, uint32_t len) {
  return ifly::UsbCdcAcm::Instance().Write(data, len);
}

uint32_t IFly_USBCDC_Read(uint8_t *data, uint32_t len) {
  return ifly::UsbCdcAcm::Instance().Read(data, len);
}

uint32_t IFly_USBCDC_Available(void) {
  return ifly::UsbCdcAcm::Instance().Available();
}

uint8_t IFly_USBCDC_IsConfigured(void) {
  return ifly::UsbCdcAcm::Instance().IsConfigured() ? 1U : 0U;
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
  ifly::UsbCdcAcm::Instance().OnReset(hpcd);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
  ifly::UsbCdcAcm::Instance().OnSetupStage(hpcd);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  ifly::UsbCdcAcm::Instance().OnDataInStage(hpcd, epnum);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
  ifly::UsbCdcAcm::Instance().OnDataOutStage(hpcd, epnum);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
  ifly::UsbCdcAcm::Instance().OnSuspend(hpcd);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {
  ifly::UsbCdcAcm::Instance().OnResume(hpcd);
}

} // extern "C" 结束










