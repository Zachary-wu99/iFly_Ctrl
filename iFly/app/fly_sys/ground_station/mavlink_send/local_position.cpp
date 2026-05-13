#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterLocalPositionStream(MavlinkSend *send,
                                                      uint32_t interval_ms,
                                                      uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("LOCAL_POSITION_NED",
                              &MavlinkSend::SendLocalPosition,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendLocalPosition()
{
}

} // namespace iFly
