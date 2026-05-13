#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterVersionInfoStream(MavlinkSend *send,
                                                    uint32_t interval_ms,
                                                    uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("AUTOPILOT_VERSION",
                              &MavlinkSend::SendVersionInfo,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendVersionInfo()
{
}

} // namespace iFly
