#include "mavlink_cli.hpp"

#include <stdio.h>

namespace iFly {

namespace {

constexpr char kCliPrompt[] = "iFly> ";

} // namespace

MavlinkCliService::MavlinkCliService()
{
}

void MavlinkCliService::Init()
{
  shell_.ClearRegistrations();
  shell_.SetPrompt(kCliPrompt);
  shell_.ClearPassword();
  shell_.DisableActivationKey();
  active_transport_name_ = "mavlink";
  UpdateShellBanner();
  RegisterParameters();
  RegisterFunctions();
}

void MavlinkCliService::SetOutput(Shell::OutputHandler output, void *context)
{
  shell_.SetOutput(output, context);
}

void MavlinkCliService::SetConnected(bool connected)
{
  shell_.SetConnected(connected);
}

void MavlinkCliService::ProcessInput(const uint8_t *data, uint32_t length)
{
  shell_.ProcessInput(data, length);
}

void MavlinkCliService::UpdateShellBanner()
{
  const int written = snprintf(
      banner_subtitle_, sizeof(banner_subtitle_),
      "iFly flight controller CLI | transport=%s",
      (active_transport_name_ != nullptr) ? active_transport_name_ : "unbound");
  if ((written <= 0) ||
      (static_cast<uint32_t>(written) >= sizeof(banner_subtitle_))) {
    banner_subtitle_[0] = '\0';
  }

  shell_.SetBanner("iFly Flight Controller", banner_subtitle_);
}

} // namespace iFly
