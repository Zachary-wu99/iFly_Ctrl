#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterCommandResponseStream(MavlinkSend *send,
                                                        uint32_t interval_ms,
                                                        uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("COMMAND_ACK",
                              &MavlinkSend::SendCommandResponse,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendCommandResponse()
{
}

} // namespace iFly
