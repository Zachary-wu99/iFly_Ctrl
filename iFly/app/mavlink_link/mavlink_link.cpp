#include "mavlink_link.hpp"

#include <string.h>

namespace iFly {

MavlinkLink::MavlinkLink(SerialIoBase *io)
    : io_(io)
{
}

void MavlinkLink::BindIo(SerialIoBase *io)
{
  io_ = io;
}

bool MavlinkLink::ReceiveMessage(mavlink_message_t *msg)
{
  if ((io_ == nullptr) || !io_->IsConnected() || (msg == nullptr)) {
    return false;
  }

  uint8_t rx_byte = 0U;
  while (io_->Read(&rx_byte, 1U) == 1U) {
    if (mavlink_parse_char(MAVLINK_COMM_0, rx_byte, msg, &rx_status_) != 0U) {
      return true;
    }
  }

  return false;
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

bool MavlinkLink::DecodeConsoleMessage(const mavlink_message_t &msg,
                                       mavlink_serial_control_t *control)
{
  if ((msg.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL) || (control == nullptr)) {
    return false;
  }

  mavlink_msg_serial_control_decode(&msg, control);

  if (control->device != SERIAL_CONTROL_DEV_SHELL) {
    return false;
  }

  if ((control->target_system != 0U) &&
      (control->target_system != kSystemId)) {
    return false;
  }

  if ((control->target_component != 0U) &&
      (control->target_component != kComponentId)) {
    return false;
  }

  console_target_system_ = msg.sysid;
  console_target_component_ = msg.compid;
  console_target_valid_ = true;

  if (control->count > MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN) {
    control->count = MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN;
  }

  return true;
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

    SendConsoleReply(data + offset, static_cast<uint8_t>(chunk_length), flags);
    offset += chunk_length;
  }

  return offset;
}

void MavlinkLink::SendConsoleReply(const uint8_t *data,
                                   uint8_t len,
                                   uint8_t flags)
{
  if (!console_target_valid_) {
    return;
  }

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
