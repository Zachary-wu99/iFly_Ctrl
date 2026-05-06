/**
 * @file dronecan_node.hpp
 * @brief DroneCAN 节点测试接口。
 */
#ifndef IFLY_DRONECAN_NODE_HPP
#define IFLY_DRONECAN_NODE_HPP

#include <stdint.h>

#include "hardware_can.hpp"
#include "canard.h"

namespace iFly {

/**
 * @brief DroneCAN 测试节点。
 *
 * @details
 * 该类负责把工程内 CAN 帧队列接入 libcanard，并周期广播节点状态和测试 IMU 数据。
 */
class DroneCanNode final {
public:
  /**
   * @brief DroneCAN 节点配置。
   */
  struct Config final {
    CanPortId port = CanPortId::kCan1; /**< 使用的 CAN 逻辑端口。 */
    uint8_t node_id = 42U; /**< 固定 DroneCAN 节点 ID。 */
  };

  /**
   * @brief 构造默认 DroneCAN 节点。
   */
  DroneCanNode();

  /**
   * @brief 构造指定配置的 DroneCAN 节点。
   *
   * @param config 节点配置。
   */
  explicit DroneCanNode(const Config &config);

  /**
   * @brief 初始化 DroneCAN 节点。
   *
   * @return 初始化成功返回 `true`。
   */
  bool Init();

  /**
   * @brief 轮询 DroneCAN 收发与周期发布。
   */
  void Poll();

  /**
   * @brief 判断节点是否完成初始化。
   *
   * @return 已初始化返回 `true`。
   */
  bool IsReady() const {
    return initialized_;
  }

private:
  static constexpr uint32_t kRxPoolFrameCount = 80U; /**< CAN 接收对象池帧数。 */
  static constexpr uint32_t kTxPoolFrameCount = 80U; /**< CAN 发送对象池帧数。 */
  static constexpr uint32_t kCanardMemoryPoolSize = 4096U; /**< libcanard 内存池大小。 */

  static bool ShouldAcceptTransferThunk(const CanardInstance *instance,
                                        uint64_t *out_data_type_signature,
                                        uint16_t data_type_id,
                                        CanardTransferType transfer_type,
                                        uint8_t source_node_id);
  static void OnTransferThunk(CanardInstance *instance,
                              CanardRxTransfer *transfer);

  bool ShouldAcceptTransfer(uint64_t *out_data_type_signature,
                            uint16_t data_type_id,
                            CanardTransferType transfer_type,
                            uint8_t source_node_id) const;
  void OnTransfer(CanardRxTransfer *transfer);

  void DrainRx(uint64_t now_us);
  void FlushTx();
  void CleanupStaleTransfers(uint64_t now_us);
  void PublishNodeStatus(uint64_t now_us);
  void PublishRawImu(uint64_t now_us);
  void SendGetNodeInfoResponse(CanardRxTransfer *transfer);

  Config config_ {};
  HardwareCan<kRxPoolFrameCount, kTxPoolFrameCount> can_;
  CanardInstance canard_ {};
  uint8_t canard_pool_[kCanardMemoryPoolSize] {};
  bool initialized_ = false;
  uint64_t started_us_ = 0ULL;
  uint64_t last_node_status_us_ = 0ULL;
  uint64_t last_raw_imu_us_ = 0ULL;
  uint64_t last_cleanup_us_ = 0ULL;
  uint8_t node_status_transfer_id_ = 0U;
  uint8_t raw_imu_transfer_id_ = 0U;
};

} // namespace iFly

#endif /* IFLY_DRONECAN_NODE_HPP */
