#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterGpsStatusStream(MavlinkSend *send,
                                                  uint32_t interval_ms,
                                                  uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("GPS_RAW_INT",
                              &MavlinkSend::SendGpsStatus,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendGpsStatus()
{
}

} // namespace iFly
