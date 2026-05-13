#include "mavlink_send_streams.hpp"

namespace iFly {

MavlinkSend::StreamHandle RegisterParameterValueStream(MavlinkSend *send,
                                                       uint32_t interval_ms,
                                                       uint32_t start_delay_ms)
{
  if (send == nullptr) {
    return MavlinkSend::kInvalidStreamHandle;
  }

  return send->RegisterStream("PARAM_VALUE",
                              static_cast<MavlinkSend::StreamCallback>(
                                  &MavlinkSend::SendParameterValue),
                              interval_ms,
                              start_delay_ms);
}

void MavlinkSend::SendParameterValue()
{
}

void MavlinkSend::SendParameterValue(const MavlinkParameterValue &parameter)
{
  mavlink_message_t msg {};
  (void)mavlink_msg_param_value_pack(kSystemId,
                                     kComponentId,
                                     &msg,
                                     parameter.name,
                                     parameter.value,
                                     parameter.type,
                                     parameter.count,
                                     parameter.index);
  SendMessage(msg);
}

} // namespace iFly
