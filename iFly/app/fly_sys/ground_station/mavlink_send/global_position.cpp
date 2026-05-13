#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterGlobalPositionStream(MavlinkSend *send,
                                                       uint32_t interval_ms,
                                                       uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("GLOBAL_POSITION_INT",
                              &MavlinkSend::SendGlobalPosition,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendGlobalPosition()
{
}

} // namespace iFly
