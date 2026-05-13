#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterFlightHudStream(MavlinkSend *send,
                                                  uint32_t interval_ms,
                                                  uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("VFR_HUD",
                              &MavlinkSend::SendFlightHud,
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendFlightHud()
{
}

} // namespace iFly
