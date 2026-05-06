#include "dronecan_node.hpp"

#include <string.h>

#include "tick.hpp"
#include "uavcan.equipment.ahrs.RawIMU.h"
#include "uavcan.protocol.GetNodeInfo.h"
#include "uavcan.protocol.NodeStatus.h"

namespace {

constexpr uint8_t kDefaultNodeId = 42U;
constexpr uint64_t kNodeStatusPeriodUs = 1000000ULL;
constexpr uint64_t kRawImuPeriodUs = 10000ULL;
constexpr float kRawImuIntegrationInterval = 0.01F;
constexpr uint8_t kBoardId = 23U;
constexpr char kNodeName[] = "org.ifly.ctrl.test";

uint8_t NormalizeNodeId(uint8_t node_id)
{
  if ((node_id < CANARD_MIN_NODE_ID) || (node_id > CANARD_MAX_NODE_ID)) {
    return kDefaultNodeId;
  }

  return node_id;
}

bool IsExpired(uint64_t now_us, uint64_t last_us, uint64_t period_us)
{
  return (last_us == 0ULL) || ((now_us - last_us) >= period_us);
}

iFly::CanFramePacket ToCanPacket(const CanardCANFrame &frame)
{
  iFly::CanFramePacket packet {};
  packet.id = frame.id & CANARD_CAN_EXT_ID_MASK;
  packet.dlc = frame.data_len;
  if (packet.dlc > sizeof(packet.data)) {
    packet.dlc = sizeof(packet.data);
  }

  packet.flags = iFly::kCanFrameFlagExtendedId;
  if ((frame.id & CANARD_CAN_FRAME_RTR) != 0U) {
    packet.flags |= iFly::kCanFrameFlagRemoteFrame;
  }

  (void)memcpy(packet.data, frame.data, packet.dlc);
  return packet;
}

CanardCANFrame ToCanardFrame(const iFly::CanFramePacket &packet)
{
  CanardCANFrame frame {};
  frame.id = (packet.id & CANARD_CAN_EXT_ID_MASK) | CANARD_CAN_FRAME_EFF;
  if ((packet.flags & iFly::kCanFrameFlagRemoteFrame) != 0U) {
    frame.id |= CANARD_CAN_FRAME_RTR;
  }

  frame.data_len = packet.dlc;
  if (frame.data_len > sizeof(frame.data)) {
    frame.data_len = sizeof(frame.data);
  }

  (void)memcpy(frame.data, packet.data, frame.data_len);
  return frame;
}

void FillNodeStatus(struct uavcan_protocol_NodeStatus *status,
                    uint64_t started_us,
                    uint64_t now_us)
{
  if (status == nullptr) {
    return;
  }

  status->uptime_sec = static_cast<uint32_t>((now_us - started_us) / 1000000ULL);
  status->health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
  status->mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
  status->sub_mode = 0U;
  status->vendor_specific_status_code = 0U;
}

void FillUniqueId(uint8_t *unique_id)
{
  if (unique_id == nullptr) {
    return;
  }

  static constexpr uint8_t kIflyCtrlUniqueId[16] = {
      'i', 'F', 'l', 'y',
      '_', 'C', 't', 'r',
      'l', 0U, 0U, 0U,
      0U, 0U, 0U, kBoardId,
  };

  (void)memcpy(unique_id, kIflyCtrlUniqueId, sizeof(kIflyCtrlUniqueId));
}

void FillNodeName(struct uavcan_protocol_GetNodeInfoResponse *response)
{
  if (response == nullptr) {
    return;
  }

  uint8_t length = 0U;
  while ((length < sizeof(response->name.data)) && (kNodeName[length] != '\0')) {
    response->name.data[length] = static_cast<uint8_t>(kNodeName[length]);
    ++length;
  }

  response->name.len = length;
}

void FillRawImu(struct uavcan_equipment_ahrs_RawIMU *raw_imu, uint64_t now_us)
{
  if (raw_imu == nullptr) {
    return;
  }

  const uint32_t sample_index =
      static_cast<uint32_t>((now_us / kRawImuPeriodUs) % 200ULL);
  const float wave = (static_cast<float>(sample_index) - 100.0F) / 100.0F;

  raw_imu->timestamp.usec = now_us;
  raw_imu->integration_interval = kRawImuIntegrationInterval;
  raw_imu->rate_gyro_latest[0] = wave * 0.05F;
  raw_imu->rate_gyro_latest[1] = (1.0F - wave) * 0.02F;
  raw_imu->rate_gyro_latest[2] = wave * -0.03F;
  raw_imu->accelerometer_latest[0] = wave * 0.10F;
  raw_imu->accelerometer_latest[1] = wave * -0.08F;
  raw_imu->accelerometer_latest[2] = 9.80665F;

  for (uint8_t index = 0U; index < 3U; ++index) {
    raw_imu->rate_gyro_integral[index] =
        raw_imu->rate_gyro_latest[index] * raw_imu->integration_interval;
    raw_imu->accelerometer_integral[index] =
        raw_imu->accelerometer_latest[index] * raw_imu->integration_interval;
  }

  raw_imu->covariance.len = 0U;
}

} // namespace

namespace iFly {

DroneCanNode::DroneCanNode() : DroneCanNode(Config {}) {
}

DroneCanNode::DroneCanNode(const Config &config)
    : config_(config), can_(config.port) {
  config_.node_id = NormalizeNodeId(config_.node_id);
}

bool DroneCanNode::Init()
{
  if (initialized_) {
    return true;
  }

  can_.Init();
  if (!can_.IsConnected()) {
    return false;
  }

  canardInit(&canard_,
             canard_pool_,
             sizeof(canard_pool_),
             &DroneCanNode::OnTransferThunk,
             &DroneCanNode::ShouldAcceptTransferThunk,
             this);
  canardSetLocalNodeID(&canard_, config_.node_id);

  started_us_ = tick::NowUs();
  last_node_status_us_ = 0ULL;
  last_raw_imu_us_ = 0ULL;
  last_cleanup_us_ = started_us_;
  node_status_transfer_id_ = 0U;
  raw_imu_transfer_id_ = 0U;
  initialized_ = true;
  return true;
}

void DroneCanNode::Poll()
{
  if (!initialized_ && !Init()) {
    return;
  }

  const uint64_t now_us = tick::NowUs();

  DrainRx(now_us);

  if (IsExpired(now_us, last_node_status_us_, kNodeStatusPeriodUs)) {
    PublishNodeStatus(now_us);
    last_node_status_us_ = now_us;
  }

  if (IsExpired(now_us, last_raw_imu_us_, kRawImuPeriodUs)) {
    PublishRawImu(now_us);
    last_raw_imu_us_ = now_us;
  }

  CleanupStaleTransfers(now_us);
  FlushTx();
}

bool DroneCanNode::ShouldAcceptTransferThunk(const CanardInstance *instance,
                                             uint64_t *out_data_type_signature,
                                             uint16_t data_type_id,
                                             CanardTransferType transfer_type,
                                             uint8_t source_node_id)
{
  DroneCanNode *node =
      static_cast<DroneCanNode *>(canardGetUserReference(instance));
  if (node == nullptr) {
    return false;
  }

  return node->ShouldAcceptTransfer(out_data_type_signature,
                                    data_type_id,
                                    transfer_type,
                                    source_node_id);
}

void DroneCanNode::OnTransferThunk(CanardInstance *instance,
                                   CanardRxTransfer *transfer)
{
  DroneCanNode *node =
      static_cast<DroneCanNode *>(canardGetUserReference(instance));
  if (node == nullptr) {
    return;
  }

  node->OnTransfer(transfer);
}

bool DroneCanNode::ShouldAcceptTransfer(uint64_t *out_data_type_signature,
                                        uint16_t data_type_id,
                                        CanardTransferType transfer_type,
                                        uint8_t source_node_id) const
{
  (void)source_node_id;

  if (out_data_type_signature == nullptr) {
    return false;
  }

  if ((transfer_type == CanardTransferTypeRequest) &&
      (data_type_id == UAVCAN_PROTOCOL_GETNODEINFO_ID)) {
    *out_data_type_signature = UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE;
    return true;
  }

  return false;
}

void DroneCanNode::OnTransfer(CanardRxTransfer *transfer)
{
  if (transfer == nullptr) {
    return;
  }

  if ((transfer->transfer_type == CanardTransferTypeRequest) &&
      (transfer->data_type_id == UAVCAN_PROTOCOL_GETNODEINFO_ID)) {
    SendGetNodeInfoResponse(transfer);
  }
}

void DroneCanNode::DrainRx(uint64_t now_us)
{
  CanFramePacket packet {};
  while (can_.ReadFrame(&packet)) {
    if ((packet.flags & kCanFrameFlagExtendedId) == 0U) {
      continue;
    }

    const CanardCANFrame frame = ToCanardFrame(packet);
    (void)canardHandleRxFrame(&canard_, &frame, now_us);
  }
}

void DroneCanNode::FlushTx()
{
  for (;;) {
    CanardCANFrame *frame = canardPeekTxQueue(&canard_);
    if (frame == nullptr) {
      return;
    }

    const CanFramePacket packet = ToCanPacket(*frame);
    if (!can_.WriteFrame(packet)) {
      return;
    }

    canardPopTxQueue(&canard_);
  }
}

void DroneCanNode::CleanupStaleTransfers(uint64_t now_us)
{
  if (!IsExpired(now_us,
                 last_cleanup_us_,
                 CANARD_RECOMMENDED_STALE_TRANSFER_CLEANUP_INTERVAL_USEC)) {
    return;
  }

  canardCleanupStaleTransfers(&canard_, now_us);
  last_cleanup_us_ = now_us;
}

void DroneCanNode::PublishNodeStatus(uint64_t now_us)
{
  struct uavcan_protocol_NodeStatus status {};
  FillNodeStatus(&status, started_us_, now_us);

  uint8_t payload[UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE] {};
  const uint32_t payload_length =
      uavcan_protocol_NodeStatus_encode(&status, payload);

  (void)canardBroadcast(&canard_,
                        UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE,
                        UAVCAN_PROTOCOL_NODESTATUS_ID,
                        &node_status_transfer_id_,
                        CANARD_TRANSFER_PRIORITY_LOW,
                        payload,
                        static_cast<uint16_t>(payload_length));
}

void DroneCanNode::PublishRawImu(uint64_t now_us)
{
  struct uavcan_equipment_ahrs_RawIMU raw_imu {};
  FillRawImu(&raw_imu, now_us);

  uint8_t payload[UAVCAN_EQUIPMENT_AHRS_RAWIMU_MAX_SIZE] {};
  const uint32_t payload_length =
      uavcan_equipment_ahrs_RawIMU_encode(&raw_imu, payload);

  (void)canardBroadcast(&canard_,
                        UAVCAN_EQUIPMENT_AHRS_RAWIMU_SIGNATURE,
                        UAVCAN_EQUIPMENT_AHRS_RAWIMU_ID,
                        &raw_imu_transfer_id_,
                        CANARD_TRANSFER_PRIORITY_LOW,
                        payload,
                        static_cast<uint16_t>(payload_length));
}

void DroneCanNode::SendGetNodeInfoResponse(CanardRxTransfer *transfer)
{
  struct uavcan_protocol_GetNodeInfoRequest request {};
  if (uavcan_protocol_GetNodeInfoRequest_decode(transfer, &request)) {
    return;
  }

  const uint64_t now_us = tick::NowUs();
  struct uavcan_protocol_GetNodeInfoResponse response {};
  FillNodeStatus(&response.status, started_us_, now_us);
  response.software_version.major = 0U;
  response.software_version.minor = 1U;
  response.software_version.optional_field_flags = 0U;
  response.hardware_version.major = 1U;
  response.hardware_version.minor = 0U;
  FillUniqueId(response.hardware_version.unique_id);
  response.hardware_version.certificate_of_authenticity.len = 0U;
  FillNodeName(&response);

  uint8_t payload[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE] {};
  const uint32_t payload_length =
      uavcan_protocol_GetNodeInfoResponse_encode(&response, payload);

  uint8_t transfer_id = transfer->transfer_id;
  const uint8_t destination_node_id = transfer->source_node_id;
  const uint8_t priority = transfer->priority;
  canardReleaseRxTransferPayload(&canard_, transfer);

  (void)canardRequestOrRespond(&canard_,
                               destination_node_id,
                               UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE,
                               static_cast<uint8_t>(UAVCAN_PROTOCOL_GETNODEINFO_ID),
                               &transfer_id,
                               priority,
                               CanardResponse,
                               payload,
                               static_cast<uint16_t>(payload_length));
}

} // namespace iFly
