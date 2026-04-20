// CAN 底层服务接口。
// 定义端口编号、帧封装格式以及 HAL 桥接服务。
#ifndef IFLY_CAN_HPP
#define IFLY_CAN_HPP

#include <stdint.h>

#include "can.h"
#include "lock_free_queue.hpp"

namespace iFly {

/**
 * @brief 软件层统一支持的 CAN 端口编号。
 *
 * @details
 * 这里的含义是“软件抽象支持两路 CAN”：
 * - `kCan1` 表示第 1 路逻辑 CAN 端口
 * - `kCan2` 表示第 2 路逻辑 CAN 端口
 *
 * 它不等于“芯片一定真的焊了两路 CAN 外设”。
 * 当前工程默认只自动映射了 `CAN1`，`CAN2` 这个槽位只是预留好，
 * 以后如果硬件上有第二路 CAN，再把对应的 `CAN_HandleTypeDef *`
 * 挂进来即可。
 */
enum class CanPortId : uint8_t {
  kCan1 = 0U,
  kCan2 = 1U,
  kCount = 2U
};

const char *ToString(CanPortId port);

enum CanFrameFlags : uint8_t {
  /** 这一帧使用扩展帧 ID（29 位）而不是标准帧 ID（11 位）。 */
  kCanFrameFlagExtendedId = 1U << 0,
  /** 这一帧是远程帧 RTR，不是普通数据帧。 */
  kCanFrameFlagRemoteFrame = 1U << 1,
  /** 这一帧来自硬件的 FIFO1，仅用于接收方向记录来源。 */
  kCanFrameFlagRxFifo1 = 1U << 2
};

/**
 * @brief CAN 上下层统一使用的固定帧封包格式。
 *
 * @details
 * CAN 和 UART 最大的不同点在于：
 * - UART 更像“连续字节流”
 * - CAN 天然是“一帧一帧”的消息
 *
 * 但这个工程上层统一继承了 `SerialIoBase`，它的接口是按字节流来的，
 * 所以这里把一帧 CAN 包装成一个固定大小的“软件封包”。
 *
 * 这样做的好处是：
 * - 上层仍然可以复用统一的 `Write()/Read()` 风格接口
 * - 底层不会丢掉 CAN 的帧边界
 * - 队列里每次搬运的单位总是完整的一帧，逻辑更稳定
 *
 * 字段含义：
 * - `id`：报文 ID。标准帧用低 11 位，扩展帧用低 29 位
 * - `dlc`：这一帧有效数据长度，范围 0~8
 * - `flags`：帧类型标志，例如扩展帧、远程帧
 * - `filterIndex`：接收时命中的硬件滤波器索引
 * - `data[8]`：最多 8 字节数据
 *
 * 使用约定：
 * - `Write()` 传入的原始字节，必须按 `CanFramePacket` 整帧排列
 * - `Read()` 读出的原始字节，也总是按 `CanFramePacket` 整帧返回
 * - 如果你不想自己关心打包细节，优先使用 `WriteFrame()/ReadFrame()`
 */
struct CanFramePacket final {
  uint32_t id = 0U;
  uint8_t dlc = 0U;
  uint8_t flags = 0U;
  uint8_t filterIndex = 0U;
  uint8_t reserved = 0U;
  uint8_t data[8] {};
};

static_assert(sizeof(CanFramePacket) == 16U, "CanFramePacket size must stay fixed at 16 bytes.");

/**
 * @brief CAN 运行时服务。
 *
 * RX path: HAL FIFO -> upper RX lock-free queue.
 * TX path: TX lock-free queue -> double buffer -> HAL mailbox.
 */
class CanService final {
public:
  /** 软件层最多维护两路逻辑 CAN 端口。 */
  static constexpr uint8_t kMaxPorts = 2U;
  /** 一个软件 CAN 帧封包的固定大小。 */
  static constexpr uint32_t kCanFramePacketSize = sizeof(CanFramePacket);
  /** 发送队列最多缓存多少帧。 */
  static constexpr uint32_t kFixedTxQueueFrameCount = 64U;
  /** 发送无锁队列底层总存储大小，注意队列内部保留 1 字节哨兵。 */
  static constexpr uint32_t kFixedTxQueueStorageSize =
      (kFixedTxQueueFrameCount * kCanFramePacketSize) + 1U;
  /** 获取全局唯一的 CAN 服务单例。 */
  static CanService &Instance();

  /** 手动把某一路逻辑 CAN 端口绑定到具体的 HAL 句柄。 */
  void AttachHardware(CanPortId port, CAN_HandleTypeDef *hcan);
  /** 初始化某一路 CAN 端口，并挂接上层接收队列。 */
  bool InitPort(CanPortId port, LockFreeQueueBase *rxQueue);
  /** 反初始化某一路端口，停止硬件并清空运行时状态。 */
  void DeinitPort(CanPortId port);

  /**
   * @brief 原始字节方式写入待发送数据。
   *
   * @details
   * 这里不是“随便来几个字节就发出去”，而是必须按
   * `CanFramePacket` 整帧写入。
   */
  uint32_t Write(CanPortId port, const uint8_t *data, uint32_t len);
  /** 直接按一帧 CAN 报文写入，通常比原始 `Write()` 更直观。 */
  bool WriteFrame(CanPortId port, const CanFramePacket &frame);
  /** 查询发送队列剩余空间，单位是字节。 */
  uint32_t TxFree(CanPortId port) const;
  /** 查询发送队列已使用空间，单位是字节。 */
  uint32_t TxUsed(CanPortId port) const;
  /** 查询接收过程中累计丢弃的字节数。 */
  uint32_t RxDropped(CanPortId port) const;
  /** 查询这一端口当前是否已经完成初始化并可工作。 */
  bool IsReady(CanPortId port) const;

  /** @brief 兼容性空钩子。当前 RX 已经在 HAL 回调中直接上抛到上层队列。 */
  void ServiceRxPath(CanPortId port);

  /** HAL 告知某个 RX FIFO 有报文待取时的桥接入口。 */
  void OnRxFifoPending(CAN_HandleTypeDef *hcan, uint32_t fifo);
  /** HAL 告知某个 RX FIFO 已满时的桥接入口。 */
  void OnRxFifoFull(CAN_HandleTypeDef *hcan, uint32_t fifo);
  /** HAL 告知某个发送邮箱完成发送时的桥接入口。 */
  void OnTxComplete(CAN_HandleTypeDef *hcan);
  /** HAL 告知某个发送邮箱发送中止时的桥接入口。 */
  void OnTxAbort(CAN_HandleTypeDef *hcan);
  /** HAL 告知 CAN 外设出错时的桥接入口。 */
  void OnError(CAN_HandleTypeDef *hcan);

private:
  CanService() = default;
};

} // namespace iFly

#endif /* IFLY_CAN_HPP */
