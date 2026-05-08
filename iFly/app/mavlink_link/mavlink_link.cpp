#include "mavlink_link.hpp"

#include <string.h>

#include "flight_ctrl_cli.hpp"
#include "tick.hpp"

namespace iFly {

MavlinkLink::MavlinkLink(SerialIoBase *io)
    : io_(io)
{
}

void MavlinkLink::BindIo(SerialIoBase *io)
{
  io_ = io;
}

void MavlinkLink::BindConsole(FlightCtrlCli *console)
{
  if (console_ != nullptr) {
    console_->SetOutput(nullptr, nullptr);
    console_->SetConnected(false);
  }

  console_ = console;
  if (console_ != nullptr) {
    console_->SetOutput(&MavlinkLink::ConsoleOutput, this);
    console_->SetConnected(false);
  }
}

void MavlinkLink::Poll()
{
  if ((io_ == nullptr) || !io_->IsConnected()) {
    return;
  }

  uint8_t rx_buffer[64] {};
  const uint32_t read_length = io_->Read(rx_buffer, sizeof(rx_buffer));

  for (uint32_t index = 0U; index < read_length; ++index) {
    mavlink_message_t msg {};
    if (mavlink_parse_char(MAVLINK_COMM_0, rx_buffer[index], &msg, &rx_status_) != 0U) {
      HandleMessage(msg);
    }
  }

  SendHeartbeat(tick::NowMs());
}

void MavlinkLink::HandleMessage(const mavlink_message_t &msg)
{
  switch (msg.msgid) {
    case MAVLINK_MSG_ID_SERIAL_CONTROL:
      HandleSerialControl(msg);
      break;
    
    default:
      break;
  }
}

void MavlinkLink::HandleSerialControl(const mavlink_message_t &msg)
{
  if (console_ == nullptr) {
    return;
  }

  mavlink_serial_control_t control {};
  mavlink_msg_serial_control_decode(&msg, &control);

  if (control.device != SERIAL_CONTROL_DEV_SHELL) {
    return;
  }

  if ((control.target_system != 0U) && (control.target_system != kSystemId)) {
    return;
  }

  if ((control.target_component != 0U) &&
      (control.target_component != kComponentId)) {
    return;
  }

  console_target_system_ = msg.sysid;
  console_target_component_ = msg.compid;
  console_target_valid_ = true;
  console_output_sent_ = false;
  console_->SetConnected(true);

  uint8_t count = control.count;
  if (count > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
    count = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
  }

  if (count > 0U) {
    console_->ProcessInput(control.data, count);
  }

  if (((control.flags & SERIAL_CONTROL_FLAG_RESPOND) != 0U) &&
      !console_output_sent_) {
    SendSerialControlReply(nullptr, 0U, SERIAL_CONTROL_FLAG_REPLY);
  }
}

void MavlinkLink::SendHeartbeat(uint32_t now_ms)
{
  if ((last_heartbeat_ms_ != 0U) &&
      ((now_ms - last_heartbeat_ms_) < kHeartbeatPeriodMs)) {
    return;
  }

  last_heartbeat_ms_ = now_ms;

  mavlink_message_t msg {};
  (void)mavlink_msg_heartbeat_pack(kSystemId,
                                   kComponentId,
                                   &msg,
                                   MAV_TYPE_GENERIC,
                                   MAV_AUTOPILOT_GENERIC,
                                   0U,
                                   0U,
                                   MAV_STATE_STANDBY);
  SendMessage(msg);
}

void MavlinkLink::SendMessage(const mavlink_message_t &msg)
{
  if (io_ == nullptr) {
    return;
  }

  uint8_t tx_buffer[MAVLINK_MAX_PACKET_LEN] {};
  const uint16_t tx_length = mavlink_msg_to_send_buffer(tx_buffer, &msg);
  (void)io_->Write(tx_buffer, tx_length);
}

uint32_t MavlinkLink::SendConsoleOutput(const uint8_t *data, uint32_t length)
{
  if ((data == nullptr) || (length == 0U) || !console_target_valid_) {
    return 0U;
  }

  uint32_t offset = 0U;
  while (offset < length) {
    uint32_t chunk_length = length - offset;
    if (chunk_length > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
      chunk_length = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
    }

    uint8_t flags = SERIAL_CONTROL_FLAG_REPLY;
    if ((offset + chunk_length) < length) {
      flags |= SERIAL_CONTROL_FLAG_MULTI;
    }

    SendSerialControlReply(data + offset, static_cast<uint8_t>(chunk_length),
                           flags);
    offset += chunk_length;
  }

  if (offset > 0U) {
    console_output_sent_ = true;
  }
  return offset;
}

uint32_t MavlinkLink::ConsoleOutput(void *context, const uint8_t *data,
                                    uint32_t length)
{
  MavlinkLink *link = reinterpret_cast<MavlinkLink *>(context);
  if (link == nullptr) {
    return 0U;
  }

  return link->SendConsoleOutput(data, length);
}

void MavlinkLink::SendSerialControlReply(const uint8_t *data,
                                         uint8_t len,
                                         uint8_t flags)
{
  if (len > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
    len = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
  }

  uint8_t payload[MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN] {};
  if ((data != nullptr) && (len > 0U)) {
    memcpy(payload, data, len);
  }

  mavlink_message_t msg {};
  (void)mavlink_msg_serial_control_pack(kSystemId,
                                        kComponentId,
                                        &msg,
                                        SERIAL_CONTROL_DEV_SHELL,
                                        flags,
                                        0U,
                                        0U,
                                        len,
                                        payload,
                                        console_target_system_,
                                        console_target_component_);
  SendMessage(msg);
}

} // namespace iFly
